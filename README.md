# OPPO A2m (MT6833) 自编译内核 · SukiSU Ultra + SUSFS v2.3.0

在 ColorOS 13.1（Android 13）的 **4.19.191** 内核上，把 SukiSU Ultra 与 SUSFS v2.3.0 跑起来，
并修掉了三个会导致「开不了机 / 管理器不认驱动 / WiFi 起不来」的坑。

非 GKI 机型，内核由 MTK 官方 4.19 源码重新编译，boot 分区重打包后刷入。

---

## 设备与环境

| 项目 | 值 |
|---|---|
| 机型 | OPPO A2m（PJU110） |
| SoC | MediaTek MT6833（Dimensity 700） |
| 内核 | 4.19.191+ / aarch64 / Non-GKI |
| 系统 | ColorOS 13.1.1（Android 13） |
| Root 方案 | SukiSU Ultra（内核态 KernelSU 分支） |
| 隐藏方案 | SUSFS v2.3.0（Inline Hook） |
| 分区 | A/B，boot_a |

---

## 已实现

- ✅ 开机正常，root 可用（`u:r:ksu:s0`）
- ✅ 管理器识别驱动，显示 `SuSFS 版本 v2.3.0（SUSFS Inline Hook）`
- ✅ Seccomp 已关闭，管理器能拿到驱动 fd
- ✅ WiFi / 蓝牙 / GPS / FM 模块正常加载
- ✅ 连接模块（a2m_connsys）随系统启动
- ❌ KPM 不可用（见「已知问题」）

---

## 关键技术点

### 1. SUSFS v2.3.0 ABI 适配（4.19 没有官方 v2 补丁）

SukiSU v4.2.1 的命令层用的是 **SUSFS v2.3.0 的 ABI**（`void susfs_xxx(void __user **)`
+ 结构体带 `err` 字段），而上游 susfs4ksu 给 4.19 的补丁只到 **v1.5.5**。
两版结构体布局不同（例如 `open_redirect` 在 v1.5.5 里前面多一个 `target_ino`），直接混用会读错偏移。

本项目的做法：

- **内核核心**保留 v1.5.5 的 4.19 原生实现（sus_path 的 inode 标记、sus_mount 的 mnt_id 伪造、
  sus_kstat 的 stat 伪造），这些是上游针对 4.19 写的，最稳定
- **新增 `fs/susfs_abi.c`** 做 ABI 翻译层，把 v2.3.0 的 16 条命令翻译成 v1.5.5 核心调用，
  结构体统一成 v2 布局，管理器看到的就是它期望的 v2.3.0
- **补齐 v2 独有能力**：`sus_map`、`avc_log_spoofing`、`enabled_features`、`variant`/`version`、
  `priority_uid_filter`、`sus_path_loop`

相关文件：`patches/susfs_v2.c`、`patches/susfs_v2.h`、`patches/susfs_v2_def.h`、`patches/susfs_abi.c`

### 2. 开机 1 秒崩溃：sucompat 编进了 5.x 的实现

**现象**：刷入后开机只振动一下，随即断电重启。

**定位**：pstore 里的崩溃栈

```
pc : ksu_handle_stat+0x20/0xb4
Call trace: ksu_handle_stat ← vfs_statx ← clean_path ← unpack_to_rootfs
```

反汇编对应指令是 `ldr x9,[x8]`，崩溃时 `x8 = 0x3730000000766564`（ASCII 就是 `"dev"`）——
代码把字符串内容当指针解引用了。

**根因**：`feature/sucompat.c` 里

```c
#ifdef CONFIG_KSU_SUSFS     /* ← 这套实现是给 5.x 写的 */
    ... (*filename)->name ...
```

打开 SUSFS 后，4.19 会编进这套 5.x 的实现，而 `vfs_statx` 传的是 `const char __user *`，
于是在解压 initramfs 阶段就崩了。

**修复**：把分支限定到 5.0+

```c
#if defined(CONFIG_KSU_SUSFS) && LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
```

并同步修 `sucompat.h` 的分流、4.19 分支的 sulog 参数类型、`adb_root` 接口改名。

### 3. 管理器「未检测到驱动」：setresuid 钩子缺失 + sid 恒为 0

**现象**：管理器首页显示「内核上未检测到 SukiSU 驱动程序」，logcat 里

