#!/bin/bash
# GameServer 重新编译 (WSL 内执行)
set -e
cd /mnt/d/Triton/OpenWorldARPG/GameServer/build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
make -j$(nproc) game_server test_client token_probe test_aoi test_combat 2>&1 | tail -15
echo "BUILD_OK"
ldd ./game_server | grep -E 'hiredis|mysql' || true
