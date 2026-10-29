#ifndef KSU_SUSFS_DEF_H
#define KSU_SUSFS_DEF_H

#include <linux/bits.h>
#include <linux/string.h>
#include <linux/uidgid.h>
#include <linux/sched.h>

/********/
/* ENUM */
/********/
/* shared with userspace ksu_susfs tool (SUSFS v2.3.0 编号) */
#define SUSFS_MAGIC 0xFAFAFAFA
#define CMD_SUSFS_ADD_SUS_PATH 0x55550
#define CMD_SUSFS_ADD_SUS_PATH_LOOP 0x55553
#define CMD_SUSFS_ADD_SUS_MOUNT 0x55560
#define CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS 0x55561
#define CMD_SUSFS_ADD_SUS_KSTAT 0x55570
#define CMD_SUSFS_UPDATE_SUS_KSTAT 0x55571
#define CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY 0x55572
#define CMD_SUSFS_ADD_TRY_UMOUNT 0x55580
#define CMD_SUSFS_SET_UNAME 0x55590
#define CMD_SUSFS_ENABLE_LOG 0x555a0
#define CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG 0x555b0
#define CMD_SUSFS_ADD_OPEN_REDIRECT 0x555c0
#define CMD_SUSFS_RUN_UMOUNT_FOR_CURRENT_MNT_NS 0x555d0
#define CMD_SUSFS_SHOW_VERSION 0x555e1
#define CMD_SUSFS_SHOW_ENABLED_FEATURES 0x555e2
#define CMD_SUSFS_SHOW_VARIANT 0x555e3
#define CMD_SUSFS_SHOW_SUS_SU_WORKING_MODE 0x555e4
#define CMD_SUSFS_IS_SUS_SU_READY 0x555f0
#define CMD_SUSFS_SUS_SU 0x60000
#define CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING 0x60010
#define CMD_SUSFS_ADD_SUS_MAP 0x60020
#define CMD_SUSFS_SET_PRIORITY_UID_FILTER 0x60030

#define SUSFS_MAX_LEN_PATHNAME 256
#define SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE 4096
/* v2 用户态结构体里该缓冲区是 8192，内核只保留前 4096 */
#define SUSFS_V2_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE 8192
#define SUSFS_ENABLED_FEATURES_SIZE 8192
#define SUSFS_APP_STR_BUF_SIZE 65535

/* getpriority uid 过滤范围（应用 uid 段） */
#define SUSFS_PRIORITY_UID_FILTER_MIN_UID 10000
#define SUSFS_PRIORITY_UID_FILTER_MAX_UID 19999
#define SUSFS_PRIORITY_UID_FILTER_UID_NUM \
	(SUSFS_PRIORITY_UID_FILTER_MAX_UID - SUSFS_PRIORITY_UID_FILTER_MIN_UID + 1)
#define SUSFS_PRIORITY_UID_FILTER_BITMAP_LEN \
	BITS_TO_LONGS(SUSFS_PRIORITY_UID_FILTER_UID_NUM)
#define SUSFS_MAX_VERSION_BUFSIZE 16
#define SUSFS_MAX_VARIANT_BUFSIZE 16

#define TRY_UMOUNT_DEFAULT 0
#define TRY_UMOUNT_DETACH 1

#define SUS_SU_DISABLED 0
#define SUS_SU_WITH_OVERLAY 1
#define SUS_SU_WITH_HOOKS 2

#define DEFAULT_SUS_MNT_ID 100000
#define DEFAULT_SUS_MNT_ID_FOR_KSU_PROC_UNSHARE 1000000
#define DEFAULT_SUS_MNT_GROUP_ID 1000

/*
 * inode->i_state => 标记位
 * mount->mnt.susfs_mnt_id_backup => 原始 mnt_id
 * task_struct->susfs_last_fake_mnt_id => 伪造 mnt_id 游标
 */
#define INODE_STATE_SUS_PATH BIT(24)
#define INODE_STATE_SUS_MOUNT BIT(25)
#define INODE_STATE_SUS_KSTAT BIT(26)
#define INODE_STATE_OPEN_REDIRECT BIT(27)
#define INODE_STATE_SUS_MAP BIT(28)

#define TASK_STRUCT_NON_ROOT_USER_APP_PROC BIT(24)

#define MAGIC_MOUNT_WORKDIR "/debug_ramdisk/workdir"
#define DATA_ADB_UMOUNT_FOR_ZYGOTE_SYSTEM_PROCESS "/data/adb/susfs_umount_for_zygote_system_process"
#define DATA_ADB_NO_AUTO_ADD_SUS_BIND_MOUNT "/data/adb/susfs_no_auto_add_sus_bind_mount"
#define DATA_ADB_NO_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT "/data/adb/susfs_no_auto_add_sus_ksu_default_mount"
#define DATA_ADB_NO_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT "/data/adb/susfs_no_auto_add_try_umount_for_bind_mount"

/* 应用进程判定：uid 落在 10000~99999 区段（定义在 fs/susfs.c） */
extern bool susfs_is_current_app_uid(void);

/*
 * v2 用线程 TIF 位记录 unshare / no_su 状态，4.19 侧由 uid 动态判定，
 * 这里保留空实现以兼容 KernelSU 的调用点。
 */
static inline void susfs_set_current_proc_umounted(void) { }
static inline void susfs_clear_current_proc_umounted(void) { }
static inline void susfs_set_current_proc_umounted_for_zygote_next(void) { }
static inline void susfs_clear_current_proc_umounted_for_zygote_next(void) { }
static inline void susfs_set_current_proc_no_su(void) { }
static inline void susfs_clear_current_proc_no_su(void) { }

static inline bool susfs_starts_with(const char *str, const char *prefix)
{
	while (*prefix) {
		if (*str++ != *prefix++)
			return false;
	}
	return true;
}

static inline bool susfs_ends_with(const char *str, const char *suffix)
{
	size_t str_len, suffix_len;

	if (!str || !suffix)
		return false;
	str_len = strlen(str);
	suffix_len = strlen(suffix);
	if (suffix_len > str_len)
		return false;
	return !strcmp(str + str_len - suffix_len, suffix);
}

#endif // #ifndef KSU_SUSFS_DEF_H
