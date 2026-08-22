#!/usr/bin/env bash
# 检查测试运行后 Redis 中的 key 布局 (缓存/会话/在线/频控)
set -u
python3 - <<'EOF'
import socket, time
s = socket.create_connection(('172.28.80.1', 7000), timeout=3)
def cmd(c):
    s.sendall((c + '\r\n').encode())
    time.sleep(0.15)
    return s.recv(65536).decode(errors='replace').replace('\r', '')
for c in ['DBSIZE', 'KEYS player:*', 'KEYS session:*', 'KEYS online:*', 'KEYS dlg:rate:*',
          'TTL player:dlguser', 'GET player:dlguser']:
    print(f'{c} -> {cmd(c).strip()}')
s.close()
EOF
