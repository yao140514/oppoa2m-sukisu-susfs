#!/usr/bin/env python3
# 让 access("/system/bin/su") 直接返回成功，不依赖栈上临时字符串与真实文件
import re

K = '/home/ai/a2m/k13/kernel/'
p1 = K + 'drivers/kernelsu/feature/sucompat.c'
s = open(p1).read()

old = '''int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode, int *__unused_flags)
{
    if (!is_su_allowed(filename_user))
        return 0;

    ksu_sucompat_user_common(filename_user, "faccessat", false);
    return 0;
}'''

new = '''int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode, int *__unused_flags)
{
    char path[sizeof(su_path)] = { 0 };
    long len;

    if (!is_su_allowed(filename_user))
        return 0;

    /*
     * 返回 1 表示调用方直接认为 access 成功。
     * 本机 /system/bin/su 并不存在（仅靠 execve 钩子拦截），
     * 若按真实路径查找，PATH 搜索阶段就会被判为不可访问。
     */
    len = ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));
    if (len > 0 && !memcmp(path, su_path, sizeof(su_path))) {
        pr_info("faccessat: su -> allowed\\n");
        return 1;
    }

    ksu_sucompat_user_common(filename_user, "faccessat", false);
    return 0;
}'''

if 'faccessat: su -> allowed' in s:
    print('sucompat already patched')
else:
    assert old in s, 'faccessat anchor not found'
    s = s.replace(old, new, 1)
    open(p1, 'w').write(s)
    print('sucompat patched')

p2 = K + 'fs/open.c'
t = open(p2).read()
old2 = '''#ifdef CONFIG_KSU
	ksu_handle_faccessat(&dfd, &filename, &mode, NULL);
#endif'''
new2 = '''#ifdef CONFIG_KSU
	/* 返回非 0 表示这是 su 路径，直接视为可访问 */
	if (ksu_handle_faccessat(&dfd, &filename, &mode, NULL))
		return 0;
#endif'''
if '这是 su 路径' in t:
    print('open.c already patched')
else:
    assert old2 in t, 'open.c anchor not found'
    t = t.replace(old2, new2, 1)
    open(p2, 'w').write(t)
    print('open.c patched')
