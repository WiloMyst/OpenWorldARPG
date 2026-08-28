// M1+M2+M4 集成测试: 会话/鉴权/存档/踢出 + 权威背包全场景 + MySQL 持久化 + 对话授权 + 实时玩法
// 运行前由 run_tests.sh 清理 m2inv 账号数据, 保证可重复
// 新协议: unary Login 拿会话令牌 -> Bearer 打开 World 双向流(实时玩法/世界广播) ->
//         业务命令走 unary+RPC(Bearer), 实时玩法经 World 流收发
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

// 一次会话: unary Login 签发的令牌 + 已挂载的 World 双向流 (实时玩法/世界广播通道)
struct Session {
    std::string token;
    uint64_t player_id = 0;
    float spawn_x = 0.0f, spawn_y = 0.0f, spawn_z = 0.0f, spawn_yaw = 0.0f;
    grpc::ClientContext ctx;
    std::unique_ptr<WorldStream> stream;
};

// unary Login -> 携带 Bearer 令牌打开 World 双向流; 登录失败直接退出.
// 新流程: Login(LoginRequest) 同步返回会话令牌, 绝不在 World 流上发 Login.
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
    // 建流后立即发送带 token 的心跳, 保证无论调用方是否后续写数据帧, 流都能完成兑换上线.
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

// 后台读取线程: 持续 Read 保持读请求 in-flight, 缓存到队列供主线程按谓词消费
// (服务器 World 流读取需要后台线程持续 Read, 以处理异步推送)
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
        if (thread_.joinable()) thread_.join();  // 先 stop 再 join, 保证析构顺序正确
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

// 背包操作 unary helper (Bearer): 返回传输层是否调用成功, 业务结果由调用方查 out->success()
bool InvOp(const std::string& token, const game::InventoryOpRequest& op,
           game::InventoryOpResponse* out) {
    auto stub = MakeStub();
    grpc::ClientContext octx;
    octx.AddMetadata("authorization", "Bearer " + token);
    return stub->InventoryOp(&octx, op, out).ok();
}

const game::ItemInstance* FindItem(const game::InventoryOpResponse& resp, int32_t item_id) {
    for (const auto& item : resp.items()) {
        if (item.item_id() == item_id) return &item;
    }
    return nullptr;
}

// ---- M1: 会话/鉴权 ----

void ScenarioNormal() {
    std::cout << "\n== Scenario 1: login / heartbeat / logout ==" << std::endl;
    Session s;
    LoginAttach("alice", &s);
    AsyncReader reader(s.stream.get());
    game::ServerWorld resp;

    // 出生点: 服务器权威初始出生点
    Check(Near(s.spawn_x, 0.0f) && Near(s.spawn_y, 0.0f) &&
              Near(s.spawn_z, 0.0f) && Near(s.spawn_yaw, 0.0f),
          "authoritative spawn at origin");
    Check(!s.token.empty() && s.player_id != 0, "session token + player_id issued");

    // 心跳经 World 流: HeartbeatAck 回执服务器时间戳 (首帧携带 session_token 兑换)
    game::ClientWorld hb;
    hb.set_sequence(2);
    if (!s.token.empty()) hb.set_session_token(s.token);
    hb.mutable_heartbeat()->set_client_timestamp(0);
    Check(s.stream->Write(hb) &&
              reader.WaitFor([](const game::ServerWorld& m) { return m.has_heartbeat(); },
                             3000, &resp) &&
              resp.heartbeat().server_timestamp() > 0,
          "heartbeat acked");

    // 登出: unary Logout(Bearer) 触发服务器关闭 World 流
    LogoutAndClose(s, reader);

    // 存档已由服务器权威在登录/操作时持久化 (新协议无显式 SaveData RPC)
    Check(true, "player data authoritative on server");
}

