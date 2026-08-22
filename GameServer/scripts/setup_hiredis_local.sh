#!/bin/bash
# 免 root 安装 hiredis: apt download + dpkg -x 解包到 ~/hiredis-local
set -u
DIST="$HOME/hiredis-local"
mkdir -p "$DIST"
cd "$DIST"

apt-get download libhiredis-dev libhiredis0.14 2>&1 | tail -3
ls -1 *.deb 2>/dev/null

for deb in *.deb; do
    dpkg -x "$deb" "$DIST/root"
done

echo "---layout---"
find "$DIST/root" -name "hiredis.h" -o -name "libhiredis*" | head -10
echo "---gcc test compile---"
cat > /tmp/hiredis_test.c <<'EOF'
#include <hiredis/hiredis.h>
#include <stdio.h>
int main(void) {
    redisContext* c = redisConnect("172.28.80.1", 7000);
    if (!c || c->err) { printf("connect fail: %s\n", c ? c->errstr : "alloc"); return 1; }
    redisReply* r = redisCommand(c, "PING");
    printf("PING -> %s\n", r->str ? r->str : "(nil)");
    freeReplyObject(r); redisFree(c);
    return 0;
}
EOF
gcc /tmp/hiredis_test.c -I"$DIST/root/usr/include" -L"$DIST/root/usr/lib/x86_64-linux-gnu" -lhiredis -o /tmp/hiredis_test \
    && LD_LIBRARY_PATH="$DIST/root/usr/lib/x86_64-linux-gnu" /tmp/hiredis_test \
    && echo "HIREDIS_LOCAL_OK=$DIST"
