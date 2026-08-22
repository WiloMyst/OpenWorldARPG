#!/bin/bash
# 降级模式验证: 错误 Redis 端口启动 -> 登录仍可用 + limiter=memory
set -u
BUILD=/mnt/d/Triton/OpenWorldARPG/GameServer/build
cd "$BUILD"

pkill -f '[g]ame_server$' 2>/dev/null && sleep 1

# 临时配置: Redis 指向不存在的端口
sed 's/^  port: 7000/  port: 7999/' ../config.yaml > config_degraded.yaml
cp ../config.yaml config_real.yaml
mv config_degraded.yaml ../config.yaml

setsid nohup ./game_server > server_out.log 2>&1 < /dev/null &
sleep 2

if ss -tln | grep -q 50061; then
    echo "[ok] server started in degraded mode"
else
    echo "[err] server failed to start"
    tail -5 server_out.log
    mv config_real.yaml ../config.yaml
    exit 1
fi

# 冒烟: 登录 + 对话票据 (应走内存频控)
./test_client 2>&1 | grep -E 'Scenario 7|token flood|dialogue auth result|token issued' | head -5
grep -a -E 'RedisStore|cache=|limiter=' server_out.log | head -6

# 恢复真实配置并按正常模式重启
pkill -f '[g]ame_server$' 2>/dev/null && sleep 1
mv config_real.yaml ../config.yaml
setsid nohup ./game_server > server_out.log 2>&1 < /dev/null &
sleep 2
if ss -tln | grep -q 50061; then
    echo "[ok] server restarted in normal mode"
    grep -a 'RedisStore' server_out.log | head -2
else
    echo "[err] restart failed"
    tail -5 server_out.log
fi
