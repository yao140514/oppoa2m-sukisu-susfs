# 源码说明

这里放的是本项目的**完整改动源码**，配合上游原始源码使用。

完整内核树 1.4GB、MTK 连接模块源码 549MB，绝大部分是 MTK / OPPO 原厂代码，
不适合放进仓库，所以只提交改动部分 —— 但提交的每个文件都是**完整文件**，不是 diff。

## 目录

### kernel/drivers/kernelsu/

完整的 SukiSU 内核驱动（v4.2.1），已包含本项目的 4.19 适配修改：

- `feature/sucompat.c` 的 SUSFS 分支限定到 5.0+（修开机崩溃）
- `feature/sucompat.c` 的 execveat 不再提前 return
- `hook/lsm_hook.c` 的 sid 延迟初始化 + uid 兜底
- `extension/kpm/` 的 KPM 桥接

直接覆盖内核树的 `drivers/kernelsu/` 即可。

### kernel/fs/

- `susfs.c` —— SUSFS v2.3.0 核心（基于 v1.5.5 的 4.19 实现改造）
- `susfs_abi.c` —— v2.3.0 命令到 v1.5.5 核心的 ABI 翻译层

放到内核树 `fs/` 下，并在 `fs/Makefile` 里加 `obj-$(CONFIG_KSU_SUSFS) += susfs.o susfs_abi.o`。

### kernel/include/linux/

- `susfs.h` —— 对外接口
- `susfs_def.h` —— 常量与结构体定义

放到 `include/linux/`。

### kernel-hooks/

改过的内核原生文件，完整文件形式便于对照：

| 文件 | 改动 |
|---|---|
| `fs/stat.c` | `vfs_statx()` 顶部加 `ksu_handle_stat()` 调用 |
| `fs/open.c` | `do_faccessat()` 顶部加 `ksu_handle_faccessat()` 调用，返回非 0 时直接 `return 0` |
| `kernel/sys.c` | `__sys_setresuid()` 里加 `ksu_handle_setresuid()` 调用 |
| `fs/exec.c` | `do_execveat_common()` 里加 `ksu_handle_execveat()` 调用 |

### module/

- `gl_init.c` —— MTK wlan gen4m 的 `os/linux/gl_init.c`，在 `initWlan()` 里加了 EMI 地址兜底：
  WMT 没给出有效 `gConEmiPhyBase` 时，直接用设备树 `wifi-reserve-memory` 的基址。

## 复现步骤

1. 取 MTK 4.19.191 内核源码（本机为 OPPO A2m / MT6833，`k6833v1_64_k419`）
2. 按上面说明覆盖 `drivers/kernelsu/`、`fs/`、`include/linux/`，并替换 `kernel-hooks/` 里的四个文件
3. 内核 `.config` 打开：

```
CONFIG_KSU=y
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_OPEN_REDIRECT=y
CONFIG_KSU_SUSFS_SUS_MAP=y
CONFIG_KPM=y
```

4. 编译：

```bash
make O=out ARCH=arm64 k6833v1_64_k419_defconfig
make O=out ARCH=arm64 -j$(nproc) Image.gz
```

5. 连接模块用 `scripts/build_module.sh` 编译，产物需 `llvm-strip -g` 去调试段

## 未包含

- 完整内核源码树（1.4GB，MTK 原厂）
- 完整连接模块源码（549MB，MTK 原厂）
- 原厂 boot 镜像（请自行从设备备份）

需要完整源码树的，可以按上面的路径从对应机型的分支自行获取，再套用本目录的改动。
