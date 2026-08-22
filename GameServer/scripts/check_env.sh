#!/bin/bash
# 环境探测: WSL 网关 IP / MySQL game 用户 / hiredis 可用性
set -u
echo "GATEWAY=$(ip route show default | awk '{print $3}')"

MYSQL="mysql"
if ! command -v mysql >/dev/null 2>&1; then
    export LD_LIBRARY_PATH="$HOME/mysql-test/libs/usr/lib/x86_64-linux-gnu:$HOME/mysql-test/libs/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
    MYSQL="$HOME/mysql-test/dist/bin/mysql"
fi

echo "--- game user connect test ---"
"$MYSQL" -h 172.28.80.1 -P 3306 -u game -pgame_pass_2026 game_server \
    -e "SHOW TABLES; SELECT account,player_id,level,exp FROM players LIMIT 5;" 2>&1 | head -15

echo "--- hiredis ---"
pkg-config --exists hiredis && echo "hiredis-ok $(pkg-config --modversion hiredis)" || echo "hiredis-missing"
ls /usr/include/hiredis/hiredis.h 2>/dev/null && echo "header-present" || echo "header-absent"

echo "--- redis ping (via gateway:7000) ---"
GW=$(ip route show default | awk '{print $3}')
timeout 3 bash -c "echo > /dev/tcp/$GW/7000" 2>/dev/null && echo "redis-port-open" || echo "redis-port-closed"
command -v redis-cli >/dev/null 2>&1 && redis-cli -h "$GW" -p 7000 ping 2>&1 || echo "redis-cli-not-installed"
