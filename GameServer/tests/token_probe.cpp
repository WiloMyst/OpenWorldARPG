// 跨服联调探针: 登录 GameServer -> 申请对话票据 -> 票据打印到 stdout
// 供 VHServer 端脚本 (test_cross_server.py) 验证信令面签发 / 数据面校验闭环。
// 用法: ./token_probe [account] [npc_id] [server_addr]
// 成功输出单行票据 (account.npc_id.expires_at.hmac), 失败输出 ERROR: reason 并退出 1

#include <cstdint>
#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>
#include "game.pb.h"
#include "game.grpc.pb.h"

int main(int argc, char* argv[]) {
    const std::string account = argc > 1 ? argv[1] : "dlgprobe";
    const int32_t npc_id = argc > 2 ? std::stoi(argv[2]) : 42;
    const std::string target = argc > 3 ? argv[3] : "127.0.0.1:50061";

    auto stub = game::GameService::NewStub(
        grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
    grpc::ClientContext ctx;
    auto stream = stub->GameChannel(&ctx);

    game::ClientMessage login;
    login.set_sequence(1);
    login.mutable_login()->set_account(account);
    login.mutable_login()->set_token("dev-token-2026");
    if (!stream->Write(login)) {
        std::cout << "ERROR: send login failed" << std::endl;
        return 1;
    }

    game::ServerMessage resp;
    if (!stream->Read(&resp) || !resp.has_login() || !resp.login().success()) {
        std::cout << "ERROR: login rejected" << std::endl;
        return 1;
    }

    game::ClientMessage auth;
    auth.set_sequence(2);
    auth.mutable_dialogue_auth()->set_npc_id(npc_id);
    if (!stream->Write(auth) || !stream->Read(&resp) ||
        !resp.has_dialogue_auth_result()) {
        std::cout << "ERROR: no dialogue_auth_result" << std::endl;
        return 1;
    }

    const auto& result = resp.dialogue_auth_result();
    if (!result.ok()) {
        std::cout << "ERROR: " << result.reason() << std::endl;
        return 1;
    }

    std::cout << result.dialogue_token() << std::endl;
    stream->WritesDone();
    return 0;
}
