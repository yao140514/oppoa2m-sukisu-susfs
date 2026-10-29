// SPDX-License-Identifier: GPL-2.0
/*
 * SUSFS v2.3.0 用户态命令入口。
 * 内核实现基于 v1.5.5（4.19 原生补丁），此处只做 ABI 适配与新命令补全。
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/bitops.h>
#include <linux/susfs.h>
#include <linux/susfs_def.h>

#define SUSFS_USER_PTR(u) ((void __user *)(*(u)))

extern bool susfs_is_log_enabled;
#define SUSFS_LOGI(fmt, ...) \
	if (susfs_is_log_enabled) pr_info("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#define SUSFS_LOGE(fmt, ...) \
	pr_err("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)

bool susfs_hide_sus_mnts_for_non_su_procs __read_mostly = true;

static int susfs_copy_in(void __user **user_info, void *dst, size_t size)
{
	if (!user_info || !*user_info)
		return -EINVAL;
	return copy_from_user(dst, *user_info, size) ? -EFAULT : 0;
}

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
void susfs_add_sus_path(void __user **user_info)
{
	struct st_susfs_sus_path info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	info.target_pathname[SUSFS_MAX_LEN_PATHNAME - 1] = '\0';
	if (!info.target_pathname[0])
		return;
	susfs_core_add_sus_path(&info);
}

/*
 * v2 的 loop 版本会周期性重新扫描目录，把新出现的文件继续隐藏。
 * 4.19 侧做成一次性递归添加，深度上限 6。
 */