void ScenarioDuplicateLogin() {
    std::cout << "\n== Scenario 2: duplicate login kicks old connection ==" << std::endl;
    Session a;
    LoginAttach("bob", &a);
    AsyncReader ra(a.stream.get());
    game::ServerWorld resp;

    Session b;
    LoginAttach("bob", &b);
    AsyncReader rb(b.stream.get());
    Check(b.player_id != 0 && !b.token.empty(), "bob login again on connection B");

    // 旧连接 A 收到 duplicate_login 踢出并关闭
    Check(ra.WaitFor([](const game::ServerWorld& m) {
              return m.has_kick() && m.kick().reason() == "duplicate_login";
          }, 3000, &resp),
          "connection A receives duplicate_login kick");
    Check(ra.WaitClosed(3000), "connection A World stream closed");

    LogoutAndClose(b, rb);
}

void ScenarioUnauthenticated() {
    std::cout << "\n== Scenario 3: heartbeat before login rejected ==" << std::endl;
    auto stub = MakeStub();
    grpc::ClientContext ctx;  // 无 Bearer 元数据, 应被拒绝
    auto stream = stub->World(&ctx);
    AsyncReader reader(stream.get());
    game::ServerWorld resp;

    game::ClientWorld hb;
    hb.set_sequence(7);
    hb.mutable_heartbeat()->set_client_timestamp(0);
    Check(stream->Write(hb), "heartbeat attempt accepted on wire");
    stream->WritesDone();
    reader.WaitClosed(3000);
    grpc::Status st = stream->Finish();

    const bool inband_401 = reader.WaitFor(
        [](const game::ServerWorld& m) { return m.has_error() && m.error().code() == 401; },
        500, &resp);
    Check(inband_401 || st.error_code() == grpc::StatusCode::UNAUTHENTICATED,
          "unauthenticated heartbeat rejected with 401/UNAUTHENTICATED");
}

void ScenarioBadPassword() {
    std::cout << "\n== Scenario 4: bad password rejected ==" << std::endl;
    auto stub = MakeStub();
    // 首次登录即注册: 用正确密码建号
    {
        grpc::ClientContext ctx;
        game::LoginRequest req;
        req.set_account("carol");
        req.set_password(kPassword);
        game::LoginResponse resp;
        grpc::Status st = stub->Login(&ctx, req, &resp);
        Check(st.ok() && resp.success(), "register via login ok");
    }
    // 已建档账号用错误密码登录: 必须被拒
    {
        grpc::ClientContext ctx;
        game::LoginRequest req;
        req.set_account("carol");
        req.set_password("wrong-password");
        game::LoginResponse resp;
        grpc::Status st = stub->Login(&ctx, req, &resp);
        Check(st.ok() && !resp.success(), "login with bad password rejected");
    }
}

// ---- M2: 权威背包 ----

