#!/usr/bin/env python3
# 4.19 + SUSFS 模式下 task_fix_setuid 被条件编译排除，
# 需要按上游做法在 __sys_setresuid 里调用 ksu_handle_setresuid，
# 否则 zygote 孵化应用时不会关闭 seccomp，管理器也拿不到驱动 fd。
import sys

p = '/home/ai/a2m/k13/kernel/kernel/sys.c'
s = open(p).read()

anchor = '''long __sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	struct user_namespace *ns = current_user_ns();'''

if 'ksu_handle_setresuid' in s:
    print('already patched')
    sys.exit(0)

new = '''long __sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
#if defined(CONFIG_KSU) && defined(CONFIG_KSU_SUSFS)
	extern int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid);
#endif
	struct user_namespace *ns = current_user_ns();'''

assert anchor in s, 'anchor not found'
s = s.replace(anchor, new, 1)

# 在声明之后、逻辑之前调用
call_anchor = '''	kuid_t kruid, keuid, ksuid;

	kruid = make_kuid(ns, ruid);'''
call_new = '''	kuid_t kruid, keuid, ksuid;

#if defined(CONFIG_KSU) && defined(CONFIG_KSU_SUSFS)
	/* zygote 在 fork 出应用时走这里：关闭 seccomp 并为管理器安装驱动 fd */
	ksu_handle_setresuid(ruid, euid, suid);
#endif

	kruid = make_kuid(ns, ruid);'''
assert call_anchor in s, 'call anchor not found'
s = s.replace(call_anchor, call_new, 1)

open(p, 'w').write(s)
print('sys.c patched')
