#include "game/net/dialogue_revoker.h"

#include <chrono>
#include <condition_variable>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <queue>
#include <set>
#include <thread>

#include <spdlog/spdlog.h>

#include "admin.grpc.pb.h"

namespace game {
namespace net {

namespace {
// 控制面单次通知上限: 发送线程独立于玩法线程池, VHServer 不可达时最多阻塞至此
constexpr int64_t kRevokeTimeoutMs = 1500;
} // namespace

struct DialogueRevoker::Impl {
    std::string endpoint;
    std::string admin_secret;
    std::unique_ptr<vhadmin::AdminService::Stub> stub;

    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::string> pending;
    std::set<std::string> in_queue;   // 同账号在途去重: 一次会话终结只需一份通知

    std::thread worker;
    bool running = false;
};

DialogueRevoker::DialogueRevoker(const std::string& endpoint, const std::string& admin_secret)
    : pimpl_(std::make_unique<Impl>()) {
    pimpl_->endpoint = endpoint;
    pimpl_->admin_secret = admin_secret;
}

DialogueRevoker::~DialogueRevoker() {
    Stop();
}

void DialogueRevoker::Start() {
    auto channel = grpc::CreateChannel(pimpl_->endpoint, grpc::InsecureChannelCredentials());
    pimpl_->stub = vhadmin::AdminService::NewStub(channel);

    pimpl_->running = true;
    pimpl_->worker = std::thread([this] { SendLoop(); });
    spdlog::info("[DialogueRevoker] Control-plane link to VHServer [endpoint={}]", pimpl_->endpoint);
}

void DialogueRevoker::Revoke(const std::string& account) {
    if (account.empty()) return;
    {
        std::lock_guard<std::mutex> lock(pimpl_->mtx);
        if (!pimpl_->running) return;
        // 在途去重: 会话关闭路径高频调用, 同账号重复通知合并为一份
        if (!pimpl_->in_queue.insert(account).second) return;
        pimpl_->pending.push(account);
    }
    pimpl_->cv.notify_one();
}

void DialogueRevoker::Stop() {
    {
        std::lock_guard<std::mutex> lock(pimpl_->mtx);
        if (!pimpl_->running) return;
        pimpl_->running = false;
    }
    pimpl_->cv.notify_all();
    if (pimpl_->worker.joinable()) pimpl_->worker.join();
    pimpl_->stub.reset();
}

void DialogueRevoker::SendLoop() {
    while (true) {
        std::string account;
        {
            std::unique_lock<std::mutex> lock(pimpl_->mtx);
            pimpl_->cv.wait(lock, [this] {
                return !pimpl_->pending.empty() || !pimpl_->running;
            });

            if (pimpl_->pending.empty()) {
                // 停止前排空在途通知, 然后退出
                if (!pimpl_->running) break;
                continue;
            }
            account = std::move(pimpl_->pending.front());
            pimpl_->pending.pop();
            pimpl_->in_queue.erase(account);
        }

        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRevokeTimeoutMs));
        ctx.AddMetadata("authorization", "Bearer " + pimpl_->admin_secret);

        vhadmin::RevokeDialogueRequest req;
        req.set_account(account);
        vhadmin::RevokeDialogueResponse resp;
        const grpc::Status st = pimpl_->stub->RevokeDialogue(&ctx, req, &resp);
        if (st.ok()) {
            spdlog::info("[DialogueRevoker] Revoked [account={}, streams={}]",
                         account, resp.revoked());
        } else {
            // 尽力而为: 送达失败不重试不阻塞, 由数据面票据 TTL 兜底
            spdlog::warn("[DialogueRevoker] Notify failed [account={}, code={}, msg={}]",
                         account, static_cast<int>(st.error_code()), st.error_message());
        }
    }
}

} // namespace net
} // namespace game