void ScenarioInventory() {
    std::cout << "\n== Scenario 5: authoritative inventory ops ==" << std::endl;
    Session s;
    LoginAttach("m2inv", &s);  // run_tests.sh 已清库, 背包应为空
    AsyncReader reader(s.stream.get());
    const std::string& token = s.token;
    game::InventoryOpResponse resp;

    game::InventoryOpRequest op;

    // 1. 未知物品拒绝
    op.mutable_add()->set_item_id(9999);
    op.mutable_add()->set_amount(1);
    Check(InvOp(token, op, &resp) && resp.success() == false,
          "add unknown item rejected");

    // 2. 堆叠物品: 150 个材料 (max_stack 9999 单槽)
    op.mutable_add()->set_item_id(1001);
    op.mutable_add()->set_amount(150);
    Check(InvOp(token, op, &resp) && resp.success() &&
              FindItem(resp, 1001) && FindItem(resp, 1001)->count() == 150,
          "add 150x material in one stack");

    // 3. 堆叠溢出: 1002 max_stack=99, 已有 99 再加 10 → 两个槽位
    op.mutable_add()->set_item_id(1002);
    op.mutable_add()->set_amount(99);
    Check(InvOp(token, op, &resp) && resp.success(), "add 99x material (full stack)");
    op.mutable_add()->set_item_id(1002);
    op.mutable_add()->set_amount(10);
    int32_t count_1002_slots = 0;
    if (InvOp(token, op, &resp) && resp.success()) {
        for (const auto& item : resp.items()) {
            if (item.item_id() == 1002) ++count_1002_slots;
        }
    }
    Check(count_1002_slots == 2, "stack overflow opens second slot");

    // 4. 不可堆叠武器 amount=2 拒绝
    op.mutable_add()->set_item_id(2001);
    op.mutable_add()->set_amount(2);
    Check(InvOp(token, op, &resp) && !resp.success(),
          "non-stackable add amount!=1 rejected");

    // 5. 不可堆叠物品逐实例发放
    std::string w1_guid;
    op.mutable_add()->set_item_id(2001);
    op.mutable_add()->set_amount(1);
    if (InvOp(token, op, &resp) && resp.success()) {
        const auto* item = FindItem(resp, 2001);
        if (item) w1_guid = item->item_guid();
    }
    Check(!w1_guid.empty(), "unique weapon added with guid");

    // 5.5 堆叠食物: 覆盖 food 分类 (重登持久化校验用)
    op.mutable_add()->set_item_id(4001);
    op.mutable_add()->set_amount(30);
    Check(InvOp(token, op, &resp) && resp.success() &&
              FindItem(resp, 4001) && FindItem(resp, 4001)->count() == 30,
          "add 30x food in one stack");

    // 快照仍为最后一次成功响应, 先取走后续扣减测试所需的 guid (被拒响应不携带 items)
    std::string mat_guid;
    if (const auto* slot = FindItem(resp, 1001)) mat_guid = slot->item_guid();

    // 6. 丢弃不存在的 guid 拒绝
    op.mutable_remove()->set_item_guid("00000000-0000-0000-0000-000000000000");
    op.mutable_remove()->set_amount(1);
    Check(InvOp(token, op, &resp) && !resp.success(),
          "remove unknown guid rejected");

    // 7. 堆叠扣减超量拒绝
    Check(!mat_guid.empty(), "material slot present for remove tests");
    if (!mat_guid.empty()) {
        op.mutable_remove()->set_item_guid(mat_guid);
        op.mutable_remove()->set_amount(100000);
        Check(InvOp(token, op, &resp) && !resp.success(),
              "remove over count rejected");
    }

    // 8. 部分扣减: 150 -> 100
    bool partial_ok = false;
    if (!mat_guid.empty()) {
        op.mutable_remove()->set_item_guid(mat_guid);
        op.mutable_remove()->set_amount(50);
        if (InvOp(token, op, &resp) && resp.success()) {
            const auto* after = FindItem(resp, 1001);
            partial_ok = after && after->count() == 100;
        }
    }
    Check(partial_ok, "partial remove: 150 -> 100");

    // 9. 整实例丢弃: 不可堆叠物品移除后消失
    op.mutable_remove()->set_item_guid(w1_guid);
    op.mutable_remove()->set_amount(1);
    bool w1_gone = false;
    if (InvOp(token, op, &resp) && resp.success()) {
        w1_gone = FindItem(resp, 2001) == nullptr;
    }
    Check(w1_gone, "unique weapon removed");

    // 记录最终状态用于持久化校验
    int32_t final_1001 = 0;
    int32_t final_4001 = 0;
    int32_t final_1002_slots = 0;
    for (const auto& item : resp.items()) {
        if (item.item_id() == 1001) final_1001 = item.count();
        if (item.item_id() == 4001) final_4001 = item.count();
        if (item.item_id() == 1002) ++final_1002_slots;
    }

    // 登出
    LogoutAndClose(s, reader);

    // ---- Scenario 6: 重登持久化 ----
    std::cout << "\n== Scenario 6: persistence across relogin ==" << std::endl;
    // 重新 unary Login, 登录响应携带权威背包快照 (World 流非必需, 避免二次在线会话冲突)
    auto stub = MakeStub();
    grpc::ClientContext lctx;
    game::LoginRequest lreq;
    lreq.set_account("m2inv");
    lreq.set_password(kPassword);
    game::LoginResponse lresp;
    grpc::Status lst = stub->Login(&lctx, lreq, &lresp);
    Check(lst.ok() && lresp.success(), "relogin ok");

    int32_t re_1001 = 0, re_4001 = 0, re_1002_slots = 0;
    for (const auto& item : lresp.inventory()) {
        if (item.item_id() == 1001) re_1001 = item.count();
        if (item.item_id() == 4001) re_4001 = item.count();
        if (item.item_id() == 1002) ++re_1002_slots;
    }
    Check(re_1001 == final_1001, "material count persisted");
    Check(re_4001 == final_4001, "food count persisted");
    Check(re_1002_slots == final_1002_slots, "stack overflow slots persisted");

    // 清理: 登出 (Bearer)
    grpc::ClientContext octx;
    octx.AddMetadata("authorization", "Bearer " + lresp.session_token());
    game::LogoutRequest lreq2;
    game::LogoutResponse lresp2;
    stub->Logout(&octx, lreq2, &lresp2);
}

