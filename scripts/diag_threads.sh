#!/bin/bash
# 诊断进程各线程阻塞点: pid 作为参数
pid=$1
for t in /proc/$pid/task/*; do
  tid=$(basename "$t")
  name=$(cat "$t/comm" 2>/dev/null)
  state=$(grep State "$t/status" 2>/dev/null | awk '{print $2}')
  wchan=$(cat "$t/wchan" 2>/dev/null)
  echo "tid=$tid name=$name state=$state wchan=$wchan"
done
