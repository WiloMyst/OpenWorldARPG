#!/usr/bin/env python3
"""E2E: GameServer 签发对话票据 -> VHServer 数据面校验

流程:
  1. 连 GameServer, 登录后申请对话票据
  2. 连 VHServer 双向流, 分别验证: 无票据 / 篡改签名 / 伪造过期票据 / 有效票据
"""
import hashlib
import hmac
import queue
import sys
import time

import grpc
import game_pb2
import game_pb2_grpc
import avatarStream_pb2
import avatarStream_pb2_grpc

GAME_ADDR = "127.0.0.1:50061"
VH_ADDR = "127.0.0.1:50051"
SECRET = "dev-dialogue-secret-2026"

passed = 0
failed = 0


def check(name, ok, detail=""):
    global passed, failed
    if ok:
        passed += 1
        print(f"[PASS] {name}")
    else:
        failed += 1
        print(f"[FAIL] {name}  {detail}")


def make_hmac_token(account, npc_id, expires_at_ms):
    payload = f"{account}.{npc_id}.{expires_at_ms}"
    sig = hmac.new(SECRET.encode(), payload.encode(), hashlib.sha256).hexdigest()
    return f"{payload}.{sig}"


def vh_request(token, text="hello"):
    """向 VHServer 发一条带票据的请求, 返回 (success, error_msg)"""
    chan = grpc.insecure_channel(VH_ADDR)
    stub = avatarStream_pb2_grpc.AvatarServiceStub(chan)
    out = queue.Queue()
    stream = stub.ChatWithAvatar(iter(out.get, None))
    req = avatarStream_pb2.AvatarStreamRequest(
        session_id="e2e_test", stream_type="TEXT_INFER",
        text_payload=text, auth_token=token)
    out.put(req)
    try:
        resp = next(stream)
        return resp.success, resp.error_msg
    finally:
        stream.cancel()
        chan.close()


def main():
    # ---- 1. GameServer: 登录 + 申请票据 ----
    chan = grpc.insecure_channel(GAME_ADDR)
    stub = game_pb2_grpc.GameServiceStub(chan)
    out = queue.Queue()
    stream = stub.GameChannel(iter(out.get, None))

    out.put(game_pb2.ClientMessage(
        sequence=1,
        login=game_pb2.LoginRequest(account="e2euser", token="dev-token-2026")))
    login_resp = next(stream)
    check("login ok", login_resp.login.success, login_resp.login.error_msg)

    out.put(game_pb2.ClientMessage(
        sequence=2,
        dialogue_auth=game_pb2.DialogueAuthRequest(npc_id=1)))
    auth_resp = next(stream)
    r = auth_resp.dialogue_auth_result
    check("dialogue token issued", r.ok, r.reason)
    token = r.dialogue_token
    check("token format", token.count(".") == 3 and len(token.split(".")[-1]) == 64)
    stream.cancel()
    chan.close()

    # ---- 2. VHServer: 票据校验矩阵 ----
    # 2a. 无票据 -> 拒绝
    ok, err = vh_request("")
    check("empty token rejected", not ok and "auth" in err.lower(), err)

    # 2b. 篡改签名 -> 拒绝
    parts = token.split(".")
    bad_sig = ("0" if parts[3][0] != "0" else "1") + parts[3][1:]
    ok, err = vh_request(f"{parts[0]}.{parts[1]}.{parts[2]}.{bad_sig}")
    check("tampered signature rejected", not ok and "signature" in err.lower(), err)

    # 2c. 伪造过期票据 (签名正确但已过期) -> 拒绝
    expired = make_hmac_token("e2euser", 1, int(time.time() * 1000) - 60000)
    ok, err = vh_request(expired)
    check("expired token rejected", not ok and "expired" in err.lower(), err)

    # 2d. 有效票据 -> 通过鉴权进入推理管线 (LLM API key 为占位值, 允许推理侧报错, 但不能是鉴权拒绝)
    ok, err = vh_request(token)
    auth_passed = "auth" not in err.lower() if not ok else True
    check("valid token passes auth", auth_passed, f"success={ok} err={err}")

    print(f"\n==== {'ALL TESTS PASSED' if failed == 0 else 'SOME TESTS FAILED'} ====")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
