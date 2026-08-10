# k3screenctrl — PHICOMM K3 前面板 LCD 驱动（修复版）

让 **PHICOMM K3** 路由器的前面板小屏在 **ImmortalWrt / OpenWrt 24.10** 上正常工作。

上游程序在该固件下**启动即段错误、屏幕全黑**；修好崩溃后又出现「速率不刷新、设备页空白、
按键要连按几次、按中键跳到 WiFi 密码页」等一连串问题。本仓库定位并修复了全部根因，
共 **7 个补丁、6 个源文件**，其余代码与上游逐字节一致。

> **本仓库不是上游**。原始逆向与实现属 updateing / lwz322 / XiaoShan 等作者，
> 详见 [CREDITS.md](CREDITS.md)。协议为 **GPL-2.0**，见 [LICENSE](LICENSE)。

---

## 适用范围

| | |
|---|---|
| 硬件 | PHICOMM K3（`bcm53xx/generic`，Cortex-A9 **无 VFP**） |
| 固件 | ImmortalWrt 24.10.6 实测通过；同代 OpenWrt 应可用 |
| 屏幕固件 | **5 页版（原厂）**，见下方说明 |

### ⚠️ 屏幕固件版本决定页数

本修复针对 **5 页**的原厂屏幕固件。刷过新版斐讯官方/官改固件的机器可能是 **7 页**版
（多出「升级」和「天气」两页），页码编排不同，**需要改回上游枚举**，
详见 [docs/FIXES.md](docs/FIXES.md) 第 3 节。

先确认你的机器是哪一种：从任一页连续按右键走完一圈，数出总页数。

---

## 快速开始

```sh
# 1. 传入设备（编译好的 armv5 soft-float 二进制）
cat release/k3screenctrl | ssh root@<路由器IP> 'cat > /usr/bin/k3screenctrl'

# 2. 采集脚本
for f in scripts/*.sh; do
    cat "$f" | ssh root@<路由器IP> "cat > /lib/k3screenctrl/$(basename $f)"
done

# 3. init 脚本
cat init.d/k3screenctrl | ssh root@<路由器IP> 'cat > /etc/init.d/k3screenctrl'

# 4. 赋权并启动
ssh root@<路由器IP> 'chmod +x /usr/bin/k3screenctrl /etc/init.d/k3screenctrl /lib/k3screenctrl/*.sh
                     /etc/init.d/k3screenctrl enable
                     /etc/init.d/k3screenctrl restart'
```

完整步骤（含依赖、校验、回滚）见 **[docs/INSTALL.md](docs/INSTALL.md)**。

---

## 修了什么

| # | 问题 | 根因 |
|---|---|---|
| 1 | **启动即段错误** | `infocenter.c` use-after-free：`free()` 后仍解引用指向该缓冲区的游标 |
| 2 | 同上 | `scripts.c` 64KB 缓冲区从不写 `'\0'`，却被当 C 字符串交给 `strchr()` |
| 3 | **数据不刷新 / 设备页空白 / 中键跳错页 / 要连按几次** | `mcu_proto.h` 页码枚举按「7 屏版」编排，与 5 页 MCU 整体错位 |
| 4 | 切页显示旧值 | 切页时未重新采集 |
| 5 | 熄屏后仍满负荷跑脚本 | `check_screen_timeout()` 不阻止后续采集 |
| 6 | 开机自动跳走一页 | MCU 复位时吐出一帧伪 `KEY_RIGHT_SHORT` |
| 7 | 首页无法手动刷新 | 中键切到当前页时页码未变，MCU 不清屏 |

每一条的推导过程、实测数据与代码位置见 **[docs/FIXES.md](docs/FIXES.md)**。

---

## 文档

| 文档 | 内容 |
|---|---|
| [docs/INSTALL.md](docs/INSTALL.md) | 安装、依赖、校验、回滚 |
| [docs/USAGE.md](docs/USAGE.md) | 按键操作、LuCI 各选项、屏幕显示规则 |
| [docs/BUILD.md](docs/BUILD.md) | 交叉编译（**必须 soft-float**） |
| [docs/DEBUG.md](docs/DEBUG.md) | 串口抓帧、探针、页码扫描等排查手法 |
| [docs/FIXES.md](docs/FIXES.md) | 7 个修复的完整技术分析 |
| [CREDITS.md](CREDITS.md) | 出处、版权、致谢 |

## 目录

```
src/         打过补丁的完整源码（可直接编译）
patches/     7 个补丁，可对上游 master 逐个应用
scripts/     6 个采集脚本 + OUI 表
init.d/      启动脚本
luci/        LuCI 界面文件（未改动，仅供参考）
release/     编译好的 armv5 soft-float 二进制 + SHA256SUMS
docs/        文档
```
