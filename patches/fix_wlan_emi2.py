#!/usr/bin/env python3
# 无条件使用 wifi-reserve-memory 作为 EMI 区，并输出诊断日志
F = '/home/ai/a2m/modules13/vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m/os/linux/gl_init.c'
s = open(F).read()

start = s.find('#if (CFG_SUPPORT_CONNINFRA == 0)\n\t/* 4.19 WMT')
if start == -1:
    print('fallback block not found')
    raise SystemExit(1)

# 找到该块对应的 #endif
end = s.find('#endif', s.find('wlan: emi', start))
end = s.find('\n', end) + 1

new = '''#if (CFG_SUPPORT_CONNINFRA == 0)
	/* 4.19 WMT 分支：wmt 未正确初始化 EMI，直接用 wifi-reserve-memory */
	{
		struct device_node *wnode;
		struct device_node *mnode;
		struct reserved_mem *rmem;

		wnode = of_find_compatible_node(NULL, NULL, "mediatek,wifi");
		if (wnode) {
			mnode = of_parse_phandle(wnode, "memory-region", 0);
			if (mnode) {
				rmem = of_reserved_mem_lookup(mnode);
				if (rmem) {
					gConEmiPhyBaseFinal = rmem->base;
					gConEmiSizeFinal = rmem->size;
					pr_info("wlan: emi fallback base=0x%llx size=0x%llx was=0x%llx\\n",
						(unsigned long long)rmem->base,
						(unsigned long long)rmem->size,
						(unsigned long long)gConEmiPhyBase);
				} else {
					pr_info("wlan: emi lookup failed\\n");
				}
				of_node_put(mnode);
			} else {
				pr_info("wlan: emi no memory-region\\n");
			}
			of_node_put(wnode);
		} else {
			pr_info("wlan: emi no wifi node\\n");
		}
	}
#endif
'''

s = s[:start] + new + s[end:]
open(F, 'w').write(s)
print('fallback replaced, unconditional')
