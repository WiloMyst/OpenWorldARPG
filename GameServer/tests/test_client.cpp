// M1+M2 集成测试: 会话/鉴权/存档/踢出 + 权威背包全场景 + MySQL 持久化
// 运行前由 run_tests.sh 清理 m2inv 账号数据, 保证可重复
#include <cstdint>
#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>
#include "game.pb.h"
#include "game.grpc.pb.h"

namespace {

constexpr const char* kTarget = "127.0.0.1:50061";
constexpr const char* kToken = "dev-token-2026";

int g_failed = 0;

void Check(bool cond, const std::string& name) {
    std::cout << (cond ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if (!cond) ++g_failed;
}

using Stream = grpc::ClientReaderWriter<game::ClientMessage, game::ServerMessage>;

std::unique_ptr<game::GameService::Stub> MakeStub() {
    return game::GameService::NewStub(
        grpc::CreateChannel(kTarget, grpc::InsecureChannelCredentials()));
}

game::ClientMessage MakeLogin(uint64_t seq, const std::string& account, const std::string& token) {
    game::ClientMessage msg;
    msg.set_sequence(seq);
    msg.mutable_login()->set_account(account);
    msg.mutable_login()->set_token(token);
    return msg;
}

// 发送一条消息并读取一条响应 (请求-响应模式)
bool Call(Stream& stream, uint64_t seq, const game::ClientMessage& msg, game::ServerMessage* out) {
    if (!stream.Write(msg)) return false;
    return stream.Read(out);
}

const game::ItemInstance* FindItem(const game::InventoryOpResponse& resp, int32_t item_id) {
    for (const auto& item : resp.items()) {
        if (item.item_id() == item_id) return &item;
    }
    return nullptr;
}

// 背包操作 helper: 返回响应并校验 ack
bool InvOp(Stream& stream, uint64_t seq, game::InventoryOpRequest* op, game::ServerMessage* out) {
    game::ClientMessage msg;
    msg.set_sequence(seq);
    *msg.mutable_inventory_op() = *op;
    return Call(stream, seq, msg, out) && out->ack_sequence() == seq;
}

// ---- M1: 会话/鉴权 ----

void ScenarioNormal() {
    std::cout << "\n== Scenario 1: login / heartbeat / save / logout ==" << std::endl;
    auto stub = MakeStub();
    grpc::ClientContext ctx;
    auto stream = stub->GameChannel(&ctx);
    game::ServerMessage resp;

    Check(stream->Write(MakeLogin(1, "alice", kToken)), "send login");
    Check(stream->Read(&resp) && resp.has_login() && resp.login().success() &&
              resp.login().player_data().account() == "alice",
          "login ok with player data");
    Check(resp.ack_sequence() == 1, "ack sequence echoed");

    game::ClientMessage hb;
    hb.set_sequence(2);
    hb.mutable_heartbeat()->set_client_timestamp(0);
    Check(stream->Write(hb) && stream->Read(&resp) && resp.has_heartbeat() &&
              resp.heartbeat().server_timestamp() > 0,
          "heartbeat acked");

    game::ClientMessage save;
    save.set_sequence(3);
    save.mutable_save_data()->mutable_player_data()->set_account("alice");
    save.mutable_save_data()->mutable_player_data()->set_level(5);
    save.mutable_save_data()->mutable_player_data()->set_exp(1200);
    Check(stream->Write(save) && stream->Read(&resp) && resp.has_save_data() &&
              resp.save_data().success(),
          "save data ok");

    game::ClientMessage logout;
    logout.set_sequence(4);
    logout.mutable_logout();
    Check(stream->Write(logout) && stream->WritesDone(), "send logout + half close");
    Check(!stream->Read(&resp), "stream closed by server after logout");
}

void ScenarioDuplicateLogin() {
    std::cout << "\n== Scenario 2: duplicate login kicks old connection ==" << std::endl;
    auto stub = MakeStub();

    grpc::ClientContext ctx_a;
    auto stream_a = stub->GameChannel(&ctx_a);
    game::ServerMessage resp;
    Check(stream_a->Write(MakeLogin(1, "bob", kToken)) &&
              stream_a->Read(&resp) && resp.login().success(),
          "bob login on connection A");

    grpc::ClientContext ctx_b;
    auto stream_b = stub->GameChannel(&ctx_b);
    Check(stream_b->Write(MakeLogin(1, "bob", kToken)) &&
              stream_b->Read(&resp) && resp.login().success(),
          "bob login again on connection B");

    Check(stream_a->Read(&resp) && resp.has_kick() &&
              resp.kick().reason() == "duplicate_login",
          "connection A receives duplicate_login kick");
    Check(!stream_a->Read(&resp), "connection A stream closed");

    game::ClientMessage logout;
    logout.mutable_logout();
    stream_b->Write(logout);
    stream_b->WritesDone();
    stream_b->Finish();
}

void ScenarioUnauthenticated() {
    std::cout << "\n== Scenario 3: heartbeat before login rejected ==" << std::endl;
    auto stub = MakeStub();
    grpc::ClientContext ctx;
    auto stream = stub->GameChannel(&ctx);

    game::ClientMessage hb;
    hb.set_sequence(7);
    hb.mutable_heartbeat()->set_client_timestamp(0);
    game::ServerMessage resp;
    Check(stream->Write(hb) && stream->Read(&resp) && resp.has_error() &&
              resp.error().code() == 401,
          "unauthenticated heartbeat rejected with 401");
    stream->WritesDone();
    stream->Finish();
}

void ScenarioBadToken() {
    std::cout << "\n== Scenario 4: bad token rejected ==" << std::endl;
    auto stub = MakeStub();
    grpc::ClientContext ctx;
    auto stream = stub->GameChannel(&ctx);

    game::ServerMessage resp;
    Check(stream->Write(MakeLogin(1, "carol", "wrong-token")) &&
              stream->Read(&resp) && resp.has_login() && !resp.login().success(),
          "login with bad token rejected");
    stream->WritesDone();
    stream->Finish();
}

// ---- M2: 权威背包 ----

void ScenarioInventory() {
    std::cout << "\n== Scenario 5: authoritative inventory ops ==" << std::endl;
    auto stub = MakeStub();
    grpc::ClientContext ctx;
    auto stream = stub->GameChannel(&ctx);
    game::ServerMessage resp;

    // 登录: run_tests.sh 已清库, 背包应为空
    Check(stream->Write(MakeLogin(1, "m2inv", kToken)) &&
              stream->Read(&resp) && resp.login().success() &&
              resp.login().inventory_size() == 0,
          "login with empty inventory");

    uint64_t seq = 10;
    game::InventoryOpRequest op;
    const game::InventoryOpResponse* inv = nullptr;

    // 1. 未知物品拒绝
    op.mutable_add()->set_item_id(9999);
    op.mutable_add()->set_amount(1);
    Check(InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success() == false,
          "add unknown item rejected");

    // 2. 堆叠物品: 150 个材料 (max_stack 9999 单槽)
    op.mutable_add()->set_item_id(1001);
    op.mutable_add()->set_amount(150);
    Check(InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success() &&
              FindItem(resp.inventory_op(), 1001) &&
              FindItem(resp.inventory_op(), 1001)->count() == 150,
          "add 150x material in one stack");

    // 3. 堆叠溢出: 1002 max_stack=99, 已有 99 再加 10 → 两个槽位
    op.mutable_add()->set_item_id(1002);
    op.mutable_add()->set_amount(99);
    Check(InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success(),
          "add 99x material (full stack)");
    op.mutable_add()->set_item_id(1002);
    op.mutable_add()->set_amount(10);
    int32_t count_1002_slots = 0;
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        for (const auto& item : resp.inventory_op().items()) {
            if (item.item_id() == 1002) ++count_1002_slots;
        }
    }
    Check(count_1002_slots == 2, "stack overflow opens second slot");

    // 4. 不可堆叠武器 amount=2 拒绝
    op.mutable_add()->set_item_id(2001);
    op.mutable_add()->set_amount(2);
    Check(InvOp(*stream, seq++, &op, &resp) && !resp.inventory_op().success(),
          "non-stackable add amount!=1 rejected");

    // 5. 武器发放成长数据
    std::string w1_guid, w2_guid;
    op.mutable_add()->set_item_id(2001);
    op.mutable_add()->set_amount(1);
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        const auto* item = FindItem(resp.inventory_op(), 2001);
        if (item) w1_guid = item->item_guid();
    }
    op.mutable_add()->set_item_id(2002);
    op.mutable_add()->set_amount(1);
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        const auto* item = FindItem(resp.inventory_op(), 2002);
        if (item) w2_guid = item->item_guid();
    }
    Check(!w1_guid.empty() && !w2_guid.empty(), "weapons added with guids");

    // 6. 圣遗物按配置生成词条
    std::string a1_guid, a2_guid;
    op.mutable_add()->set_item_id(3001);
    op.mutable_add()->set_amount(1);
    bool artifact_ok = false;
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        const auto* item = FindItem(resp.inventory_op(), 3001);
        if (item && item->has_artifact_data()) {
            a1_guid = item->item_guid();
            artifact_ok = item->artifact_data().slot() == "flower" &&
                          item->artifact_data().main_stat() == "hp_flat" &&
                          item->artifact_data().main_stat_value() > 0;
        }
    }
    Check(artifact_ok, "artifact instance generated from config");

    op.mutable_add()->set_item_id(3003);
    op.mutable_add()->set_amount(1);
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        const auto* item = FindItem(resp.inventory_op(), 3003);
        if (item) a2_guid = item->item_guid();
    }
    Check(!a2_guid.empty(), "second flower artifact added");

    // 7. 武器装备 + 互斥
    op.mutable_equip()->set_item_guid(w1_guid);
    op.mutable_equip()->set_character_id(0);
    Check(InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success(),
          "equip weapon 1 on char 0");

    op.mutable_equip()->set_item_guid(w2_guid);
    op.mutable_equip()->set_character_id(0);
    bool w2_equipped = false, w1_unequipped = false;
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        for (const auto& item : resp.inventory_op().items()) {
            if (item.item_guid() == w1_guid && item.equipped_character_id() == -1) w1_unequipped = true;
            if (item.item_guid() == w2_guid && item.equipped_character_id() == 0) w2_equipped = true;
        }
    }
    Check(w1_unequipped && w2_equipped, "weapon exclusivity: old weapon unequipped");

    // 8. 圣遗物部位互斥
    op.mutable_equip()->set_item_guid(a1_guid);
    op.mutable_equip()->set_character_id(0);
    Check(InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success(),
          "equip flower artifact 1");

    op.mutable_equip()->set_item_guid(a2_guid);
    op.mutable_equip()->set_character_id(0);
    bool a1_replaced = false;
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        for (const auto& item : resp.inventory_op().items()) {
            if (item.item_guid() == a1_guid && item.equipped_character_id() == -1) a1_replaced = true;
        }
    }
    Check(a1_replaced, "artifact slot exclusivity: same slot replaced");

    // 9. 已装备物品不可丢弃
    op.mutable_remove()->set_item_guid(w2_guid);
    op.mutable_remove()->set_amount(1);
    Check(InvOp(*stream, seq++, &op, &resp) && !resp.inventory_op().success(),
          "remove equipped item rejected");

    // 10. 卸下 + 丢弃
    op.mutable_unequip()->set_item_guid(w2_guid);
    Check(InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success(), "unequip weapon 2");
    op.mutable_remove()->set_item_guid(w2_guid);
    op.mutable_remove()->set_amount(1);
    bool w2_gone = false;
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        w2_gone = FindItem(resp.inventory_op(), 2002) == nullptr;
    }
    Check(w2_gone, "unequipped weapon removed");

    // 11. 不可使用物品拒绝 (材料 use_target=none)
    {
        const game::ItemInstance* mat = FindItem(resp.inventory_op(), 1001);
        Check(mat != nullptr, "material present for use test");
        if (mat) {
            op.mutable_use()->set_item_guid(mat->item_guid());
            op.mutable_use()->set_amount(1);
            Check(InvOp(*stream, seq++, &op, &resp) && !resp.inventory_op().success(),
                  "use non-usable item rejected");
        }
    }

    // 12. 食物需要目标角色
    std::string food_guid;
    op.mutable_add()->set_item_id(4001);
    op.mutable_add()->set_amount(5);
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        const auto* item = FindItem(resp.inventory_op(), 4001);
        if (item) food_guid = item->item_guid();
    }
    op.mutable_use()->set_item_guid(food_guid);
    op.mutable_use()->set_target_character_id(-1);
    op.mutable_use()->set_amount(1);
    Check(InvOp(*stream, seq++, &op, &resp) && !resp.inventory_op().success(),
          "use food without target rejected");

    op.mutable_use()->set_item_guid(food_guid);
    op.mutable_use()->set_target_character_id(0);
    op.mutable_use()->set_amount(2);
    bool food_consumed = false;
    if (InvOp(*stream, seq++, &op, &resp) && resp.inventory_op().success()) {
        const auto* item = FindItem(resp.inventory_op(), 4001);
        food_consumed = item && item->count() == 3;
    }
    Check(food_consumed, "use food with target: count 5 -> 3");

    // 记录最终状态用于持久化校验
    int32_t final_1001 = 0, final_4001 = 0;
    for (const auto& item : resp.inventory_op().items()) {
        if (item.item_id() == 1001) final_1001 = item.count();
        if (item.item_id() == 4001) final_4001 = item.count();
    }

    // 登出
    game::ClientMessage logout;
    logout.set_sequence(seq++);
    logout.mutable_logout();
    stream->Write(logout);
    stream->WritesDone();
    stream->Finish();

    // ---- Scenario 6: 重登持久化 ----
    std::cout << "\n== Scenario 6: persistence across relogin ==" << std::endl;
    grpc::ClientContext ctx2;
    auto stream2 = stub->GameChannel(&ctx2);
    Check(stream2->Write(MakeLogin(1, "m2inv", kToken)) &&
              stream2->Read(&resp) && resp.login().success(),
          "relogin ok");

    int32_t re_1001 = 0, re_4001 = 0;
    bool re_weapon = false, re_artifact_equipped = false, re_ext_ok = false;
    for (const auto& item : resp.login().inventory()) {
        if (item.item_id() == 1001) re_1001 = item.count();
        if (item.item_id() == 4001) re_4001 = item.count();
        if (item.item_id() == 2001) re_weapon = item.has_weapon_data();
        if (item.item_id() == 3003 && item.equipped_character_id() == 0) {
            re_artifact_equipped = item.has_artifact_data() &&
                                   item.artifact_data().slot() == "flower" &&
                                   item.artifact_data().main_stat_value() > 0;
        }
    }
    Check(re_1001 == final_1001, "material count persisted");
    Check(re_4001 == final_4001, "food count persisted");
    Check(re_weapon, "weapon ext data persisted");
    Check(re_artifact_equipped, "artifact slot/equip state persisted");

    // 清理: 登出
    game::ClientMessage logout2;
    logout2.set_sequence(2);
    logout2.mutable_logout();
    stream2->Write(logout2);
    stream2->WritesDone();
    stream2->Finish();
}

