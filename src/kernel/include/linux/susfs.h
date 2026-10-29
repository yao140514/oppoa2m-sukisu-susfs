#ifndef KSU_SUSFS_H
#define KSU_SUSFS_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/hashtable.h>
#include <linux/path.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/workqueue.h>
#include <linux/susfs_def.h>

/*
 * 内核侧实现基于 SUSFS v1.5.5（4.19 原生补丁），
 * 对用户态暴露 SUSFS v2.3.0 的命令与结构体布局。
 */
#define SUSFS_VERSION "v2.3.0"
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define SUSFS_VARIANT "NON-GKI"
#else
#define SUSFS_VARIANT "GKI"
#endif

/*********/
/* MACRO */
/*********/
#define getname_safe(name) (name == NULL ? ERR_PTR(-EINVAL) : getname(name))
#define putname_safe(name) (IS_ERR(name) ? NULL : putname(name))

/********************************/
/* 用户态 ABI 结构体 (v2.3.0)   */
/********************************/
struct st_susfs_sus_path {
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                             err;
};

struct st_susfs_sus_path_hlist {
	unsigned long                   target_ino;
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	struct hlist_node               node;
};

struct st_susfs_sus_mount {
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long                   target_dev;
};

struct st_susfs_sus_mount_list {
	struct list_head                list;
	struct st_susfs_sus_mount       info;
};

struct st_susfs_hide_sus_mnts_for_non_su_procs {
	bool                            enabled;
	int                             err;
};

struct st_susfs_sus_kstat {
	bool                            is_statically;
	unsigned long                   target_ino;
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long                   spoofed_ino;
	unsigned long                   spoofed_dev;
	unsigned int                    spoofed_nlink;
	long long                       spoofed_size;
	long                            spoofed_atime_tv_sec;
	unsigned long                   spoofed_atime_tv_nsec;
	long                            spoofed_mtime_tv_sec;
	unsigned long                   spoofed_mtime_tv_nsec;
	long                            spoofed_ctime_tv_sec;
	unsigned long                   spoofed_ctime_tv_nsec;
	long long                       spoofed_blocks;
	long                            spoofed_blksize;
	int                             flags;
	int                             err;
};

struct st_susfs_sus_kstat_hlist {
	unsigned long                   target_ino;
	struct st_susfs_sus_kstat       info;
	struct hlist_node               node;
};

struct st_susfs_try_umount {
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                             mnt_mode;
};

struct st_susfs_try_umount_list {
	struct list_head                list;
	struct st_susfs_try_umount      info;
};

struct st_susfs_uname {
	char                            release[__NEW_UTS_LEN+1];
	char                            version[__NEW_UTS_LEN+1];
	int                             err;
};

struct st_susfs_log {
	bool                            enabled;
	int                             err;
};

struct st_susfs_spoof_cmdline_or_bootconfig {
	char                            fake_cmdline_or_bootconfig[SUSFS_V2_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE];
	int                             err;
};

struct st_susfs_open_redirect {
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	char                            redirected_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                             uid_scheme;
	int                             err;
};

struct st_susfs_open_redirect_hlist {
	struct hlist_node               node;
	unsigned long                   target_ino;
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	char                            redirected_pathname[SUSFS_MAX_LEN_PATHNAME];
};

struct st_susfs_sus_map {
	char                            target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                             err;
};

struct st_susfs_priority_uid_filter {
	char                            app_strings[SUSFS_APP_STR_BUF_SIZE];
	int                             err;
};

struct st_susfs_avc_log_spoofing {
	bool                            enabled;
	int                             err;
};

struct st_susfs_enabled_features {
	char                            enabled_features[SUSFS_ENABLED_FEATURES_SIZE];
	int                             err;
};

struct st_susfs_variant {
	char                            susfs_variant[SUSFS_MAX_VARIANT_BUFSIZE];
	int                             err;
};

struct st_susfs_version {
	char                            susfs_version[SUSFS_MAX_VERSION_BUFSIZE];
	int                             err;
};

struct st_sus_su {
	int                             mode;
};

/******************************/
/* 内核核心实现（v1.5.5 模型）*/
/******************************/
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
int susfs_core_add_sus_path(struct st_susfs_sus_path *info_in);
int susfs_sus_ino_for_filldir64(unsigned long ino);
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
int susfs_add_sus_mount(struct st_susfs_sus_mount* __user user_info);
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT
int susfs_auto_add_sus_bind_mount(const char *pathname, struct path *path_target);
#endif
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT
void susfs_auto_add_sus_ksu_default_mount(const char __user *to_pathname);
#endif
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
int susfs_core_add_sus_kstat(struct st_susfs_sus_kstat *info_in);
int susfs_core_update_sus_kstat(struct st_susfs_sus_kstat *info_in);
void susfs_sus_ino_for_generic_fillattr(unsigned long ino, struct kstat *stat);
void susfs_sus_ino_for_show_map_vma(unsigned long ino, dev_t *out_dev, unsigned long *out_ino);
#endif

#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
int susfs_add_try_umount(struct st_susfs_try_umount* __user user_info);
void susfs_try_umount(uid_t target_uid);
void susfs_try_umount_all(uid_t target_uid);
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT
void susfs_auto_add_try_umount_for_bind_mount(struct path *path);
#endif
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
int susfs_core_set_uname(struct st_susfs_uname *info_in);
void susfs_spoof_uname(struct new_utsname* tmp);
#endif

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_set_log(bool enabled);
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
int susfs_core_set_cmdline_or_bootconfig(const char *fake);
int susfs_spoof_cmdline_or_bootconfig(struct seq_file *m);
#endif

#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
int susfs_core_add_open_redirect(struct st_susfs_open_redirect *info_in);
struct filename* susfs_get_redirected_path(unsigned long ino);
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_SU
int susfs_get_sus_su_working_mode(void);
int susfs_sus_su(struct st_sus_su* __user user_info);
#endif

void susfs_init(void);

extern struct work_struct susfs_extra_works;
void susfs_extra_work_fn(struct work_struct *work);

/* 由 4.19 挂载补丁在别处实现 */
bool susfs_is_mnt_devname_ksu(struct path *path);
void susfs_run_try_umount_for_current_mnt_ns(void);

/********************************/
/* 用户态命令入口 (v2.3.0 ABI)  */
/********************************/
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
void susfs_add_sus_path(void __user **user_info);
void susfs_add_sus_path_loop(void __user **user_info);
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
void susfs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info);
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
void susfs_add_sus_kstat(void __user **user_info);
void susfs_update_sus_kstat(void __user **user_info);
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
void susfs_set_uname(void __user **user_info);
#endif

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_enable_log(void __user **user_info);
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
void susfs_set_cmdline_or_bootconfig(void __user **user_info);
#endif

#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
void susfs_add_open_redirect(void __user **user_info);
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MAP
void susfs_add_sus_map(void __user **user_info);
bool susfs_is_inode_sus_map(struct inode *inode);
#endif

void susfs_set_priority_uid_filter(void __user **user_info);
bool susfs_is_priority_uid_filtered(uid_t uid);
void susfs_parse_priority_app_strings(void);

void susfs_set_avc_log_spoofing(void __user **user_info);
bool susfs_is_avc_log_spoofing_enabled(void);

void susfs_get_enabled_features(void __user **user_info);
void susfs_show_variant(void __user **user_info);
void susfs_show_version(void __user **user_info);

void susfs_start_sdcard_monitor_fn(void);

#endif // #ifndef KSU_SUSFS_H
