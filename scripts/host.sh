#!/bin/sh
# Copyright (C) 2017 XiaoShan https://www.mivm.cn
# 2026-08-10 适配 ImmortalWrt 24.10（第三版 · 单 awk 重写）
#
#   为什么这么写：
#   1) 每设备速率改用 conntrack 字节计数。本机启用软件 flow offloading，命中快速
#      路径的包不再经过 nftables forward chain，nftables/iptables 的 counter 会
#      漏计；nf_flow_table 会把字节数回写 conntrack，实测仍在增长，是 offload
#      开启时唯一准确的数据源。（硬件 offload 在 bcm53xx 上未实现，那个开关是摆设）
#   2) 全部逻辑合并进单个 awk。前两版的开销来源实测为：
#        · conntrack 用 for(i=1;i<=NF;i++) 逐字段 substr → 245 ms
#        · 主循环每设备 ~12 次 fork（grep/tr/cut/uci…）× 6 台 → 212 ms
#      本版改用 index() 定位字段、单进程读全部输入，两项开销同时消除。
#
#   输出格式（k3screenctrl 的 update_host_info 要求，顺序不可改）：
#     第一行 = 主机数 N
#     其后每主机 4 行：名称 / 下行 B/s / 上行 B/s / 图标索引
#     最后一行 = 0   ← 哨兵，缺了会被判为「输出不完整」而丢掉最后一台主机

STATE_DIR="/tmp/k3screenctrl/device_speed"
STATE="$STATE_DIR/conntrack.state"
CUSTOM="/tmp/k3_custom"
OUI="/lib/k3screenctrl/oui/oui.txt"

if [ -s /tmp/lan_online_list.temp ]; then
	cat /tmp/lan_online_list.temp
	rm -f /tmp/lan_online_list.temp
	exit 0
fi

mkdir -p "$STATE_DIR" 2>/dev/null
uci show k3screenctrl > "$CUSTOM" 2>/dev/null
[ -f "$STATE" ] || : > "$STATE"
[ -f "$OUI" ] || OUI=/dev/null

read -r _UP _ < /proc/uptime          # shell 内建，零 fork
NOW="${_UP%%.*}"
_LANIP="$(uci -q get network.lan.ipaddr)"
PFX="${_LANIP%.*}."

awk -v PFX="$PFX" -v NOW="$NOW" -v STATE="$STATE" -v OUT="$STATE.tmp" '
# ---- 从 "key=" 处取出到下一个空格为止的值；找不到返回 "" ----
function fld(s, key, from,   p, r, e) {
	p = index(substr(s, from), key)
	if (p == 0) return ""
	p += from - 1 + length(key)
	r = substr(s, p)
	e = index(r, " ")
	return (e ? substr(r, 1, e - 1) : r)
}
function pos(s, key, from,   p) {
	p = index(substr(s, from), key)
	return (p ? p + from - 1 : 0)
}

# ---------- ① ARP 表：确定设备清单与顺序 ----------
FILENAME ~ /arp$/ {
	if ($1 == "IP" || $0 !~ /br-lan/ || $3 == "0x0") next
	n++; IP[n] = $1; MAC[n] = toupper($4)
	next
}
# ---------- ② DHCP 租约：IP → 主机名 ----------
FILENAME ~ /dhcp\.leases$/ { HN[$3] = $4; next }

# ---------- ③ OUI：整行留着，稍后按 MAC 前缀匹配 ----------
FILENAME ~ /oui\.txt$/ { OUIL[++oc] = toupper($0); OUII[oc] = $1; next }

# ---------- ④ LuCI 自定义名称与图标 ----------
FILENAME ~ /k3_custom$/ {
	e = index($0, "="); if (!e) next
	k = substr($0, 1, e - 1); v = substr($0, e + 1)
	gsub(/'"'"'/, "", v)                            # 去掉单引号
	np = k; sub(/\.[^.]+$/, "", np)           # section 名
	lp = k; sub(/^.*\./, "", lp)              # 选项名
	if (lp == "mac")  CM[np] = toupper(v)
	if (lp == "name") CN[np] = v
	if (lp == "icon") CI[np] = v
	next
}
# ---------- ⑤ 上轮 conntrack 快照 ----------
FILENAME == STATE {
	if (FNR == 1) { PREV_T = $1 + 0; next }
	PU[$1] = $2 + 0; PD[$1] = $3 + 0; next
}
# ---------- ⑥ conntrack：用 index() 定位，不逐字段扫 ----------
{
	p1 = pos($0, "src=", 1); if (!p1) next
	s1 = fld($0, "src=", p1)
	d1 = fld($0, "dst=", p1)
	pb = pos($0, "bytes=", 1); if (!pb) next
	b1 = fld($0, "bytes=", pb) + 0
	pb2 = pos($0, "bytes=", pb + 6)
	b2 = pb2 ? fld($0, "bytes=", pb2) + 0 : 0

	L = length(PFX)
	if (substr(s1, 1, L) == PFX)      { CU[s1] += b1; CD[s1] += b2 }
	else if (substr(d1, 1, L) == PFX) { CD[d1] += b1; CU[d1] += b2 }
}

END {
	# ---- 写新快照 ----
	printf "%s\n", NOW > OUT
	for (k in CU) printf "%s %.0f %.0f\n", k, CU[k], CD[k] > OUT
	close(OUT)

	DT = NOW - PREV_T
	if (PREV_T <= 0 || DT < 1 || DT > 600) DT = 0     # 无基线/间隔异常 → 本轮不出数

	print n
	for (i = 1; i <= n; i++) {
		ip = IP[i]; mac = MAC[i]
		name = (ip in HN && HN[ip] != "" && HN[ip] != "*") ? HN[ip] : "Unknown"

		# OUI → 图标
		logo = "0"
		pre = substr(mac, 1, 2) substr(mac, 4, 2) substr(mac, 7, 2)
		for (j = 1; j <= oc; j++) if (index(OUIL[j], pre)) { logo = OUII[j]; break }

		# LuCI 自定义覆盖
		for (sec in CM) if (CM[sec] == mac) {
			if (CN[sec] != "") name = CN[sec]
			if (CI[sec] != "") logo = CI[sec]
			break
		}

		up = 0; dn = 0
		if (DT > 0 && (ip in PU)) {
			du = CU[ip] - PU[ip]; dd = CD[ip] - PD[ip]
			# 连接关闭后条目从 conntrack 消失，累计值可能回落 → 夹到 0
			if (du > 0) up = du / DT
			if (dd > 0) dn = dd / DT
		}

		print name
		printf "%.0f\n", dn
		printf "%.0f\n", up
		print logo
	}
	print 0
}
' /proc/net/arp /tmp/dhcp.leases "$OUI" "$CUSTOM" "$STATE" /proc/net/nf_conntrack

[ -s "$STATE.tmp" ] && mv "$STATE.tmp" "$STATE"
