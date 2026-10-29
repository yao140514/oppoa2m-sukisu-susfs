#!/usr/bin/env python3
"""校验新 boot 镜像：内核版本串、ramdisk/dtb 是否与原厂一致、KSU 痕迹。"""
import gzip
import sys

sys.path.insert(0, '/mnt/d/oppoa2m/work')
from repack_boot import parse, avb_info

orig_path, new_path = sys.argv[1], sys.argv[2]
od = open(orig_path, 'rb').read()
nd = open(new_path, 'rb').read()
oh, ok, oram, osec, odtb = parse(od)
nh, nk, nram, nsec, ndtb = parse(nd)

print('原厂: kernel=%d ramdisk=%d dtb=%d' % (len(ok), len(oram), len(odtb)))
print('新镜像: kernel=%d ramdisk=%d dtb=%d' % (len(nk), len(nram), len(ndtb)))
print('ramdisk 与原厂一致:', nram == oram)
print('dtb 与原厂一致:', ndtb == odtb)
print('cmdline 一致:', oh['cmdline'] == nh['cmdline'])
a = avb_info(nd)
print('AVB 尾部:', '有' if a else '无')

try:
    raw = gzip.decompress(nk)
    print('解压内核: %d 字节' % len(raw))
    for s in [b'4.19.191+', b'KernelSU', b'kernelsu', b'ksu_handle_execveat', b'SukiSU']:
        print('  含 %-20s : %s' % (s.decode(), s in raw))
except Exception as e:
    print('内核解压失败:', e)
