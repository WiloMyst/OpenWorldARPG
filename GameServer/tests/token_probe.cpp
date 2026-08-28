// 跨服联调探针: unary Login GameServer -> 以会话令牌鉴权申请对话票据 -> 票据打印到 stdout
// 供 VHServer 端脚本 (test_cross_server.py) 验证信令面签发 / 数据面校验闭环。
// 用法: ./token_probe [account] [npc_id] [server_addr]
// 成功输出单行票据 (account.npc_id.expires_at.hmac), 失败输出 ERROR: reason 并退出 1

#include <cstdlib>
#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>
#include "game.pb.h"
#include "game.grpc.pb.h"

int main(int argc, char* argv[]) {
    const std::string account = argc > 1 ? argv[1] : "dlgprobe";
    const int32_t npc_id = argc > 2 ? std::stoi(argv[2]) : 42;
    const std::string target = argc > 3 ? argv[3] : "127.0.0.1:50061";
    const std::string kPassword = "test-pass-123";

    auto stub = game::GameService::NewStub(
        grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));

    // 1. unary Login: 建档 + 签发会话令牌 (成功即登出, 探针只取票据能力)
    grpc::ClientContext lctx;
    game::LoginRequest lreq;
    lreq.set_account(account);
    lreq.set_password(kPassword);
    game::LoginResponse lresp;
    grpc::Status lst = stub->Login(&lctx, lreq, &lresp);
    if (!lst.ok()) {
        std::cout << "ERROR: login rpc " << lst.error_message() << std::endl;
        return 1;
    }
    if (!lresp.success()) {
        std::cout << "ERROR: login rejected: " << lresp.error_msg() << std::endl;
        return 1;
    }
    const std::string session_token = lresp.session_token();

    // 2. 以 Bearer 会话令牌鉴权申请对话票据 (信令面)
    grpc::ClientContext actx;
    actx.AddMetadata("authorization", "Bearer " + session_token);
    game::DialogueAuthRequest areq;
    areq.set_npc_id(npc_id);
    game::DialogueAuthResult aresp;
    grpc::Status ast = stub->AuthenticateDialogue(&actx, areq, &aresp);
    if (!ast.ok()) {
        std::cout << "ERROR: dialogue auth rpc " << ast.error_message() << std::endl;
        return 1;
    }
    if (!aresp.ok()) {
        std::cout << "ERROR: " << aresp.reason() << std::endl;
        return 1;
    }

    std::cout << aresp.dialogue_token() << std::endl;
    return 0;
}