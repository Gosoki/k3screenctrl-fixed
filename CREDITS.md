# 出处、版权与致谢

本仓库是对 **[lwz322/k3screenctrl](https://github.com/lwz322/k3screenctrl)** 的修复分支，
自身不主张对原始代码的著作权。所有上游作者的署名与版权声明均予保留。

## 许可证

**GNU General Public License v2.0**（见 [LICENSE](LICENSE)，取自上游 `COPYING`，339 行原文未改）。

依 GPL-2.0 §2(a)，本仓库对上游代码的修改已在下列位置显著标注：

- `patches/` 下 7 个补丁，逐一对应源文件与改动内容
- `src/` 中每处改动均带 `/* FIX: ... */` 注释，说明原因与实测依据
- [docs/FIXES.md](docs/FIXES.md) 给出完整技术分析

任何再分发同样受 GPL-2.0 约束。

## 上游沿革

| 项目 | 作者 | 贡献 |
|---|---|---|
| [updateing/k3screenctrl](https://github.com/updateing/k3screenctrl) | updateing（`haotia@gmail.com`，见 `configure.ac`） | **原始逆向与实现**（2017）。MCU 串口协议、帧格式、页面模型均出自此处 |
| [Hill-98/luci-app-k3screenctrl](https://github.com/Hill-98/luci-app-k3screenctrl) | Hill-98 | LuCI 网页设置支持（2018） |
| [zxlhhyccc/Hill-98-k3screenctrl](https://github.com/zxlhhyccc/Hill-98-k3screenctrl) | zxlhhyccc | 7 屏支持 |
| [lwz322/k3screenctrl](https://github.com/lwz322/k3screenctrl) | lwz322 | 当前上游。采集脚本整合、DSA 适配（2020–2023） |
| [lwz322/luci-app-k3screenctrl](https://github.com/lwz322/luci-app-k3screenctrl) | lwz322 | LuCI 分支 |
| [lwz322/k3screenctrl_build](https://github.com/lwz322/k3screenctrl_build) | lwz322 | OpenWrt 打包定义 |
| [CCluv/k3screenctrl](https://github.com/CCluv/k3screenctrl) | CCluv | 屏幕固件更新相关代码与固件文件 |
| [likanchen/k3screenctrl](https://github.com/likanchen/k3screenctrl) | likanchen | 「屏幕睡死」问题的跟进 |

沿革依据为上游 README 的说明，以及 `configure.ac`、源码与脚本内的署名。

## 文件级版权声明

以下声明来自文件本身，原样保留：

| 文件 | 声明 |
|---|---|
| `scripts/host.sh` | `# Copyright (C) 2017 XiaoShan https://www.mivm.cn` |
| `luci/controller/k3screenctrl.lua` | `-- Copyright (C) 2018 XiaoShan mivm.cn` |
| `luci/model/cbi/k3screenctrl.lua` | `-- Copyright (C) 2018 XiaoShan mivm.cn` |
| `configure.ac` | `AC_INIT(k3screenctrl, 0.10, haotia@gmail.com)` |

其余采集脚本（`basic.sh`/`wan.sh`/`wifi.sh`/`port.sh`/`weather.sh`）无独立版权头，
随项目适用 GPL-2.0。上游 `AUTHORS` 为空文件，C 源码各文件亦无单独版权头。

`scripts/oui.txt` 为 MAC 前缀到图标索引的映射表，随上游分发。

## 本仓库的改动

修复 7 处、涉及 6 个源文件（`scripts.c`、`infocenter.c`、`mcu_proto.h`、`pages.c`、
`signals.c`、`handlers.c`、`main.c`），另调整了 `init.d/k3screenctrl` 与 `scripts/basic.sh`。
其余文件与上游 master（2023-07）逐字节一致。

其中 **`scripts.c` 与 `infocenter.c` 的两个内存 bug 属上游普遍性缺陷**，与本机型无关，
在任何较新 musl/glibc 环境下都会复现，适合回馈上游。其余修复依赖本机 5 页 MCU 的实测结论，
是否适用于其他机器需自行验证。

## 第三方组件

`src/crcccitt.c`、`src/checksum.h` 为 CRC-CCITT(XModem) 实现，随上游分发，版权归其原作者。

## 免责

本仓库与 PHICOMM（斐讯）无任何关联。所涉 MCU 协议由上游作者逆向获得。
修改屏幕驱动存在使屏幕不可用的风险，**请自行承担**；回滚方法见 [docs/INSTALL.md](docs/INSTALL.md)。
