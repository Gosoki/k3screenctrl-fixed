# 调试手法

排查本项目的问题时用到的方法，按「先测事实、再改代码」的顺序整理。

## 1. `-t` 测试模式

```sh
k3screenctrl -t; echo "退出码=$?"
```

打印六段信息后退出。**关键特性：`-t` 在 `update_all_info()` 之后就 return，
不会执行 `screen_initialize()`**，因此不碰 `/dev/mem`、GPIO 和串口。

由此可以二分：`-t` 崩 → 问题在采集/解析；`-t` 正常但屏幕异常 → 问题在屏幕通信或 MCU。

## 2. 单独跑采集脚本

```sh
sh /lib/k3screenctrl/basic.sh   # 逐个执行，看输出行数与内容是否符合预期
```

各脚本的输出行数必须与 `infocenter.c` 里的 `stores[]` 项数严格一致，否则 tokenizer
会错位。`host.sh` 尤其注意：首行是主机数 N，其后每主机 4 行（名称/下行/上行/图标），
**末行还有一个哨兵 `0`**，缺了会被判为「输出不完整」而丢掉最后一台主机。

## 3. 串口抓帧（最有用）

在 `serial_port.c` 的 `serial_write()` / `serial_read()` 里插打印，即可看到与 MCU 的全部往来：

```c
int serial_write(const unsigned char *data, int len) {
    int ret = write(g_serial_fd, data, len);
    int ti = 2, tv = 0, pg = -1;
    if (len > 2) {
        /* type 可能被 ESCAPE(0x10) 前置，必须跳过后再取 */
        if (data[ti] == 0x10 && len > 3) { tv = data[ti+1]; ti += 2; }
        else { tv = data[ti]; ti += 1; }
        if (tv == 4 && len > ti)                 /* SWITCH_PAGE：解出页码 */
            pg = (data[ti] == 0x10 && len > ti+1) ? data[ti+1] : data[ti];
    }
    { FILE *f = fopen("/tmp/tx", "a");
      if (f) { fprintf(f, "T=%02d page=%d len=%d\n", tv, pg, len); fclose(f); } }
    return ret;
}
```

⚠️ **解帧时务必处理转义**，否则统计全错 —— 我第一次就是直接取 `data[2]`，
把所有 `SWITCH_PAGE` 都误判成了 `type=0x10`。

帧结构：

```
FRAME_HEADER(0x01) | PAYLOAD_HEADER(0x30) | TYPE | PAYLOAD | CRC-CCITT(2B) | TRAILER(0x04)
载荷中的 0x01 / 0x04 / 0x10 需由 FRAME_ESCAPE(0x10) 转义
```

`REQUEST_TYPE`：1=GET_MCU_VERSION 4=SWITCH_PAGE 5=PORTS 6=WAN 7=WIFI 8=HOSTS
9=BASIC 10=NOTIFY_EVENT 11=WEATHER（**2 和 3 空缺**，实测发过去 MCU 不报错也无反应）。

## 4. 页码映射扫描

**遇到「页面对不上」时先做这个**，一次就能测出 MCU 的真实页码：

```c
/* 放进 SIGALRM 分支：每 N 个周期发一个页码，1→7 循环 */
static int tick = 0, pg = 0;
if (++tick >= 4) { tick = 0; pg = (pg % 7) + 1; request_switch_page((PAGE)pg); }
```

屏幕会自动轮播，记下「发送页码 → 实际显示内容」的对应关系即可。
本机结果：1=型号/MAC 2=USB与网口 3=速率 4=WiFi密码 5=已接入终端，**6、7 无效**。

## 5. 判断进程是否卡住

```sh
cat /proc/<pid>/wchan      # do_sys_poll 表示正常等在 poll 上
cat /proc/<pid>/syscall    # 168 = poll，参数含 nfds 与超时
ls -l /proc/<pid>/fd/      # 应有 /dev/ttyS1 与 anon_inode:[signalfd]
awk '/SigBlk|SigPnd/' /proc/<pid>/status
```

`SigBlk` 含 SIGALRM 是正常的（程序用 signalfd 接收，故先 block）；
若 `SigPnd` 长期非 0，说明信号没被读走。

## 6. 计时（BusyBox 环境）

BusyBox 的 `date` 不支持 `%N`，`time ( ... )` 也不被 ash 接受。用 `/proc/uptime` 的厘秒：

```sh
cs() { awk '{print int($1*100)}' /proc/uptime; }
T0=$(cs); i=0; while [ $i -lt 20 ]; do sh 脚本 >/dev/null 2>&1; i=$((i+1)); done; T1=$(cs)
echo "单次 $(( (T1-T0)*10/20 )) ms"
```

## 7. 探针注意事项

- **别用 `sed` 往脚本里插一行「打印+原逻辑」的复合语句**，日后用正则删除时容易把原逻辑一并删掉
- 探针只插到**当前页**的脚本上会误判 —— `page_update()` 只跑当前页，其他页脚本本就不会被调用
- 用户操作会污染测量（按键会重置熄屏计时、切页会触发额外采集），静置测量或明确告知
- **部署前一定校验产物大小**，编译失败后 `cp` 空文件会让服务彻底起不来：

```sh
[ "$(wc -c < 产物)" -gt 20000 ] || exit 1
```

## 8. 常用观察点

| 目的 | 方法 |
|---|---|
| 采集是否在跑 | `/tmp/k3screenctrl/device_speed/conntrack.state` 首行是时间戳，看它是否递增 |
| 串口是否活跃 | `grep '^1:' /proc/tty/driver/serial` 看 `tx:`/`rx:` 计数 |
| 服务日志 | `logread \| grep -i k3screen` |
| MCU 是否回话 | 抓 `serial_read()`。本机 MCU **只在按键时主动发帧** |