```
KernelSU driver install syscall was blocked by seccomp
could not retrieve kernelsu driver fd
```

**根因有两层**：

1. `hook/lsm_hook.c` 在 SUSFS 模式下不注册 `task_fix_setuid`，改用 `ksu_handle_setresuid()`；
   但 4.19 的 `kernel/sys.c` **从未调用**它 —— zygote 孵化应用时既不关 seccomp，也不安装驱动 fd
2. 补上调用点后仍然不行：`susfs_zygote_sid` 在 4.19 恒为 0
   （`susfs_set_batch_sid()` 只在 5.10+ 的策略加载路径里被调用），sid 比对永远失败

**修复**：

- `__sys_setresuid()` 里调用 `ksu_handle_setresuid()`
- `ksu_handle_setresuid()` 首次进入时补设 sid（此时 SELinux 策略已加载）；
  仍失败则退化为按 uid 判定，保证管理器能关 seccomp 并安装 fd

修复后 dmesg 出现 `sid '76' is set for secctx_name 'u:r:zygote:s0'`，管理器随即恢复。

相关文件：`patches/fix_setresuid.py`、`patches/fix_sid_init.py`

### 4. WiFi 起不来：EMI 共享内存拿错了节点

**现象**：WiFi 开关是开的，但系统认为 disabled；`svc wifi enable` 无效。

**日志**：

```
wlanDownloadEMISection: (INIT ERROR) Consys emi memory address gConEmiPhyBaseFinal invalid
wlanAdapterStart: (INIT WARN) Fail reason: 5
wlanProbe: probe failed, reason:3
```

**根因**：MTK 的 EMI 共享内存分配错位。

开机日志显示真正给 WiFi 预留的 6MB 在 `wifi-reserve-memory`：

```
Reserved memory: created DMA memory pool at 0x00000000be000000, size 6 MiB
OF: reserved mem: initialized node wifi-reserve-memory
```

而 WMT 的 `wmt_allocate_connsys_emi()` 读的是 `consys@18002000` 的 `memory-region`，
指向 `consys-reserve-memory` —— 那个只有 **20KB**。于是 WiFi 驱动拿到的基址无效，固件写不进去。

**修复**：改 `gen4m/os/linux/gl_init.c` 的 `initWlan()`，在 WMT 没给出有效地址时，
直接用设备树里自己的预留区：

```c
wnode = of_find_compatible_node(NULL, NULL, "mediatek,wifi");
mnode = of_parse_phandle(wnode, "memory-region", 0);
rmem  = of_reserved_mem_lookup(mnode);
gConEmiPhyBaseFinal = rmem->base;
gConEmiSizeFinal    = rmem->size;
```

修复后的日志：

```
wlanProbe: probe success, feature set: 0x8488141a
WMT turn on WIFI success!
Wifi is connected to "CHH"
```

相关文件：`patches/fix_wlan_emi2.py`

---

## 已知问题

### KPM 不可用（`ksud kpm list` 返回 -95）

KSU 的 KPM 桥接依赖 KernelPatch 接管 `__arm64_sys_truncate`：

```c
fn = find_kernel_symbol_exact("__arm64_sys_truncate");
regs.regs[0] = SUKISU_KPM_SUPERCALL_MAGIC;
if (fn(&regs) != SUKISU_KPM_ABI_VERSION) return -EOPNOTSUPP;
```

`kptools -p` 注入显示 `patched=true`，但运行时探测仍返回错误，说明 KernelPatch 没有完成初始化。

已验证的事实：

- 升级 SukiSU 到 v4.3.0 **不解决**（其 `extension/kpm/kpm.c` 桥接与 v4.2.1 完全一致）
- `kpimg` 版本 `e00`（比官方 0.13.9 新）
- 内核 `CONFIG_KALLSYMS_ALL=y` 满足要求

最可疑的是 `CONFIG_RANDOMIZE_BASE=y`（KASLR）导致 `paging_init` hook 偏移对不上，
**待验证**方案是给 bootconfig 加 `nokaslr`。

---

## 目录说明

