#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "mcu_proto.h"
#include "pages.h"

#include "requests.h"

static int g_signal_fd;

int signal_setup() {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM); // Timed update
    sigaddset(&mask, SIGTERM); // Router reboot
    sigaddset(&mask, SIGUSR1); // Factory reset
    sigaddset(&mask, SIGUSR2); // Firmware update

    /* Block in order to prevent default disposition */
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        syslog(LOG_WARNING, "could not block signals: %s\n", strerror(errno));
    }

    g_signal_fd = signalfd(-1, &mask, 0);
    if (g_signal_fd < 0) {
        syslog(LOG_WARNING, "could not set up signal fd: %s\n",
               strerror(errno));
    }

    return g_signal_fd;
}

static time_t g_last_check_time;
void refresh_screen_timeout() { g_last_check_time = time(NULL); }

static void check_screen_timeout() {
    extern int g_is_screen_on;
    /* FIX: 原实现未判断屏幕是否已熄灭，超时后每个刷新周期都重发 EVENT_SLEEP
     * （实测熄屏后 64 秒内发了 32 帧）。只在由亮转灭的那一次发送。 */
    if (CFG->screen_timeout != 0 && g_is_screen_on &&
        time(NULL) - g_last_check_time >= CFG->screen_timeout) {
        g_is_screen_on = 0; /* Do not process key messages - just wake up if there are any */
        request_notify_event(EVENT_SLEEP);
    }
}

void signal_notify() {
    struct signalfd_siginfo siginfo;
    if (read(g_signal_fd, &siginfo, sizeof(siginfo)) <= 0) {
        syslog(LOG_WARNING,
               "could not read from signalfd, signal ignored: %s\n",
               strerror(errno));
        return;
    }

    switch (siginfo.ssi_signo) {
    case SIGALRM: {
        /* FIX: 原实现无条件采集并发送，屏幕熄灭后照样每周期跑全部采集脚本。
         * 实测熄屏后调用频率与亮屏时相同（150 秒内 195 次），CPU 与串口带宽
         * 纯属浪费 —— 设备页在 1 秒刷新下单这一项就占 21%。
         * 屏幕黑着时没有观众，跳过即可；按键唤醒会置 g_is_screen_on=1，
         * 下一个刷新周期自然恢复。 */
        extern int g_is_screen_on;
        check_screen_timeout();
        if (g_is_screen_on) {
            page_update();
            page_refresh();
            /* FIX: MCU 收到 UPDATE_* 只写入内部缓存，不会重绘屏幕 —— 实测停在
             * 基本信息页两分钟，连每分钟必变的 uptime 都不动，而同期该页的
             * UPDATE_BASIC 帧每 2 秒发出一次、数据源亦在变化。需要额外一条
             * 通知促使其重绘。
             * 用 EVENT_WAKEUP 而非 SWITCH_PAGE：后者会整屏重绘（闪烁），并把
             * 画面强行拉回程序认定的页面。
             * 位置必须在这里而非 page_refresh() 内部 —— 切页路径依赖
             * 「UPDATE_* → SWITCH_PAGE」的配对语义（见 requests.h 注释），
             * 中间插入其他帧会将其拆散。 */
            /* 不在周期刷新中提交页面：MCU 仅在页码**改变**时清屏，收到相同
             * 页码只做增量绘制，新数字会叠在旧数字上（四种提交方式实测皆然：
             * 仅重发、type=2、type=3、绕经无效页码再切回）。
             * 因此周期只更新 MCU 内部缓存，由用户按键切页时完成清屏重绘 ——
             * 切页路径是「采集 → 发数据 → SWITCH_PAGE」，页码一变即显示最新值。 */
        }
        alarm(CFG->update_interval);
        break; }
    case SIGTERM:
        request_notify_event(EVENT_REBOOT);
        exit(0);
        break;
    case SIGUSR1:
        request_notify_event(EVENT_RESET);
        break;
    case SIGUSR2:
        request_notify_event(EVENT_UPGRADE);
        break;
    default:
        syslog(LOG_INFO, "someone forgot to add his signal (%d) handler here\n",
               siginfo.ssi_signo);
        break;
    }
}