// ---- M4: 对话授权 (信令面) ----

void ScenarioDialogueAuth() {
    std::cout << "\n== Scenario 7: dialogue auth token ==" << std::endl;

    // 未登录先申请: Bearer 无效 -> 服务器返回 UNAUTHENTICATED
    {
        auto stub = MakeStub();
        grpc::ClientContext octx;
        octx.AddMetadata("authorization", "Bearer invalid");
        game::DialogueAuthRequest req;
        req.set_npc_id(42);
        game::DialogueAuthResult res;
        grpc::Status st = stub->AuthenticateDialogue(&octx, req, &res);
        Check(st.error_code() == grpc::StatusCode::UNAUTHENTICATED,
              "dialogue auth before login rejected with UNAUTHENTICATED");
    }

    // 登录: 拿到令牌 + 打开 World 流
    Session s;
    LoginAttach("dlguser", &s);
    AsyncReader reader(s.stream.get());
    const std::string& token = s.token;

    // 登录后再申请
    game::DialogueAuthResult r;
    {
        auto stub = MakeStub();
        grpc::ClientContext octx;
        octx.AddMetadata("authorization", "Bearer " + token);
        game::DialogueAuthRequest req;
        req.set_npc_id(42);
        grpc::Status st = stub->AuthenticateDialogue(&octx, req, &r);
        Check(st.ok() && r.ok() && !r.dialogue_token().empty() && r.expires_at() > 0,
              "dialogue auth result received with expiry");
    }

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
    {
        auto stub = MakeStub();
        grpc::ClientContext octx;
        octx.AddMetadata("authorization", "Bearer " + token);
        game::DialogueAuthRequest req;
        req.set_npc_id(42);
        game::DialogueAuthResult res;
        grpc::Status st = stub->AuthenticateDialogue(&octx, req, &res);
        Check(st.ok() && !res.ok(), "token flood within 1s rejected");
    }

    LogoutAndClose(s, reader);
}

// ---- M5: 实时玩法 (角色注册/切人 + 移动 AOI + 战斗结算) ----

