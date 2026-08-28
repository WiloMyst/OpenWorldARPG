#pragma once
#include <memory>
#include <string>

namespace game {
namespace net {

// VHServer 对话流吊销通知器 (GameServer -> VHServer 控制面客户端):
// 玩家会话在 GameServer 侧终结 (登出/踢出/断线/心跳超时) 时, 通知 VHServer 吊销该账号
// 已建立的对话流. 数据面票据仅在流建立时校验, 已建流的终止只能由控制面主动下发,
// 否则被踢玩家可凭未过期票据继续推理 (残留窗口 = 票据 TTL).
//
// 送达语义: 尽力而为 (独立发送线程消费 + 同账号在途去重), 不阻塞会话关闭路径;
// VHServer 不可达时仅记日志不重试, 由票据 TTL 兜底.
class DialogueRevoker {
public:
    DialogueRevoker(const std::string& endpoint, const std::string& admin_secret);
    ~DialogueRevoker();
    DialogueRevoker(const DialogueRevoker&) = delete;
    DialogueRevoker& operator=(const DialogueRevoker&) = delete;

    void Start();
    // 停止接收新通知并排空在途通知后退出
    void Stop();

    // 非阻塞: 账号入队 (在途去重), 由发送线程消费; 已停止时静默丢弃
    void Revoke(const std::string& account);

private:
    void SendLoop();

    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace net
} // namespace game
