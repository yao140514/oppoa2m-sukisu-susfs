#!/bin/bash
set -eo pipefail

export TC=$HOME/a2m/toolchain/clang-r383902
export PATH=$TC/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export LLVM=1
export LLVM_IAS=1
export CC=clang
export LD=ld.lld
export AR=llvm-ar
export NM=llvm-nm
export OBJCOPY=llvm-objcopy
export OBJDUMP=llvm-objdump
export READELF=llvm-readelf
export OBJSIZE=llvm-size
export STRIP=llvm-strip

K=$HOME/a2m/k13/kernel
OUT=$HOME/a2m/k13/out
cd "$K"

MODE=${1:-defconfig}
case "$MODE" in
  setup)
    mkdir -p "$OUT"
    echo "K=$K OUT=$OUT"
    ls -ld "$K/vendor" "$OUT"
    ;;
  defconfig)
    mkdir -p "$OUT"
    make O="$OUT" k6833v1_64_k419_defconfig
    echo "== defconfig done =="
    ;;
  merge)
    cp /mnt/d/oppoa2m/work/dump/device_config.txt "$OUT/.config"
    make O="$OUT" olddefconfig
    echo "== merged device config =="
    ;;
  ksucfg)
    ./scripts/config --file "$OUT/.config" -e KSU -e KSU_FEATURE_ADBROOT -d KSU_DEBUG -d KSU_SUSFS -d KPM
    make O="$OUT" olddefconfig >/dev/null
    echo "== ksu config applied =="
    grep -E "^CONFIG_KSU|^CONFIG_KPM" "$OUT/.config"
    make O="$OUT" kernelrelease 2>/dev/null | tail -1
    ;;
  build)
    LOG=${2:-$HOME/a2m/k13/build.log}
    make O="$OUT" -j"$(nproc)" TARGET_PRODUCT=k6833v1_64_k419 Image.gz 2>&1 | tee "$LOG"
    echo "== build done =="
    ls -l "$OUT/arch/arm64/boot/Image.gz"
    ;;
  *)
    echo "usage: $0 {setup|defconfig|merge|ksucfg|build}"
    exit 1
    ;;
esac