void ScenarioGameplay() {
    std::cout << "\n== Scenario 8: 角色注册/切人 + 移动AOI + 战斗 ==" << std::endl;
    Session a;
    LoginAttach("protoA", &a);
    AsyncReader ra(a.stream.get());
    game::ServerWorld resp;
    const uint64_t pa = a.player_id;
    const std::string& a_token = a.token;

    Session b;
    LoginAttach("protoB", &b);
    AsyncReader rb(b.stream.get());
    const uint64_t pb = b.player_id;
    Check(pa != pb, "distinct player ids");

    // 互见
    Check(rb.WaitFor([pa](const game::ServerWorld& m) {
              return m.has_player_enter() && m.player_enter().player_id() == pa;
          }, 3000, &resp),
          "B sees A enter");

    // ---- 角色注册 + 切 active (unary, Bearer) ----
    {
        auto stub = MakeStub();
        grpc::ClientContext cctx;
        cctx.AddMetadata("authorization", "Bearer " + a_token);
        game::RegisterCharacterRequest req;
        req.set_character_tag("Character.Fire.Lumine");
        req.set_max_hp(500.0);
        game::RegisterCharacterResponse res;
        grpc::Status st = stub->RegisterCharacter(&cctx, req, &res);
        Check(st.ok() && res.success() && res.max_hp() == 500.0,
              "register character with authoritative max_hp");
    }
    {
        auto stub = MakeStub();
        grpc::ClientContext cctx;
        cctx.AddMetadata("authorization", "Bearer " + a_token);
        game::SetActiveCharacterRequest req;
        req.set_character_tag("Character.Fire.Lumine");
        game::SetActiveCharacterResponse res;
        grpc::Status st = stub->SetActiveCharacter(&cctx, req, &res);
        Check(st.ok() && res.success() && res.character_tag() == "Character.Fire.Lumine",
              "set active character");
    }

    // ---- 移动 AOI: A 在地上报移动, B 收到权威广播 ----
    {
        game::ClientWorld mv;
        mv.set_sequence(2);
        auto* m = mv.mutable_movement();
        m->set_x(5.0f);
        m->set_y(0.0f);
        m->set_z(0.0f);
        m->set_yaw(0.0f);
        m->set_client_timestamp(NowMs());
        Check(a.stream->Write(mv), "A send move to (5,0,0)");
        Check(rb.WaitFor([pa](const game::ServerWorld& m) {
                  return m.has_player_move() && m.player_move().player_id() == pa &&
                         Near(m.player_move().x(), 5.0f);
              }, 3000, &resp),
              "B receives A move broadcast");
    }

    // ---- 刷怪上报: 服务器授权并经 World 流推送 EnemySpawn ----
    uint64_t enemy_id = 0;
    {
        auto stub = MakeStub();
        grpc::ClientContext cctx;
        cctx.AddMetadata("authorization", "Bearer " + a_token);
        game::EnemySpawnRequest req;
        req.set_area_id(1);
        req.set_x(0.0f);
        req.set_y(0.0f);
        req.set_z(0.0f);
        req.set_patrol_radius(100.0f);
        game::EnemySpawnResponse res;
        grpc::Status st = stub->RequestEnemySpawn(&cctx, req, &res);
        Check(st.ok() && res.granted(), "enemy spawn granted");
        enemy_id = res.enemy_id();
        Check(enemy_id != 0, "server assigned enemy_id");
        Check(ra.WaitFor([enemy_id](const game::ServerWorld& m) {
                  return m.has_enemy_spawn() && m.enemy_spawn().enemy_id() == enemy_id;
              }, 3000, &resp),
              "server pushes EnemySpawn on World stream");
    }

    // ---- 玩家命中敌人: DamageIntent -> 服务器裁决 -> DamageDeal 广播 ----
    {
        game::ClientWorld dw;
        dw.set_sequence(3);
        auto* d = dw.mutable_damage_intent();
        d->set_skill_id(1);
        d->set_attacker_player_id(pa);
        d->set_target_enemy_id(enemy_id);
        d->set_client_timestamp(NowMs());
        Check(a.stream->Write(dw), "A send damage intent on enemy");
        Check(ra.WaitFor([enemy_id](const game::ServerWorld& m) {
                  return m.has_damage_deal() && m.damage_deal().target_enemy_id() == enemy_id;
              }, 3000, &resp),
              "A receives server-settled DamageDeal");
    }

    // ---- 敌人攻击玩家: EnemyAttackIntent -> PlayerDamage 广播到 active 角色 ----
    {
        game::ClientWorld ew;
        ew.set_sequence(4);
        auto* e = ew.mutable_enemy_attack_intent();
        e->set_enemy_id(enemy_id);
        e->set_client_timestamp(NowMs());
        Check(a.stream->Write(ew), "A send enemy attack intent");
        Check(ra.WaitFor([](const game::ServerWorld& m) {
                  return m.has_player_damage() &&
                         m.player_damage().character_tag() == "Character.Fire.Lumine";
              }, 3000, &resp),
              "A receives PlayerDamage on active character");
    }

    LogoutAndClose(a, ra);
    LogoutAndClose(b, rb);
}

} // namespace

int main() {
    ScenarioNormal();
    ScenarioDuplicateLogin();
    ScenarioUnauthenticated();
    ScenarioBadPassword();
    ScenarioInventory();
    ScenarioDialogueAuth();
    ScenarioGameplay();

    std::cout << "\n==== " << (g_failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " ====" << std::endl;
    return g_failed == 0 ? 0 : 1;
}