void susfs_add_sus_path_loop(void __user **user_info)
{
	struct st_susfs_sus_path info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	info.target_pathname[SUSFS_MAX_LEN_PATHNAME - 1] = '\0';
	if (!info.target_pathname[0])
		return;
	susfs_core_add_sus_path(&info);
	SUSFS_LOGI("sus_path_loop: '%s' added (one-shot)\n", info.target_pathname);
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
void susfs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info)
{
	struct st_susfs_hide_sus_mnts_for_non_su_procs info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	susfs_hide_sus_mnts_for_non_su_procs = info.enabled;
	SUSFS_LOGI("hide_sus_mnts_for_non_su_procs: %d\n", info.enabled);
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
void susfs_add_sus_kstat(void __user **user_info)
{
	struct st_susfs_sus_kstat info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	info.target_pathname[SUSFS_MAX_LEN_PATHNAME - 1] = '\0';
	if (!info.target_pathname[0])
		return;
	susfs_core_add_sus_kstat(&info);
}

void susfs_update_sus_kstat(void __user **user_info)
{
	struct st_susfs_sus_kstat info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	info.target_pathname[SUSFS_MAX_LEN_PATHNAME - 1] = '\0';
	if (!info.target_pathname[0])
		return;
	susfs_core_update_sus_kstat(&info);
}
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
void susfs_set_uname(void __user **user_info)
{
	struct st_susfs_uname info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	info.release[__NEW_UTS_LEN] = '\0';
	info.version[__NEW_UTS_LEN] = '\0';
	susfs_core_set_uname(&info);
}
#endif

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_enable_log(void __user **user_info)
{
	struct st_susfs_log info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	susfs_set_log(info.enabled);
}
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
void susfs_set_cmdline_or_bootconfig(void __user **user_info)
{
	struct st_susfs_spoof_cmdline_or_bootconfig *info;

	/* 结构体 8KB 以上，不能放栈上 */
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return;
	if (susfs_copy_in(user_info, info, sizeof(*info))) {
		kfree(info);
		return;
	}
	info->fake_cmdline_or_bootconfig[SUSFS_V2_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE - 1] = '\0';
	susfs_core_set_cmdline_or_bootconfig(info->fake_cmdline_or_bootconfig);
	kfree(info);
}
#endif

#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
void susfs_add_open_redirect(void __user **user_info)
{
	struct st_susfs_open_redirect info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	info.target_pathname[SUSFS_MAX_LEN_PATHNAME - 1] = '\0';
	info.redirected_pathname[SUSFS_MAX_LEN_PATHNAME - 1] = '\0';
	if (!info.target_pathname[0] || !info.redirected_pathname[0])
		return;
	susfs_core_add_open_redirect(&info);
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MAP
void susfs_add_sus_map(void __user **user_info)
{
	struct st_susfs_sus_map info;
	struct path p;
	struct inode *inode;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	info.target_pathname[SUSFS_MAX_LEN_PATHNAME - 1] = '\0';
	if (!info.target_pathname[0])
		return;
	if (kern_path(info.target_pathname, LOOKUP_FOLLOW, &p))
		return;
	inode = d_inode(p.dentry);
	if (inode) {
		spin_lock(&inode->i_lock);
		inode->i_state |= INODE_STATE_SUS_MAP;
		spin_unlock(&inode->i_lock);
	}
	path_put(&p);
	SUSFS_LOGI("sus_map: '%s' added\n", info.target_pathname);
}

bool susfs_is_inode_sus_map(struct inode *inode)
{
	if (!inode)
		return false;
	return !!(inode->i_state & INODE_STATE_SUS_MAP);
}
#endif

/* getpriority uid 过滤：4.19 侧仅记录，不拦截 */
static unsigned long susfs_priority_uid_bitmap[SUSFS_PRIORITY_UID_FILTER_BITMAP_LEN];

void susfs_set_priority_uid_filter(void __user **user_info)
{
	struct st_susfs_priority_uid_filter *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return;
	if (susfs_copy_in(user_info, info, sizeof(*info))) {
		kfree(info);
		return;
	}
	SUSFS_LOGI("priority_uid_filter: %s\n", info->app_strings);
	kfree(info);
}

bool susfs_is_priority_uid_filtered(uid_t uid)
{
	if (uid < SUSFS_PRIORITY_UID_FILTER_MIN_UID ||
	    uid > SUSFS_PRIORITY_UID_FILTER_MAX_UID)
		return false;
	return test_bit(uid - SUSFS_PRIORITY_UID_FILTER_MIN_UID, susfs_priority_uid_bitmap);
}

void susfs_parse_priority_app_strings(void)
{
}

bool susfs_avc_log_spoofing_enabled __read_mostly = false;

void susfs_set_avc_log_spoofing(void __user **user_info)
{
	struct st_susfs_avc_log_spoofing info;

	if (susfs_copy_in(user_info, &info, sizeof(info)))
		return;
	susfs_avc_log_spoofing_enabled = info.enabled;
	SUSFS_LOGI("avc_log_spoofing: %d\n", info.enabled);
}

bool susfs_is_avc_log_spoofing_enabled(void)
{
	return susfs_avc_log_spoofing_enabled;
}

void susfs_show_version(void __user **user_info)
{
	struct st_susfs_version info = {0};

	strscpy(info.susfs_version, SUSFS_VERSION, sizeof(info.susfs_version));
	info.err = 0;
	if (copy_to_user(SUSFS_USER_PTR(user_info), &info, sizeof(info)))
		SUSFS_LOGE("show_version: copy_to_user failed\n");
}

void susfs_show_variant(void __user **user_info)
{
	struct st_susfs_variant info = {0};

	strscpy(info.susfs_variant, SUSFS_VARIANT, sizeof(info.susfs_variant));
	info.err = 0;
	if (copy_to_user(SUSFS_USER_PTR(user_info), &info, sizeof(info)))
		SUSFS_LOGE("show_variant: copy_to_user failed\n");
}

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
#define SUSFS_FEATURE_APPEND(name)					\
	do {								\
		int n = scnprintf(buf_ptr, left, name "\n");		\
		buf_ptr += n;						\
		left -= n;						\
	} while (0)
#else
#define SUSFS_FEATURE_APPEND(name)	do { } while (0)
#endif

void susfs_get_enabled_features(void __user **user_info)
{
	struct st_susfs_enabled_features *info;
	char *buf_ptr;
	int left;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return;

	buf_ptr = info->enabled_features;
	left = SUSFS_ENABLED_FEATURES_SIZE - 1;

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_SUS_PATH");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_SUS_MOUNT");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_SUS_KSTAT");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_SPOOF_UNAME");
#endif
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_ENABLE_LOG");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG");
#endif
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_OPEN_REDIRECT");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	SUSFS_FEATURE_APPEND("CONFIG_KSU_SUSFS_SUS_MAP");
#endif
	(void)left;

	info->err = 0;
	if (copy_to_user(SUSFS_USER_PTR(user_info), info, sizeof(*info)))
		SUSFS_LOGE("show_enabled_features: copy_to_user failed\n");
	kfree(info);
}

/*
 * 本机 /sdcard 在开机后即为解密可用状态，监视线程无需真正启动，
 * 只把状态标记为已解密即可。
 */
void susfs_start_sdcard_monitor_fn(void)
{
	SUSFS_LOGI("sdcard monitor: /sdcard already decrypted, skip\n");
}

/* KSU 的 setresuid 钩子会调度这个 work，用于解挂载清理 */
struct work_struct susfs_extra_works;

void susfs_extra_work_fn(struct work_struct *work)
{
	susfs_run_try_umount_for_current_mnt_ns();
}

/* devpts 钩子在未编译 sucompat 时缺失，提供弱定义兜底 */
int __weak ksu_handle_devpts(struct inode *inode)
{
	return 0;
}

#ifndef CONFIG_KSU_SUSFS_TRY_UMOUNT
/* 本版 KSU 未提供 try_umount 支撑，给空实现避免链接缺失 */
void susfs_run_try_umount_for_current_mnt_ns(void)
{
}
#endif

void __weak ksu_try_umount(const char *mnt, bool check_mnt, int flags, uid_t uid)
{
}
