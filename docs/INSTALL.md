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

---

## 6. LuCI 网页设置界面

装完后菜单位于 **系统 → Screen**（中文包生效时显示为「屏幕」）。

### 方式一：opkg 安装（推荐）

```sh
opkg update
opkg install luci-app-k3screenctrl
opkg install luci-i18n-k3screenctrl-zh-cn     # 中文，可选
```

依赖 `phicomm-k3screenctrl` 与 `luci-lua-runtime`，opkg 会自动拉取。
装完刷新浏览器即可看到菜单（若无，见下方「菜单不显示」）。

### 方式二：手动安装（源里没有该包时）

LuCI 应用一共 **4 个文件**，缺一不可 —— 尤其是 ACL，缺了菜单不显示或点进去报无权限：

| 文件 | 安装到 | 作用 |
|---|---|---|
| `luci/controller/k3screenctrl.lua` | `/usr/lib/lua/luci/controller/` | 注册菜单项 |
| `luci/model/cbi/k3screenctrl.lua` | `/usr/lib/lua/luci/model/cbi/` | 设置页面的表单定义 |
| `luci/acl.d/luci-app-k3screenctrl.json` | `/usr/share/rpcd/acl.d/` | **授予读写 `k3screenctrl` 配置的权限** |
| `luci/i18n/k3screenctrl.zh-cn.lmo` | `/usr/lib/lua/luci/i18n/` | 中文翻译（可选） |

```sh
IP=192.168.1.1

ssh root@$IP 'mkdir -p /usr/lib/lua/luci/controller /usr/lib/lua/luci/model/cbi \
                       /usr/share/rpcd/acl.d /usr/lib/lua/luci/i18n'

cat luci/controller/k3screenctrl.lua       | ssh root@$IP 'cat > /usr/lib/lua/luci/controller/k3screenctrl.lua'
cat luci/model/cbi/k3screenctrl.lua        | ssh root@$IP 'cat > /usr/lib/lua/luci/model/cbi/k3screenctrl.lua'
cat luci/acl.d/luci-app-k3screenctrl.json  | ssh root@$IP 'cat > /usr/share/rpcd/acl.d/luci-app-k3screenctrl.json'
cat luci/i18n/k3screenctrl.zh-cn.lmo       | ssh root@$IP 'cat > /usr/lib/lua/luci/i18n/k3screenctrl.zh-cn.lmo'

# 确保 lua 运行时在位（LuCI 24.10 默认可能只装了 ucode 版）
ssh root@$IP 'opkg list-installed | grep -q luci-lua-runtime || opkg install luci-lua-runtime'
```

`.lmo` 是编译后的二进制翻译文件，**传输时务必核对大小**（1632 字节），
用 `scp` 或 `cat |` 都行，但别用会做换行转换的方式。

#### 配置文件

界面读写 `/etc/config/k3screenctrl`。若该文件不存在，菜单会被 controller 主动隐藏：

```lua
if not nixio.fs.access("/etc/config/k3screenctrl") then
    return          -- 配置不存在就不注册菜单
end
```

`phicomm-k3screenctrl` 包会带上它；手动部署时可用仓库里的模板：

```sh
cat etc/config/k3screenctrl | ssh root@$IP 'cat > /etc/config/k3screenctrl'
```

#### 刷新菜单缓存

LuCI 会缓存菜单索引，新增 controller 后必须清掉，否则看不到入口：

```sh
ssh root@$IP 'rm -f /tmp/luci-indexcache* /tmp/luci-modulecache/* 2>/dev/null
              /etc/init.d/rpcd restart
              /etc/init.d/uhttpd restart'
```

然后 **在浏览器里退出登录再重新登录**（ACL 在会话建立时读取，不重登不生效）。

### 验证 LuCI

```sh
ssh root@$IP 'ls -l /usr/lib/lua/luci/controller/k3screenctrl.lua \
                    /usr/lib/lua/luci/model/cbi/k3screenctrl.lua \
                    /usr/share/rpcd/acl.d/luci-app-k3screenctrl.json
              uci show k3screenctrl | head -3'
```

浏览器进 **系统 → Screen**，应能看到熄屏时间、刷新间隔、隐藏 WiFi 密码、显示更多信息、
设备名称/图标等选项，以及一个 Test 按钮（点击会执行 `k3screenctrl -t` 并回显结果）。

各选项的含义与是否真正生效，见 [USAGE.md](USAGE.md#luci-设置系统--screen)。

### LuCI 常见问题

| 现象 | 原因与处理 |
|---|---|
| **菜单不显示** | ① `/etc/config/k3screenctrl` 不存在 → controller 主动隐藏；② 索引缓存未清 → 删 `/tmp/luci-indexcache*` 并重启 uhttpd |
| **点进去报无权限 / 保存不了** | ACL 文件缺失或未重登。补 `acl.d/*.json`，重启 `rpcd`，**退出登录再进** |
| **界面全是英文** | 未装语言包，或 `.lmo` 传输损坏（应为 1632 字节） |
| **报 lua 相关错误** | 缺 `luci-lua-runtime`。24.10 的 LuCI 默认走 ucode，这个应用是 lua 写的，必须显式安装 |
| **`opkg install` 提示 postinst 失败** | 常见于 `luci-i18n-*` 包找不到 `/etc/uci-defaults/...`。文件其实已装好，可忽略；或手动 `opkg install --force-postinst` |
| **改了设置屏幕没反应** | 熄屏时间和刷新间隔由 init 脚本在**启动时**读取并传参，改完需 `/etc/init.d/k3screenctrl restart` |

> **卸载 LuCI 部分**（保留屏幕功能）：
> ```sh
> opkg remove luci-app-k3screenctrl luci-i18n-k3screenctrl-zh-cn
> rm -f /tmp/luci-indexcache*
> ```

---

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

## 主程序常见问题

| 现象 | 原因与处理 |
|---|---|
| 服务起不了，二进制 0 字节 | 传输被截断。重传并按上面的校验步骤核对 |
| `-t` 退出码 139 | 装的还是原版。确认 `md5sum` 与 `release/` 内一致 |
| 屏幕全黑、进程却在跑 | 熄屏了（默认 60 秒无按键）。按任意键唤醒；或在 LuCI 调大「熄屏时间」 |
| 页面顺序对不上本文档 | 你的屏幕固件可能是 7 页版，见 [FIXES.md](FIXES.md) 第 3 节 |
| 启动后自己跳到第二页 | 用的是未修复版本。本仓库已拦截 MCU 复位伪按键 |