// ---- M4: 对话授权 (信令面) ----

void ScenarioDialogueAuth() {
    std::cout << "\n== Scenario 7: dialogue auth token ==" << std::endl;
    auto stub = MakeStub();
    grpc::ClientContext ctx;
    auto stream = stub->GameChannel(&ctx);
    game::ServerMessage resp;

    // 未登录先申请: 须被 401 拒绝
    game::ClientMessage auth;
    auth.set_sequence(1);
    auth.mutable_dialogue_auth()->set_npc_id(42);
    Check(stream->Write(auth) && stream->Read(&resp) && resp.has_error() &&
              resp.error().code() == 401,
          "dialogue auth before login rejected with 401");

    // 登录后再申请
    Check(stream->Write(MakeLogin(2, "dlguser", kToken)) &&
              stream->Read(&resp) && resp.login().success(),
          "dlguser login");

    game::ClientMessage auth2;
    auth2.set_sequence(3);
    auth2.mutable_dialogue_auth()->set_npc_id(42);
    const bool has_token = stream->Write(auth2) && stream->Read(&resp) &&
                           resp.has_dialogue_auth_result();
    Check(has_token, "dialogue auth result received");

    if (has_token) {
        const game::DialogueAuthResult& r = resp.dialogue_auth_result();
        Check(r.ok() && !r.dialogue_token().empty() && r.expires_at() > 0,
              "token issued with expiry");

        // 票据结构校验: account.npc_id.expires_at.hmac 四段
        size_t p1 = r.dialogue_token().find('.');
        size_t p2 = r.dialogue_token().find('.', p1 + 1);
        size_t p3 = r.dialogue_token().find('.', p2 + 1);
        Check(p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos &&
                  r.dialogue_token().substr(0, p1) == "dlguser" &&
                  r.dialogue_token().substr(p1 + 1, p2 - p1 - 1) == "42" &&
                  r.dialogue_token().substr(p2 + 1, p3 - p2 - 1) == std::to_string(r.expires_at()) &&
                  r.dialogue_token().substr(p3 + 1).size() == 64,
              "token payload binds account/npc_id/expiry + 64-char hmac");

        // 频控: 1 秒内立即再申请须被拒
        game::ClientMessage auth3;
        auth3.set_sequence(4);
        auth3.mutable_dialogue_auth()->set_npc_id(42);
        Check(stream->Write(auth3) && stream->Read(&resp) &&
                  resp.has_dialogue_auth_result() && !resp.dialogue_auth_result().ok(),
              "token flood within 1s rejected");
    }

    game::ClientMessage logout;
    logout.set_sequence(9);
    logout.mutable_logout();
    stream->Write(logout);
    stream->WritesDone();
    stream->Finish();
}

} // namespace

int main() {
    ScenarioNormal();
    ScenarioDuplicateLogin();
    ScenarioUnauthenticated();
    ScenarioBadToken();
    ScenarioInventory();
    ScenarioDialogueAuth();

    std::cout << "\n==== " << (g_failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " ====" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
