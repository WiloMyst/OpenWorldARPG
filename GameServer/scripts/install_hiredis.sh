#!/bin/bash
set -u
sudo -n apt-get update -qq 2>&1 | tail -2
sudo -n apt-get install -y libhiredis-dev redis-tools 2>&1 | tail -3
echo "---verify---"
pkg-config --exists hiredis && echo "hiredis-ok $(pkg-config --modversion hiredis)" || echo "hiredis-missing"
ls /usr/include/hiredis/hiredis.h >/dev/null 2>&1 && echo "header-present" || echo "header-absent"
GW=$(ip route show default | awk '{print $3}')
redis-cli -h "$GW" -p 7000 ping 2>&1
