#!/bin/bash
set -e
SRC=/mnt/d/oppoa2m/work/Image_su3.gz
KP=$HOME/a2m/kp
W=/mnt/d/oppoa2m/work

gunzip -f -c "$SRC" > "$KP/Image_kp"
cd "$KP"
echo "== 注入 KernelPatch =="
./kptools-linux -p -i Image_kp -k kpimg -o Image_kp_out 2>&1 | tail -8
echo "== 回读校验 =="
./kptools-linux -l -i Image_kp_out 2>&1 | head -8
gzip -9 -n -c Image_kp_out > "$W/Image_kp1.gz"
ls -lh "$W/Image_kp1.gz"
