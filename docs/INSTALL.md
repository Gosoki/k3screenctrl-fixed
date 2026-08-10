# 安装

## 0. 前提

- 已刷 ImmortalWrt / OpenWrt 24.10（实测 ImmortalWrt 24.10.6 `r33869-cf234f8de6d5`）
- 能 SSH 登录路由器
- **屏幕固件为 5 页版**（连按右键走一圈数页数；若为 7 页见 [FIXES.md](FIXES.md) 第 3 节）

## 1. 安装官方包（取得目录结构、init 与 LuCI）

```sh
opkg update
opkg install phicomm-k3screenctrl luci-app-k3screenctrl luci-i18n-k3screenctrl-zh-cn
```

这一步会装好 `/lib/k3screenctrl/`、`/etc/config/k3screenctrl`、LuCI 界面与依赖
（`bash`、`curl`、`coreutils`、`coreutils-od`、`jq`、`bc`）。**此时屏幕仍是黑的** —— 官方包
的二进制在本固件下会段错误，下一步用修复版覆盖它。

## 2. 备份原件

```sh
mkdir -p /root/k3bak
cp /usr/bin/k3screenctrl /root/k3bak/k3screenctrl.orig
cp -r /lib/k3screenctrl /root/k3bak/scripts-orig
cp /etc/init.d/k3screenctrl /root/k3bak/init.orig
```

## 3. 部署修复版

> 新固件默认无 `sftp-server`，`scp` 会报 `/usr/libexec/sftp-server: not found`，
> 因此下面统一用 `cat | ssh` 传输。

```sh
IP=192.168.1.1     # 改成你的路由器地址

cat release/k3screenctrl | ssh root@$IP 'cat > /usr/bin/k3screenctrl'
for f in scripts/*.sh; do
    cat "$f" | ssh root@$IP "cat > /lib/k3screenctrl/$(basename "$f")"
done
cat scripts/oui.txt   | ssh root@$IP 'cat > /lib/k3screenctrl/oui/oui.txt'
cat init.d/k3screenctrl | ssh root@$IP 'cat > /etc/init.d/k3screenctrl'

ssh root@$IP 'chmod +x /usr/bin/k3screenctrl /etc/init.d/k3screenctrl /lib/k3screenctrl/*.sh'
```

### 校验（重要）

传输中断会留下**截断甚至 0 字节**的文件，覆盖后服务直接起不来。务必核对：

```sh
# 本地
sha256sum release/k3screenctrl
# 设备
ssh root@$IP 'sha256sum /usr/bin/k3screenctrl; wc -c /usr/bin/k3screenctrl'
```

两端一致、且大小约 31000 字节才算成功。

## 4. 启动并设为开机自启

```sh
ssh root@$IP '/etc/init.d/k3screenctrl enable; /etc/init.d/k3screenctrl restart'
sleep 5
ssh root@$IP 'ps w | grep [k]3screenctrl'
```

应看到 `/usr/bin/k3screenctrl -m 60 -d 2` 之类的进程。

## 5. 验证

```sh
ssh root@$IP '/usr/bin/k3screenctrl -t; echo "退出码=$?"'
```

正常应完整打印 BASIC / WIFI / WAN / PORT / WEATHER / HOST 六段信息，**退出码 0**。
若输出 `139` 即 SIGSEGV，说明装的仍是原版二进制。

> `-t` 只采集并打印，不初始化屏幕，因此它成功**不代表**屏幕一定正常，反之则必然有问题。

屏幕侧应看到：开机停在第一页（型号 / CPU 温度 / 负载 / 内存 / 运行时长），左右键翻页正常。

## 回滚

```sh
ssh root@$IP 'cp /root/k3bak/k3screenctrl.orig /usr/bin/k3screenctrl
              cp /root/k3bak/scripts-orig/*.sh /lib/k3screenctrl/
              cp /root/k3bak/init.orig /etc/init.d/k3screenctrl
              /etc/init.d/k3screenctrl restart'
```

彻底卸载：

```sh
ssh root@$IP '/etc/init.d/k3screenctrl stop; /etc/init.d/k3screenctrl disable
              opkg remove luci-app-k3screenctrl phicomm-k3screenctrl'
```

## 常见问题

| 现象 | 原因与处理 |
|---|---|
| 服务起不了，二进制 0 字节 | 传输被截断。重传并按上面的校验步骤核对 |
| `-t` 退出码 139 | 装的还是原版。确认 `md5sum` 与 `release/` 内一致 |
| 屏幕全黑、进程却在跑 | 熄屏了（默认 60 秒无按键）。按任意键唤醒；或在 LuCI 调大「熄屏时间」 |
| 页面顺序对不上本文档 | 你的屏幕固件可能是 7 页版，见 [FIXES.md](FIXES.md) 第 3 节 |
| 启动后自己跳到第二页 | 用的是未修复版本。本仓库已拦截 MCU 复位伪按键 |
