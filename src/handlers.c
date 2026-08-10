#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "handlers.h"
#include "mcu_proto.h"
#include "pages.h"
#include "requests.h"
#include "signals.h"

static MCU_VERSION g_mcu_version;
void handle_mcu_version(const unsigned char *payload, int len) {
    if (len < 4) {
        syslog(LOG_WARNING,
               "Got malformed MCU version response. Length is %d\n", len);
        return;
    }
    g_mcu_version.patch_ver =
        payload[0] |
        payload[1] << 8; /* Do we need this endian compatabitity? */
    g_mcu_version.minor_ver = payload[2];
    g_mcu_version.major_ver = payload[3];

    syslog(LOG_INFO, "MCU reported version as %hhd.%hhd.%hd\n",
           g_mcu_version.major_ver, g_mcu_version.minor_ver,
           g_mcu_version.patch_ver);
}

int g_is_screen_on = 1;
/* FIX: screen_initialize() 会拉动 SCREEN_RESET_GPIO 复位 MCU，MCU 在复位过程中
 * 会吐出一帧内容为 KEY_RIGHT_SHORT 的报文（实测启动后约 1 秒必现，无人触碰按键）。
 * 它被当作真按键处理，使页面从默认的速率页自动跳到下一页。serial_setup() 里的
 * tcflush 发生在复位之前，拦不住。这里在启动初期直接忽略按键。 */
#define STARTUP_KEY_IGNORE_SEC 3
time_t g_startup_time;

void handle_key_press(const unsigned char *payload, int len) {
    if (g_startup_time != 0 &&
        time(NULL) - g_startup_time < STARTUP_KEY_IGNORE_SEC) {
        syslog(LOG_INFO, "ignoring key press during startup (MCU reset noise)\n");
        return;
    }
    if (len < 1) {
        syslog(LOG_WARNING, "Got malformed key press response. Length is %d\n",
               len);
        return;
    }
    refresh_screen_timeout();
    if (!g_is_screen_on) {
        /* Do not process key messages when waking up */
        request_notify_event(EVENT_WAKEUP);
        g_is_screen_on = 1;
        return;
    }
    switch (payload[0]) {
    case KEY_LEFT_SHORT:
        page_switch_prev();
        printf("KEY_LEFT_SHORT\n");
        break;
    case KEY_RIGHT_SHORT:
        page_switch_next();
        printf("KEY_RIGHT_SHORT\n");
        break;
    case KEY_MIDDLE_SHORT:
        /* 中键 = 回到并刷新第一页（型号/CPU温度/负载/内存/运行时长）。
         * 左右翻页时页码必变，MCU 会清屏重绘，故其余页面本就显示正常；
         * 唯独开机即停留的第一页若不翻页就不会刷新，由此键补上。
         * page_switch_to() 内含「已在目标页则绕相邻页一趟」的处理，
         * 因此在第一页再次按下同样能强制清屏重绘。 */
        page_switch_to(PAGE_BASIC_INFO);
        printf("KEY_MIDDLE_SHORT\n");
        break;
    case KEY_MIDDLE_LONG:
        request_notify_event(EVENT_SLEEP);
        g_is_screen_on = 0;
        return;
    case KEY_LEFT_LONG:
    case KEY_RIGHT_LONG:
        printf("KEY_x_LONG\n");
        return;
    default:
        syslog(LOG_WARNING, "unknown key code: %hhx\n", payload[0]);
        return;
    }
}

RESPONSE_HANDLER g_response_handlers[] = {
    {RESPONSE_MCU_VERSION, handle_mcu_version},
    {RESPONSE_KEY_PRESS, handle_key_press},
};
