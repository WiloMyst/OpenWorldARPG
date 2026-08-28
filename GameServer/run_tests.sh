#!/bin/bash
# M1/M2/M4 + Phase1(AOI) 集成测试: 清测试数据 -> 启动服务器 -> 跑测试客户端 -> 优雅关闭
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/build"

# 1. 清理测试账号数据 (幂等, 保证 inventory/aoi 场景可重复)
# 优先系统 mysql CLI; 无 root 环境回退到 build/cleanup_test_data (libmysqlclient, 与服务器同源)
if command -v mysql >/dev/null 2>&1; then
    mysql -h 172.28.80.1 -P 3306 -u game -pgame_pass_2026 game_server \
        -e "DELETE FROM inventory_items WHERE account IN ('m2inv','aoiA','aoiB','arch1','dmg1','carol'); DELETE FROM owned_characters WHERE account IN ('arch1'); DELETE FROM player_positions WHERE account IN ('aoiA','aoiB','arch1','dmg1','carol'); DELETE FROM character_hp WHERE account IN ('arch1','dmg1','carol'); DELETE FROM players WHERE account IN ('m2inv','aoiA','aoiB','arch1','dmg1','carol');" \
        2>/dev/null && echo "[prep] test data cleared" || echo "[prep] WARN: cleanup failed"
else
    "$SCRIPT_DIR/build/cleanup_test_data" \
        && echo "[prep] test data cleared" || echo "[prep] WARN: cleanup failed"
fi

# 2. 启动服务器 (先清残留进程, 避免旧实例占端口导致测试打到旧代码)
pkill -f '[g]ame_server$' 2>/dev/null && sleep 1
./game_server > server_out.log 2>&1 &
SERVER_PID=$!
sleep 2

# 3. 跑测试 (TCP 服务器已移除, test_tcp 场景并入 gRPC 路径)
./test_client
RC=$?
if [ $RC -eq 0 ]; then
    ./test_aoi
    RC=$?
fi

# 4. 关闭服务器
kill -INT $SERVER_PID 2>/dev/null
sleep 1
kill -9 $SERVER_PID 2>/dev/null

echo "TEST_RC=$RC"
echo "---- server log ----"
tail -50 server_out.log
exit $RC
