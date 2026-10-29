#!/bin/bash
# 编译 MTK 连接类外部模块（与自编内核同源）
set -o pipefail
source /home/ai/a2m/env13.sh

K=$HOME/a2m/k13/kernel
OUT=$HOME/a2m/k13/out
MODROOT=$HOME/a2m/modules13/vendor/mediatek/kernel_modules
CONN=$MODROOT/connectivity
TOP=$HOME/a2m/modules13
LOGD=$HOME/a2m/k13/modlogs
mkdir -p "$LOGD"

COMMON_INC="-I$CONN/common/common_main/include -I$CONN/common/common_main/linux/include -I$CONN/conninfra/include -DLIST_SEG_MAX=100"

build() {
  name=$1; dir=$2; shift 2
  echo "===== $name ====="
  ( cd "$K" && make O="$OUT" M="$dir" ARCH=arm64 TOP="$TOP" \
      MTK_PLATFORM=mt6853 KCFLAGS="$COMMON_INC" "$@" modules ) \
      > "$LOGD/$name.log" 2>&1
  rc=$?
  if [ $rc -eq 0 ]; then
    echo "[OK] $name"
  else
    echo "[FAIL] $name  (日志 $LOGD/$name.log)"
    tail -6 "$LOGD/$name.log"
  fi
  return $rc
}

case "${1:-wlan}" in
  wlan)
    build wlan "$CONN/wlan/core/gen4m" \
      MODULE_NAME=wlan_drv_gen4m MTK_COMBO_CHIP=SOC2_1X1 WLAN_CHIP_ID=6833 \
      CONFIG_MTK_COMBO_WIFI_HIF=axi MTK_ANDROID_WMT=y MTK_ANDROID_EMI=y \
      MTK_WLAN_SERVICE=yes WIFI_IP_SET=1 \
      OPLUS_FEATURE_WIFI_SMARTGEAR=yes OPLUS_FEATURE_TXPOWER_CTRL=yes
    ;;
  wmt)
    build wmt_chrdev_wifi "$CONN/wlan/adaptor" MODULE_NAME=wmt_chrdev_wifi \
      MTK_COMBO_CHIP=SOC2_1X1 WLAN_CHIP_ID=6833 CONFIG_MTK_COMBO_WIFI_HIF=axi \
      MTK_ANDROID_WMT=y
    ;;
  bt)
    build bt "$CONN/bt/mt66xx/wmt" BT_PLATFORM=connac1x MTK_ANDROID_WMT=y
    ;;
  wmtdrv)
    build wmt_drv "$CONN/common" MTK_ANDROID_WMT=y MTK_COMBO_CHIP=SOC2_1X1 WLAN_CHIP_ID=6833
    ;;
  connfem)
    build connfem "$CONN/connfem" MTK_COMBO_CHIP=SOC2_1X1
    ;;
  fmradio)
    build fmradio "$CONN/fmradio" MTK_COMBO_CHIP=SOC2_1X1
    ;;
  gps)
    build gps "$CONN/gps" MTK_COMBO_CHIP=SOC2_1X1
    ;;
  conninfra)
    build conninfra "$CONN/conninfra"
    ;;
  *)
    echo "usage: $0 {wlan|wmt|bt|connfem|fmradio|gps|conninfra}"
    exit 1
    ;;
esac
