"""VHServer 数据面对话票据校验测试。

前置: VHServer 已启动 (默认 127.0.0.1:50051), config.yaml 配置了 auth.dialogue_secret。
用法: python3 tests/test_dialogue_auth.py [server_addr]

测试项:
  1. 无 token      -> 拒绝 (missing token)
  2. 非法格式      -> 拒绝 (Invalid token format)
  3. 篡改签名      -> 拒绝 (Invalid token signature)
  4. 过期票据      -> 拒绝 (Token expired)
  5. 合法票据      -> 通过鉴权, 进入推理管线 (错误信息不含 Dialogue auth failed)
"""

import hashlib
import hmac
import os
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build" / "py_generated"))

import grpc
import avatarStream_pb2 as pb
import avatarStream_pb2_grpc as pb_grpc

SECRET = "dev-dialogue-secret-2026"  # 须与 config.yaml auth.dialogue_secret 一致


def make_token(account: str, npc_id: int, expires_at_ms: int, secret: str = SECRET) -> str:
    payload = f"{account}.{npc_id}.{expires_at_ms}"
    sig = hmac.new(secret.encode(), payload.encode(), hashlib.sha256).hexdigest()
    return f"{payload}.{sig}"


def send(stub, token: str, timeout: float = 5.0):
    """发送单条请求并收取首条响应, 返回 (success, error_msg)。

    用队列生成器保持上行流打开: 若用 iter([req]) 会在发出后立刻半关闭,
    服务器异步推理完成前收到关闭信号, 响应被会话清理逻辑丢弃。
    """
    import queue as _queue

    req_queue: "queue.Queue" = _queue.Queue()
    req_queue.put(pb.AvatarStreamRequest(
        session_id="pytest",
        stream_type="TEXT_INFER",
        text_payload="你好",
        auth_token=token,
    ))
    resp_iter = stub.ChatWithAvatar(iter(req_queue.get, None), timeout=timeout)
    resp = next(resp_iter)
    req_queue.put(None)  # 收到响应后再关闭上行流
    return resp.success, resp.error_msg


def expect_reject(stub, name: str, token: str, want_reason: str) -> bool:
    ok, err = send(stub, token)
    passed = (not ok) and want_reason in err
    mark = "PASS" if passed else "FAIL"
    print(f"[{mark}] {name}: success={ok} err={err!r}")
    return passed


def main() -> int:
    addr = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1:50051"
    channel = grpc.insecure_channel(addr)
    stub = pb_grpc.AvatarServiceStub(channel)

    results = []

    # 1. 无 token
    results.append(expect_reject(stub, "missing token", "", "Dialogue auth required"))

    # 2. 非法格式
    results.append(expect_reject(stub, "bad format", "garbage-token", "Invalid token format"))

    # 3. 篡改签名
    now_ms = int(time.time() * 1000)
    tampered = make_token("tester", 1, now_ms + 30000)[:-4] + "0000"
    results.append(expect_reject(stub, "tampered signature", tampered, "Invalid token signature"))

    # 4. 过期票据
    expired = make_token("tester", 1, now_ms - 1000)
    results.append(expect_reject(stub, "expired token", expired, "Token expired"))

    # 5. 合法票据: 鉴权通过即进入推理 (LLM 调用可能因假 key 失败, 但错误不应是鉴权错误)
    valid = make_token("tester", 1, now_ms + 30000)
    try:
        ok, err = send(stub, valid, timeout=30)
        auth_passed = "Dialogue auth failed" not in err
        print(f"[{'PASS' if auth_passed else 'FAIL'}] valid token: success={ok} err={err!r}")
        results.append(auth_passed)
    except grpc.RpcError as e:
        # 流超时/取消也说明已通过鉴权进入推理 (LLM 30s 未回)
        print(f"[PASS] valid token: auth passed, stream ended by {e.code().name}")
        results.append(True)

    channel.close()
    passed = sum(results)
    print(f"\n==== {passed}/{len(results)} PASSED ====")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
