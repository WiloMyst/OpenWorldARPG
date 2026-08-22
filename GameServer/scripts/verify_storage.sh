#!/bin/bash
# 验证: 表迁移日志 + Redis key 布局 + 新表结构
set -u
echo "=== server log: storage init ==="
grep -a -E 'MysqlStore|RedisStore' /mnt/d/Triton/OpenWorldARPG/GameServer/build/server_out.log | head -8

echo ""
echo "=== redis keys ==="
python3 - <<'EOF'
import socket, time
s = socket.create_connection(('172.28.80.1', 7000), timeout=3)
def cmd(c):
    s.sendall((c + '\r\n').encode())
    time.sleep(0.2)
    return s.recv(65536).decode(errors='replace')
for c in ['KEYS player:*', 'KEYS online:*', 'KEYS session:*', 'KEYS dlg:rate:*',
          'TTL player:m2inv', 'GET player:m2inv']:
    print(f'{c}\n{cmd(c)}')
s.close()
EOF

echo "=== mysql schema (V2) ==="
export LD_LIBRARY_PATH="$HOME/mysql-test/libs/usr/lib/x86_64-linux-gnu:$HOME/mysql-test/libs/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
"$HOME/mysql-test/dist/bin/mysql" -h 172.28.80.1 -P 3306 -u game -pgame_pass_2026 game_server -e "
SHOW CREATE TABLE players\G
SHOW INDEX FROM inventory_items;
SELECT account, player_id, level, last_login_at FROM players ORDER BY last_login_at DESC LIMIT 5;
" 2>/dev/null
