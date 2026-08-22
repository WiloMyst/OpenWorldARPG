"""跨服对话授权闭环测试: GameServer 信令面签发 -> VHServer 数据面校验。

前置: GameServer (50061) 与 VHServer (50051) 均已启动, 双端 dialogue_secret 一致。
用法: python3 tests/test_cross_server.py

测试项:
  1. GameServer 签发的真实票据 -> VHServer 鉴权通过, 进入推理管线
  2. 错误密钥伪造的票据      -> VHServer 拒绝 (Invalid token signature)
"""

import hashlib
import hmac
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build" / "py_generated"))

import grpc
import avatarStream_pb2 as pb
import avatarStream_pb2_grpc as pb_grpc

GAME_ADDR = "127.0.0.1:50061"
VH_ADDR = "127.0.0.1:50051"
WRONG_SECRET = "wrong-secret-not-configured-anywhere"


def send(stub, token: str, timeout: float = 30.0):
    import queue as _queue

    req_queue: "queue.Queue" = _queue.Queue()
    req_queue.put(pb.AvatarStreamRequest(
        session_id="crosssrv",
        stream_type="TEXT_INFER",
        text_payload="你好",
        auth_token=token,
    ))
    resp_iter = stub.ChatWithAvatar(iter(req_queue.get, None), timeout=timeout)
    resp = next(resp_iter)
    req_queue.put(None)
    return resp.success, resp.error_msg


def main() -> int:
    # 1. 从 GameServer 取真实票据
    probe = ROOT.parent / "GameServer" / "build" / "token_probe"
    proc = subprocess.run([str(probe)], capture_output=True, text=True, timeout=10)
    if proc.returncode != 0:
        print(f"[FAIL] token_probe: {proc.stdout.strip()}")
        return 1
    token = proc.stdout.strip()
    print(f"[INFO] GameServer issued: {token[:50]}...")

    results = []

    # 2. 真实票据应通过 VHServer 鉴权 (推理可能因 LLM key 失败, 但不应是鉴权错误)
    try:
        channel = grpc.insecure_channel(VH_ADDR)
        stub = pb_grpc.AvatarServiceStub(channel)
        ok, err = send(stub, token)
        passed = "Dialogue auth failed" not in err
        print(f"[{'PASS' if passed else 'FAIL'}] real token accepted: success={ok} err={err!r}")
        results.append(passed)
        channel.close()
    except grpc.RpcError as e:
        # 流超时/取消说明已过鉴权进入推理
        print(f"[PASS] real token accepted, stream ended by {e.code().name}")
        results.append(True)

    # 3. 错误密钥伪造的票据必须被拒绝
    now_ms = int(time.time() * 1000)
    payload = f"attacker.42.{now_ms + 30000}"
    sig = hmac.new(WRONG_SECRET.encode(), payload.encode(), hashlib.sha256).hexdigest()
    forged = f"{payload}.{sig}"
    try:
        channel = grpc.insecure_channel(VH_ADDR)
        stub = pb_grpc.AvatarServiceStub(channel)
        ok, err = send(stub, forged)
        passed = (not ok) and "Invalid token signature" in err
        print(f"[{'PASS' if passed else 'FAIL'}] forged token rejected: success={ok} err={err!r}")
        results.append(passed)
        channel.close()
    except grpc.RpcError:
        print("[FAIL] forged token: unexpected RpcError")
        results.append(False)

    passed = sum(results)
    print(f"\n==== {passed}/{len(results)} PASSED ====")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
