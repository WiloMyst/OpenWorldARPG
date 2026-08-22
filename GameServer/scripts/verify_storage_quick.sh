#!/usr/bin/env bash
# 快速验证: Redis key 布局 + MySQL V2 表结构 (供 PIE 前最后检查)
set -u

echo "=== Redis (172.28.80.1:7000) ==="
python3 - <<'EOF'
import socket, time
s = socket.create_connection(('172.28.80.1', 7000), timeout=3)
def cmd(c):
    s.sendall((c + '\r\n').encode())
    time.sleep(0.15)
    return s.recv(65536).decode(errors='replace').replace('\r', '')
for c in ['PING', 'DBSIZE', 'KEYS *']:
    print(f'{c} -> {cmd(c).strip()}')
s.close()
EOF

echo ""
echo "=== MySQL V2 schema ==="
"$HOME/mysql-test/dist/bin/mysql" -h 172.28.80.1 -P 3306 -u game -pgame_pass_2026 game_server -e "
SHOW INDEX FROM players;
SHOW INDEX FROM inventory_items;
SELECT account, player_id, level, exp, last_login_at FROM players ORDER BY last_login_at DESC LIMIT 3;
SELECT COUNT(*) AS item_rows FROM inventory_items;
" 2>&1 | grep -v "Using a password"
