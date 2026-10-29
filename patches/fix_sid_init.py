#!/usr/bin/env python3
# 4.19 上 apply_kernelsu_rules 走的是另一条分支，susfs_set_batch_sid() 不会被调用，
# 导致 susfs_zygote_sid 恒为 0，ksu_handle_setresuid 永远提前返回。
# 这里做两件事：
#   1) 首次进入时补设 sid（此时 SELinux 策略已加载）
#   2) 若仍失败，退化为按 uid 判定，至少保证管理器能关 seccomp 并拿到 fd
import sys

p = '/home/ai/a2m/k13/kernel/drivers/kernelsu/hook/lsm_hook.c'
s = open(p).read()

if 'install fd for manager(fallback)' in s:
    print('already patched')
    sys.exit(0)

# 补一个 extern 声明
old_decl = 'extern void disable_seccomp(void);'
new_decl = 'extern void disable_seccomp(void);\nextern void susfs_set_batch_sid(void);'
assert old_decl in s, 'decl anchor missing'
s = s.replace(old_decl, new_decl, 1)

old_body = '''int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    uid_t cur_uid = current_uid().val;

    if (cur_uid != 0)
        return 0;

    // We only interest in process spwaned by zygote or zygote_next
    if (susfs_is_sid_equal(current_cred(), susfs_zygote_sid))
        return handle_zygote_setresuid(ruid);'''

new_body = '''int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    uid_t cur_uid = current_uid().val;

    if (cur_uid != 0)
        return 0;

    /* 4.19 的规则加载路径不会调用 susfs_set_batch_sid，这里延迟补设一次 */
    if (unlikely(susfs_zygote_sid == 0))
        susfs_set_batch_sid();

    /* 仍然拿不到 sid 时退化为按 uid 判定，保证管理器可用 */
    if (unlikely(susfs_zygote_sid == 0)) {
        if (likely(ksu_is_manager_appid_valid()) && unlikely(is_uid_manager(ruid))) {
            disable_seccomp();
            pr_info("install fd for manager(fallback): %d\\n", ruid);
            ksu_install_fd();
        }
        return 0;
    }

    // We only interest in process spwaned by zygote or zygote_next
    if (susfs_is_sid_equal(current_cred(), susfs_zygote_sid))
        return handle_zygote_setresuid(ruid);'''

assert old_body in s, 'body anchor missing'
s = s.replace(old_body, new_body, 1)
open(p, 'w').write(s)
print('lsm_hook patched')
