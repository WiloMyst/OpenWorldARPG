#!/bin/bash
# M3 联调辅助: 清理测试账号 + 重启分离的服务器进程
set -u
BUILD_DIR="/mnt/d/Triton/OpenWorldARPG/GameServer/build"

MYSQL="mysql"
if ! command -v mysql >/dev/null 2>&1; then
    export LD_LIBRARY_PATH="$HOME/mysql-test/libs/usr/lib/x86_64-linux-gnu:$HOME/mysql-test/libs/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
    MYSQL="$HOME/mysql-test/dist/bin/mysql"
fi
"$MYSQL" -h 172.28.80.1 -P 3306 -u game -pgame_pass_2026 game_server \
    -e "DELETE FROM inventory_items WHERE account='m2inv'; DELETE FROM players WHERE account='m2inv';" \
    >/dev/null 2>&1 && echo "[prep] m2inv data cleared" || echo "[prep] WARN: cleanup failed"

pkill -f '[g]ame_server$' 2>/dev/null && echo "[prep] old server killed" || echo "[prep] no old server"
sleep 1

cd "$BUILD_DIR"
setsid nohup ./game_server > server_out.log 2>&1 < /dev/null &
sleep 2
if ss -tln | grep -q 50061; then
    echo "[ok] game_server listening on 50061"
else
    echo "[err] server failed to start"
    tail -5 server_out.log
fi
