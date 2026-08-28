// Phase 1 状态同步 + AOI 集成测试:
// 互见 / 移动广播 / 视野裁剪 / 瞬移拦截 / 超速拦截 / 时间戳校验
// 依赖服务器 config.yaml 的 world 段配置 (坐标单位 = UE 厘米 cm):
// grid_size=2000, view_radius_cells=1, max_move_speed=1200, speed_tolerance=1.5,
// teleport_threshold=10000, max_timestamp_skew_ms=5000
// 注意: 下述坐标断言基于"spawn=0 + 米"的旧语义, 现 config 已统一为 cm,
// 且出生点 = LP_CartoonCity PlayerStart (-5380,-2200,82); 跑测前需清测试账号存档
// 并将坐标按 cm 语义换算 (5->500, 45->4500, 1000->100000, 40->4000).
// 新协议: unary Login 拿会话令牌 -> Bearer 打开 World 双向流 -> 实时玩法经 World 流收发
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "game.pb.h"
#include "game.grpc.pb.h"

namespace {

constexpr const char* kTarget = "127.0.0.1:50061";
constexpr const char* kPassword = "test-pass-123";

int g_failed = 0;

void Check(bool cond, const std::string& name) {
    std::cout << (cond ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if (!cond) ++g_failed;
}

bool Near(float a, float b, float eps = 0.01f) {
    return std::abs(a - b) <= eps;
}

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::unique_ptr<game::GameService::Stub> MakeStub() {
    return game::GameService::NewStub(
        grpc::CreateChannel(kTarget, grpc::InsecureChannelCredentials()));
}

using WorldStream = grpc::ClientReaderWriter<game::ClientWorld, game::ServerWorld>;

// 一次会话: unary Login 签发的令牌 + 已挂载的 World 流
struct Session {
    std::string token;
    uint64_t player_id = 0;
    float spawn_x = 0.0f, spawn_y = 0.0f, spawn_z = 0.0f, spawn_yaw = 0.0f;
    grpc::ClientContext ctx;
    std::unique_ptr<WorldStream> stream;
};

// unary Login -> 携带 Bearer 令牌打开 World 双向流; 失败直接退出.
// Session 含非可移动成员 (ClientContext/unique_ptr stream), 采用 out-param 就地构造.
void LoginAttach(const std::string& account, Session* out) {
    auto login_stub = MakeStub();
    grpc::ClientContext lctx;
    game::LoginRequest req;
    req.set_account(account);
    req.set_password(kPassword);
    game::LoginResponse resp;
    grpc::Status st = login_stub->Login(&lctx, req, &resp);
    if (!st.ok() || !resp.success()) {
        std::cerr << "[FATAL] login rejected: " << st.error_message() << std::endl;
        std::exit(1);
    }
    out->token = resp.session_token();
    out->player_id = resp.player_id();
    out->spawn_x = resp.spawn_x();
    out->spawn_y = resp.spawn_y();
    out->spawn_z = resp.spawn_z();
    out->spawn_yaw = resp.spawn_yaw();
    // World 流会话令牌由首个 READ 帧的消息体携带兑换 (gRPC 双向流 initial metadata 不可达).
    // 建流后立即发送带 token 的心跳, 保证流完成兑换上线.
    out->stream = MakeStub()->World(&out->ctx);
    game::ClientWorld token_frame;
    token_frame.set_sequence(1);
    if (!out->token.empty()) token_frame.set_session_token(out->token);
    token_frame.mutable_heartbeat()->set_client_timestamp(0);
    const bool wrote = out->stream->Write(token_frame);
    if (!wrote) {
        std::cerr << "[FATAL] world attach (token heartbeat) write failed" << std::endl;
        std::exit(1);
    }
}

game::ClientWorld MakeMove(uint64_t seq, float x, float y, float z, float yaw, int64_t ts) {
    game::ClientWorld msg;
    msg.set_sequence(seq);
    auto* m = msg.mutable_movement();
    m->set_x(x);
    m->set_y(y);
    m->set_z(z);
    m->set_yaw(yaw);
    m->set_client_timestamp(ts);
    return msg;
}

// 后台读取线程: 持续 Read 并缓存到队列, 主线程按谓词消费 (处理服务器异步推送)
class AsyncReader {
public:
    explicit AsyncReader(WorldStream* s) : stream_(s) {
        thread_ = std::thread([this] {
            game::ServerWorld msg;
            while (!stop_.load() && stream_->Read(&msg)) {
                std::lock_guard<std::mutex> lock(mtx_);
                queue_.push_back(std::move(msg));
            }
            finished_.store(true);
        });
    }
    ~AsyncReader() {
        stop_.store(true);
        if (thread_.joinable()) thread_.join();
    }

    template <typename Pred>
    bool WaitFor(Pred pred, int timeout_ms, game::ServerWorld* out) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                for (auto it = queue_.begin(); it != queue_.end(); ++it) {
                    if (pred(*it)) {
                        *out = *it;
                        queue_.erase(it);
                        return true;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    template <typename Pred>
    bool WaitAbsent(Pred pred, int window_ms) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(window_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                for (const auto& m : queue_) {
                    if (pred(m)) return false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }

    bool WaitClosed(int timeout_ms) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (finished_.load()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

private:
    WorldStream* stream_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> finished_{false};
    std::thread thread_;
    std::mutex mtx_;
    std::deque<game::ServerWorld> queue_;
};

// unary Logout (Bearer) 触发服务器关闭该账号 World 流, 收尾客户端 RPC
void LogoutAndClose(Session& s, AsyncReader& reader) {
    auto stub = MakeStub();
    grpc::ClientContext octx;
    octx.AddMetadata("authorization", "Bearer " + s.token);
    game::LogoutRequest req;
    game::LogoutResponse resp;
    stub->Logout(&octx, req, &resp);
    s.stream->WritesDone();
    reader.WaitClosed(3000);
    s.stream->Finish();
}

void ScenarioAoi() {
    std::cout << "\n== Phase 1: 状态同步 + AOI ==" << std::endl;

    // ---- 1. 互见 ----
    std::cout << "\n-- 1. 互见 (mutual visibility) --" << std::endl;
    Session a;
    LoginAttach("aoiA", &a);
    AsyncReader ra(a.stream.get());
    game::ServerWorld resp;

    Check(Near(a.spawn_x, 0.0f) && Near(a.spawn_y, 0.0f) &&
              Near(a.spawn_z, 0.0f) && Near(a.spawn_yaw, 0.0f),
          "A spawn at authoritative origin");
    const uint64_t pa = a.player_id;

    Session b;
    LoginAttach("aoiB", &b);
    AsyncReader rb(b.stream.get());
    const uint64_t pb = b.player_id;
    Check(pa != pb, "distinct player ids");

    Check(ra.WaitFor([pb](const game::ServerWorld& m) {
              return m.has_player_enter() && m.player_enter().player_id() == pb;
          }, 3000, &resp),
          "A sees B enter");
    Check(rb.WaitFor([pa](const game::ServerWorld& m) {
              return m.has_player_enter() && m.player_enter().player_id() == pa;
          }, 3000, &resp),
          "B sees A enter");

    // ---- 2. 移动广播 ----
    std::cout << "\n-- 2. 移动广播 (movement broadcast) --" << std::endl;
    const int64_t ts2 = NowMs();
    Check(a.stream->Write(MakeMove(2, 5.0f, 0.0f, 0.0f, 0.0f, ts2)), "A send move to (5,0,0)");
    Check(rb.WaitFor([pa](const game::ServerWorld& m) {
              return m.has_player_move() && m.player_move().player_id() == pa &&
                     Near(m.player_move().x(), 5.0f);
          }, 3000, &resp),
          "B receives A move broadcast");

    // ---- 3. 瞬移拦截 ----
    std::cout << "\n-- 3. 瞬移拦截 (teleport rejection) --" << std::endl;
    Check(a.stream->Write(MakeMove(3, 1000.0f, 0.0f, 0.0f, 0.0f, NowMs())),
          "A send teleport to (1000,0,0)");
    Check(ra.WaitFor([](const game::ServerWorld& m) { return m.has_position_correction(); },
                     3000, &resp) &&
              Near(resp.position_correction().x(), 5.0f),
          "A receives correction back to authoritative (5,0,0)");
    Check(rb.WaitAbsent([pa](const game::ServerWorld& m) {
              return m.has_player_move() && m.player_move().player_id() == pa &&
                     Near(m.player_move().x(), 1000.0f);
          }, 300),
          "B receives no broadcast of rejected teleport");

    // ---- 4. 超速拦截 ----
    std::cout << "\n-- 4. 超速拦截 (overspeed rejection) --" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // 拉大上报间隔使速度校验生效
    Check(a.stream->Write(MakeMove(4, 45.0f, 0.0f, 0.0f, 0.0f, NowMs())),
          "A send overspeed move to (45,0,0) after 1s gap");
    Check(ra.WaitFor([](const game::ServerWorld& m) { return m.has_position_correction(); },
                     3000, &resp) &&
              Near(resp.position_correction().x(), 5.0f),
          "A receives correction (speed limit exceeded)");
    Check(rb.WaitAbsent([pa](const game::ServerWorld& m) {
              return m.has_player_move() && m.player_move().player_id() == pa &&
                     Near(m.player_move().x(), 45.0f);
          }, 300),
          "B receives no broadcast of rejected overspeed");

    // ---- 5. 时间戳校验 ----
    std::cout << "\n-- 5. 时间戳校验 (timestamp validation) --" << std::endl;
    Check(a.stream->Write(MakeMove(5, 10.0f, 0.0f, 0.0f, 0.0f, NowMs() + 60000)),
          "A send move with future timestamp (60s skew)");
    Check(ra.WaitFor([](const game::ServerWorld& m) { return m.has_position_correction(); },
                     3000, &resp) &&
              Near(resp.position_correction().x(), 5.0f),
          "A receives correction (clock skew)");
    Check(a.stream->Write(MakeMove(6, 10.0f, 0.0f, 0.0f, 0.0f, ts2 - 1000)),
          "A send move with out-of-order timestamp");
    Check(ra.WaitFor([](const game::ServerWorld& m) { return m.has_position_correction(); },
                     3000, &resp) &&
              Near(resp.position_correction().x(), 5.0f),
          "A receives correction (out-of-order ts)");

    // ---- 6. 视野裁剪 ----
    std::cout << "\n-- 6. 视野裁剪 (view culling) --" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));  // 2.5s 间隔, 允许 35m 单次位移
    Check(a.stream->Write(MakeMove(7, 40.0f, 0.0f, 0.0f, 0.0f, NowMs())),
          "A move to (40,0,0) leaving B's 9-grid view");
    Check(rb.WaitFor([pa](const game::ServerWorld& m) {
              return m.has_player_leave() && m.player_leave().player_id() == pa;
          }, 3000, &resp),
          "B receives A leave (view culling)");
    Check(ra.WaitFor([pb](const game::ServerWorld& m) {
              return m.has_player_leave() && m.player_leave().player_id() == pb;
          }, 3000, &resp),
          "A receives B leave (mutual culling)");

    // ---- 清理 ----
    LogoutAndClose(a, ra);
    LogoutAndClose(b, rb);
}

} // namespace

int main() {
    ScenarioAoi();
    std::cout << "\n==== " << (g_failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " ====" << std::endl;
    return g_failed == 0 ? 0 : 1;
}