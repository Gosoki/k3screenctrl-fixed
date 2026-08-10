# 编译

## ⚠️ 必须 soft-float

K3 的 Cortex-A9 **未启用 VFP**：

```
$ cat /proc/cpuinfo
Features : half thumb fastmult edsp tls
           ↑ 没有 vfp / vfpv3 / neon
```

因此工具链必须是 **`arm-linux-musleabi`（软浮点）**，不能用 `musleabihf`。
硬浮点二进制在本机执行会直接非法指令退出。

> 同一约束也适用于其他要在 K3 上跑的程序：Go 程序需 `GOARM=5`
> （OpenWrt issue #10967 点名过 K3）。

## 方式一：独立交叉编译（推荐，最快）

```sh
# 工具链（约 98 MB，gcc 11.2.1）
wget https://musl.cc/arm-linux-musleabi-cross.tgz
tar -xzf arm-linux-musleabi-cross.tgz

# 直接编译本仓库的 src/
arm-linux-musleabi-cross/bin/arm-linux-musleabi-gcc -O2 -o k3screenctrl src/*.c -I src
```

产物约 31 KB。核对架构：

```sh
$ file k3screenctrl
ELF 32-bit LSB pie executable, ARM, EABI5 version 1 (SYSV),
dynamically linked, interpreter /lib/ld-musl-arm.so.1
```

确认是软浮点（**不应**出现 `VFP registers` 之类的属性）：

```sh
arm-linux-musleabi-cross/bin/arm-linux-musleabi-readelf -A k3screenctrl | grep -i float
```

## 方式二：从上游源码 + 补丁

```sh
wget https://github.com/lwz322/k3screenctrl/archive/refs/heads/master.tar.gz
tar -xzf master.tar.gz && cd k3screenctrl-master

for p in ../patches/*.patch; do patch -p1 < "$p"; done

arm-linux-musleabi-cross/bin/arm-linux-musleabi-gcc -O2 -o k3screenctrl src/*.c -I src
```

补丁基于上游 **master（2023-07-19）**。经核对，该版本与 ImmortalWrt 打包所用的
`d8896cfa`（2020-09-18）**逐文件完全一致**，两者均可作为基线。

## 方式三：OpenWrt SDK

上游打包定义见 [lwz322/k3screenctrl_build](https://github.com/lwz322/k3screenctrl_build)。
注意其 `DEPENDS` 含机型限定，用 SDK 单独编译时不会出现在 menuconfig 里：

```makefile
DEPENDS:=@TARGET_bcm53xx_DEVICE_phicomm-k3 +@KERNEL_DEVMEM +coreutils +coreutils-od +bash +curl
```

去掉 `@TARGET_bcm53xx_DEVICE_phicomm-k3 +@KERNEL_DEVMEM` 即可（上游 README 的说明）。

> 程序通过 `/dev/mem` 写 DMU 寄存器启用 UART2（`mask_memory_byte(0x1800c1c1, 0xf0, 0)`），
> 故内核需 `CONFIG_DEVMEM=y`。ImmortalWrt 官方镜像已满足。

## autotools（上游原生方式）

仓库保留了 `configure.ac` 与 `Makefile.am`。若要走完整流程：

```sh
autoreconf -i
./configure --host=arm-linux-musleabi
make
```

一般不必如此 —— 源码没有条件编译，直接 `gcc src/*.c` 即可。

## 部署前自检

编译或传输失败会留下截断/空文件，覆盖后服务直接起不来。养成习惯：

```sh
[ "$(wc -c < k3screenctrl)" -gt 20000 ] || { echo "产物异常，勿部署"; exit 1; }
```
