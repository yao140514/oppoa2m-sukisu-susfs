#!/usr/bin/env python3
"""重组 Android boot.img (header v2)，保持原厂布局。
用法:
  repack_boot.py <原始boot.img> <新内核Image.gz> <新dtb> <输出.img>
  repack_boot.py --roundtrip <boot.img>     # 原样回环校验
"""
import struct
import sys

HDR_MAGIC = b'ANDROID!'
PAGE = 2048


def align(n, page=PAGE):
    return (n + page - 1) // page * page


def parse(d):
    assert d[:8] == HDR_MAGIC, '不是 Android boot 镜像'
    (magic, kernel_size, kernel_addr, ramdisk_size, ramdisk_addr, second_size,
     second_addr, tags_addr, page_size, header_version, os_version) = struct.unpack_from('<8s10I', d, 0)
    hdr = dict(kernel_addr=kernel_addr, ramdisk_addr=ramdisk_addr, second_size=second_size,
               second_addr=second_addr, tags_addr=tags_addr, page_size=page_size,
               header_version=header_version, os_version=os_version,
               name=d[48:64], cmdline=d[64:576], idb=d[576:608], extra=d[608:1632],
               rec_size=0, rec_off=0, dtb_addr=0)
    if header_version >= 1:
        hdr['rec_size'], hdr['rec_off'], _ = struct.unpack_from('<IQI', d, 1632)
    dtb_size = 0
    if header_version >= 2:
        dtb_size, hdr['dtb_addr'] = struct.unpack_from('<IQ', d, 1648)
    ps = page_size
    kernel = d[ps:ps + kernel_size]
    roff = ps + align(kernel_size, ps)
    ramdisk = d[roff:roff + ramdisk_size]
    soff = roff + align(ramdisk_size, ps)
    second = d[soff:soff + second_size]
    doff = soff + align(second_size, ps)
    dtb = d[doff:doff + dtb_size] if dtb_size else b''
    return hdr, kernel, ramdisk, second, dtb


def build(hdr, kernel, ramdisk, second, dtb):
    ps, hv = hdr['page_size'], hdr['header_version']
    head = struct.pack('<8s10I', HDR_MAGIC, len(kernel), hdr['kernel_addr'], len(ramdisk),
                       hdr['ramdisk_addr'], len(second), hdr['second_addr'], hdr['tags_addr'],
                       ps, hv, hdr['os_version'])
    head += hdr['name'][:16].ljust(16, b'\0')
    head += hdr['cmdline'][:512].ljust(512, b'\0')
    head += hdr['idb'][:32].ljust(32, b'\0')
    head += hdr['extra'][:1024].ljust(1024, b'\0')
    if hv >= 1:
        head += struct.pack('<IQI', hdr['rec_size'], hdr['rec_off'], 1660 if hv >= 2 else 1648)
    if hv >= 2:
        head += struct.pack('<IQ', len(dtb), hdr['dtb_addr'])
    out = bytearray(head.ljust(align(len(head), ps), b'\0'))
    out += kernel.ljust(align(len(kernel), ps), b'\0')
    out += ramdisk.ljust(align(len(ramdisk), ps), b'\0')
    out += second.ljust(align(len(second), ps), b'\0')
    if hv >= 2:
        out += dtb.ljust(align(len(dtb), ps), b'\0')
    return bytes(out)


def avb_info(d):
    if d[-64:-60] == b'AVBf':
        return struct.unpack_from('>IIQQQ', d, len(d) - 60)
    return None


def main():
    if sys.argv[1] == '--roundtrip':
        d = open(sys.argv[2], 'rb').read()
        hdr, k, r, s, dtb = parse(d)
        img = build(hdr, k, r, s, dtb)
        avb = avb_info(d)
        orig_sz = avb[2] if avb else len(d)
        print('重建长度=%d  AVB声明原始长度=%d' % (len(img), orig_sz))
        same = img[:orig_sz] == d[:orig_sz]
        print('回环一致:', same)
        if not same:
            for i in range(min(len(img), orig_sz)):
                if img[i] != d[i]:
                    print('首个差异偏移', i, hex(i))
                    break
        sys.exit(0 if same else 1)

    src, new_kernel, new_dtb, dst = sys.argv[1:5]
    orig = open(src, 'rb').read()
    hdr, k, ramdisk, second, dtb = parse(orig)
    nk = open(new_kernel, 'rb').read()
    nd = open(new_dtb, 'rb').read() if new_dtb != '-' else dtb
    print('原: page=%d hdr=v%d kernel=%d ramdisk=%d dtb=%d' %
          (hdr['page_size'], hdr['header_version'], len(k), len(ramdisk), len(dtb)))
    print('新: kernel=%d dtb=%d' % (len(nk), len(nd)))
    img = build(hdr, nk, ramdisk, second, nd)
    print('新镜像(不含AVB) = %d 字节' % len(img))

    avb = avb_info(orig)
    if avb and len(img) + avb[4] + 64 <= len(orig):
        maj, mnr, _, vb_off, vb_sz = avb
        vbmeta = orig[vb_off:vb_off + vb_sz]
        body = bytearray(img.ljust(align(len(img)), b'\0'))
        new_vb_off = len(body)
        body += vbmeta
        body += b'\0' * (len(orig) - 64 - len(body))
        footer = struct.pack('>4sIIQQQ', b'AVBf', maj, mnr, len(img), new_vb_off, len(vbmeta))
        body += footer.ljust(64, b'\0')
        assert len(body) == len(orig)
        open(dst, 'wb').write(body)
        print('输出(保留AVB尾部结构): %s  %d 字节' % (dst, len(body)))
    else:
        open(dst, 'wb').write(img)
        print('输出(无AVB尾部): %s  %d 字节' % (dst, len(img)))


if __name__ == '__main__':
    main()