```
patches/   内核侧补丁与 SUSFS 实现
  susfs_v2.c            SUSFS v2.3.0 核心（改造自 v1.5.5 的 4.19 实现）
  susfs_v2.h            v2 结构体与接口声明
  susfs_v2_def.h        常量与特性位
  susfs_abi.c           v2 → v1.5.5 的 ABI 翻译层
  make_core.py          生成 susfs_v2.c 的改造脚本
  fix_setresuid.py      补 __sys_setresuid 钩子
  fix_sid_init.py       sid 延迟初始化 + uid 兜底
  fix_su_access.py      do_faccessat 对 su 路径直接返回成功
  fix_wlan_emi2.py      WiFi EMI 走 wifi-reserve-memory

scripts/   构建与刷写
  build13.sh            内核全量构建
  build_module.sh       MTK 连接模块编译（wlan/wmt/...）
  repack_boot.py        重打包 boot 镜像
  verify_boot.py        boot 镜像校验（ramdisk/dtb 比对）
  flash_dd.sh           通过 adb root + dd 刷 boot_a
  kp_patch3.sh          KernelPatch 注入

docs/移植记录.md        完整移植过程与踩坑记录

src/       完整改动源码（可直接覆盖到内核树，详见 src/README.md）
  kernel/drivers/kernelsu/   完整 SukiSU 驱动（含 4.19 适配修改）
  kernel/fs/                 SUSFS 核心与 ABI 翻译层
  kernel/include/linux/      susfs 头文件
  kernel-hooks/              改过的内核原生文件（stat / open / sys / exec）
  module/gl_init.c           wlan EMI 地址兜底
```

---

## 构建

```bash
# 内核
bash scripts/build13.sh build /tmp/build.log

# 连接模块（以 wlan 为例）
bash scripts/build_module.sh wlan
# 产物 104MB，需 strip 掉调试段
llvm-strip -g wlan_drv_gen4m.ko -o wlan_drv_gen4m_stripped.ko

# 重打包 boot
python scripts/repack_boot.py <原厂 boot.img> <Image.gz> - <输出.img>
python scripts/verify_boot.py <原厂 boot.img> <输出.img>
```

## 刷入与回滚

```bash
# 刷入（需要 adb root）
adb push boot_new.img /data/local/tmp/t.img
adb shell "dd if=/data/local/tmp/t.img of=/dev/block/by-name/boot_a bs=1M conv=fsync; reboot"

# 回滚（fastboot）
fastboot flash boot_a boot_original.img
fastboot reboot
```

---


## 预编译产物

boot 镜像发布在 [Releases](https://github.com/yao140514/oppoa2m-sukisu-susfs/releases)：

| 文件 | 说明 |
|---|---|
| `boot_susfs_v23.img` | 稳定版内核：SukiSU 4.2.1 + SUSFS v2.3.0，WiFi / 蓝牙 / GPS 正常 |
| `a2m_connsys_module_fixed.zip` | 连接模块包（wmt / wlan / bt / gps / fm / connfem），内含 EMI 修复后的 wlan |

连接模块用法：解压到 `/data/adb/modules/a2m_connsys/`，`chmod 755 *.sh`，重启。
也可以只替换 `wlan_drv_gen4m.ko`（仓库 `images/` 里有单独一份）后重载。

> 刷入前请先用原厂 boot 做备份。镜像与本仓库记录的设备（PJU110 / MT6833）绑定，不要刷到其他机型。

---|---|
| `boot_susfs_v23.img` | 稳定版内核：SukiSU 4.2.1 + SUSFS v2.3.0，WiFi/蓝牙正常 |
| `boot_susfs_kpm_v2.img` | 注入 KernelPatch 的版本（KPM 仍返回 -95，仅供参考） |
| `wlan_drv_gen4m.ko` | 修复 EMI 地址的 WiFi 模块，替换到 `/data/adb/modules/a2m_connsys/` 后重载 |

> 刷入前请先用原厂 boot 做备份。镜像与本仓库记录的设备（PJU110 / MT6833）绑定，不要刷到其他机型。

---

## 免责声明

本项目仅用于个人设备的内核研究与学习。刷机有风险，操作前请自行备份 boot 分区，
因使用本仓库内容造成的任何损失由使用者自行承担。

内核源码遵循 GPL-2.0。SUSFS 版权归 susfs4ksu 原作者，SukiSU 版权归 SukiSU-Ultra 团队。
