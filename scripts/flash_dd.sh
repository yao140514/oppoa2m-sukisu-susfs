#!/bin/bash
# 用 dd 直接写 boot_a（有 root，不需要 fastboot），写完校验再重启
set -e
IMG="${1:-D:/oppoa2m/out/boot_susfs_fix.img}"
cd /d/oppoa2m

echo "== 推送镜像: $IMG =="
MSYS_NO_PATHCONV=1 adb push "$IMG" /data/local/tmp/test_boot.img
SIZE=$(MSYS_NO_PATHCONV=1 adb shell "stat -c %s /data/local/tmp/test_boot.img" | tr -d '\r')
if [ "$SIZE" != "67108864" ]; then
  echo "[!] 推送大小异常: $SIZE"; exit 1
fi
echo "推送大小 OK: $SIZE"

echo "== 写入 boot_a =="
MSYS_NO_PATHCONV=1 adb shell "su -c 'sync; dd if=/data/local/tmp/test_boot.img of=/dev/block/by-name/boot_a bs=1M conv=fsync 2>/dev/null; sync'"

echo "== 校验（两行应一致）=="
MSYS_NO_PATHCONV=1 adb shell "su -c 'md5sum /data/local/tmp/test_boot.img; dd if=/dev/block/by-name/boot_a bs=1M 2>/dev/null | md5sum'"

echo "== 重启 =="
MSYS_NO_PATHCONV=1 adb reboot
sleep 55
MSYS_NO_PATHCONV=1 adb wait-for-device
for i in $(seq 1 30); do
  b=$(MSYS_NO_PATHCONV=1 adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')
  [ "$b" = "1" ] && { echo "开机完成 (${i}0s)"; break; }
  sleep 6
done
echo "boot=$b"
MSYS_NO_PATHCONV=1 adb shell "uname -v"
