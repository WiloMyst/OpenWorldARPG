#!/bin/bash
# M2 集成测试: 清测试数据 -> 启动服务器 -> 跑测试客户端 -> 优雅关闭
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/build"

# 1. 清理测试账号数据 (幂等, 保证 inventory 场景可重复)
MYSQL="mysql"
if ! command -v mysql >/dev/null 2>&1; then
    # 无系统 mysql 客户端时回退到 ~/mysql-test 便携客户端
    export LD_LIBRARY_PATH="$HOME/mysql-test/libs/usr/lib/x86_64-linux-gnu:$HOME/mysql-test/libs/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
    MYSQL="$HOME/mysql-test/dist/bin/mysql"
fi
"$MYSQL" -h 172.28.80.1 -P 3306 -u game -pgame_pass_2026 game_server \
    -e "DELETE FROM inventory_items WHERE account='m2inv'; DELETE FROM players WHERE account='m2inv';" \
    2>/dev/null && echo "[prep] m2inv data cleared" || echo "[prep] WARN: cleanup failed"

# 2. 启动服务器 (先清残留进程, 避免旧实例占端口导致测试打到旧代码)
pkill -f '[g]ame_server$' 2>/dev/null && sleep 1
./game_server > server_out.log 2>&1 &
SERVER_PID=$!
sleep 2

# 3. 跑测试
./test_client
RC=$?

# 4. 关闭服务器
kill -INT $SERVER_PID 2>/dev/null
sleep 1
kill -9 $SERVER_PID 2>/dev/null

echo "TEST_RC=$RC"
echo "---- server log ----"
tail -50 server_out.log
exit $RC
