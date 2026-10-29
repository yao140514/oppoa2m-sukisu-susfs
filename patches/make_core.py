import re

src = open('D:/oppoa2m/work/susfs_k419.c', encoding='utf-8').read()
s = src

# ---------- 1. 签名与局部变量 ----------
converts = [
    ('susfs_add_sus_path', 'susfs_core_add_sus_path', 'struct st_susfs_sus_path'),
    ('susfs_add_sus_kstat', 'susfs_core_add_sus_kstat', 'struct st_susfs_sus_kstat'),
    ('susfs_update_sus_kstat', 'susfs_core_update_sus_kstat', 'struct st_susfs_sus_kstat'),
    ('susfs_set_uname', 'susfs_core_set_uname', 'struct st_susfs_uname'),
    ('susfs_add_open_redirect', 'susfs_core_add_open_redirect', 'struct st_susfs_open_redirect'),
]
for old, new, st in converts:
    o = f'int {old}({st}* __user user_info) {{'
    assert o in s, o
    s = s.replace(o, f'int {new}({st} *info_in) {{', 1)

for old, new, st in converts:
    o = f'\t{st} info;\n'
    # 只改新函数体内的声明：在其签名之后首次出现
    idx = s.index(f'int {new}({st} *info_in) {{')
    j = s.index(o, idx)
    s = s[:j] + f'\t{st} info = *info_in;\n' + s[j+len(o):]

# ---------- 2. 移除已转换函数里的 copy_from_user 块 ----------
def strip_copy_blocks(text):
    out = []
    pos = 0
    for m in re.finditer(r'int susfs_core_\w+\([^)]*\) \{\n', text):
        start = m.start()
        end = text.find('\n}\n', m.end())
        if end < 0:
            continue
        body = text[m.end():end]
        body2 = re.sub(
            r'\n\tif \(copy_from_user\(&info, user_info, [^\n]*\)\) \{\n'
            r'\t\tSUSFS_LOGE\("failed copying from userspace[^\n]*\n'
            r'\t\treturn 1;\n\t\}\n', '\n', body, count=1)
        out.append((m.end(), end, body2))
    for a, b, repl in reversed(out):
        text = text[:a] + repl + text[b:]
    return text

s = strip_copy_blocks(s)

# ---------- 3. ino 由路径自行解析 ----------
anchor = 'static int susfs_update_sus_path_inode(char *target_pathname) {'
assert anchor in s
helper = '''static unsigned long susfs_get_ino_by_path(const char *path)
{
	struct path p;
	unsigned long ino = 0;

	if (!kern_path(path, LOOKUP_FOLLOW, &p)) {
		if (d_inode(p.dentry))
			ino = d_inode(p.dentry)->i_ino;
		path_put(&p);
	}
	return ino;
}

'''
s = s.replace(anchor, helper + anchor, 1)

# sus_path：从路径取 ino
o = '''	new_entry->target_ino = info.target_ino;
	strncpy(new_entry->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME-1);
	if (susfs_update_sus_path_inode(new_entry->target_pathname)) {'''
assert o in s
s = s.replace(o, o.replace('info.target_ino', 'susfs_get_ino_by_path(info.target_pathname)'), 1)
o = 'hash_add(SUS_PATH_HLIST, &new_entry->node, info.target_ino);'
assert o in s
s = s.replace(o, 'hash_add(SUS_PATH_HLIST, &new_entry->node, new_entry->target_ino);', 1)

# open_redirect：从路径取 ino
o = 'new_entry->target_ino = info.target_ino;\n\tstrncpy(new_entry->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME-1);\n\tstrncpy(new_entry->redirected_pathname'
assert o in s
s = s.replace(o, 'new_entry->target_ino = susfs_get_ino_by_path(info.target_pathname);\n\tstrncpy(new_entry->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME-1);\n\tstrncpy(new_entry->redirected_pathname', 1)
o = 'hash_add(OPEN_REDIRECT_HLIST, &new_entry->node, info.target_ino);'
assert o in s
s = s.replace(o, 'hash_add(OPEN_REDIRECT_HLIST, &new_entry->node, new_entry->target_ino);', 1)

# ---------- 4. cmdline：改为接收内核字符串 ----------
o = 'int susfs_set_cmdline_or_bootconfig(char* __user user_fake_cmdline_or_bootconfig) {'
assert o in s
s = s.replace(o, 'int susfs_core_set_cmdline_or_bootconfig(const char *fake_cmdline) {', 1)
o = 'res = strncpy_from_user(fake_cmdline_or_bootconfig, user_fake_cmdline_or_bootconfig, SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE-1);'
assert o in s
s = s.replace(o, 'res = strscpy(fake_cmdline_or_bootconfig, fake_cmdline, SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE-1);', 1)

# ---------- 4.5 应用进程判定函数 ----------
o = 'int susfs_core_add_sus_path(struct st_susfs_sus_path *info_in) {'
assert o in s
s = s.replace(o, '''bool susfs_is_current_app_uid(void)
{
	return ((current_uid().val % 100000) >= 10000);
}

''' + o, 1)

# ---------- 4.6 额外工作队列初始化 ----------
NL = chr(10)
o = 'void susfs_init(void) {' + NL
assert o in s
s = s.replace(o, o + '	INIT_WORK(&susfs_extra_works, susfs_extra_work_fn);' + NL, 1)

# ---------- 5. 剩余 user_info 引用检查 ----------
left = [l for l in s.split('\n') if 'user_info' in l and 'susfs_core' not in l]
print('残留 user_info 行数:', len(left))
for l in left[:5]:
    print('  ', l.strip()[:100])

open('D:/oppoa2m/work/susfs_v2.c', 'w', encoding='utf-8', newline='\n').write(s)
print('写出 susfs_v2.c, 行数:', s.count('\n'))
