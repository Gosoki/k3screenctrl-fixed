#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include "common.h"
#include "infocenter.h"
#include "mcu_proto.h"
#include "requests.h"

/* 绕行清屏：MCU 仅在页码改变时清屏，故先切到相邻页再切回。中间页停留越短，
 * 视觉上的「切屏感」越轻；20ms 下基本只是一闪。 */
#define DETOUR_DELAY_US 20000

static int g_host_page = 0;
static PAGE g_current_page = PAGE_BASIC_INFO;  /* 开机默认停在第一页（型号/温度/负载/内存/运行时长） */

struct _host_info_single *get_hosts() {
    return g_host_info_array;
}

static int get_hosts_count() { return g_host_info_elements; }

static void send_page_data(PAGE page) {
    switch (page) {
    case PAGE_UPGRADE_INFO:
    case PAGE_BASIC_INFO:
        request_update_basic_info(
            g_basic_info.product_name, g_basic_info.hw_version, g_basic_info.fw_version,
            g_basic_info.sw_version,g_basic_info.mac_addr_base);
        break;
    case PAGE_PORTS:
        request_update_ports(&g_port_info);
        break;
    case PAGE_WAN:
        request_update_wan(g_wan_info.is_connected, g_wan_info.tx_bytes_per_sec,
                           g_wan_info.rx_bytes_per_sec);
        break;
    case PAGE_WIFI:
        request_update_wifi(&g_wifi_info);
        break;
    case PAGE_HOSTS:
        request_update_hosts_paged(get_hosts(), get_hosts_count(),
                                   g_host_page * HOSTS_PER_PAGE);
        break;
    case PAGE_WEATHER:
	    request_update_weather(&g_weather_info);
		break;
    default:
        syslog(LOG_WARNING, "unknown page requested: %d\n", page);
        break;
    }
}

void page_send_initial_data() {
    send_page_data(PAGE_BASIC_INFO);
    send_page_data(PAGE_PORTS);
    send_page_data(PAGE_WAN);
    send_page_data(PAGE_WIFI);
    send_page_data(PAGE_HOSTS);
	send_page_data(PAGE_WEATHER);
    request_switch_page(PAGE_BASIC_INFO);
}

/* Collect info by running scripts */
void page_update() {
    switch (g_current_page) {
    case PAGE_WAN:
        update_page_info(PAGE_WAN);
        update_page_info(PAGE_WIFI); // Shows STA count on WAN page
        break;
    default:
        update_page_info(g_current_page);
        break;
    }
}

/* Sends collected info to screen but do not switch to the page */
void page_refresh() {
    switch (g_current_page) {
    case PAGE_WAN:
        send_page_data(PAGE_WAN);
        send_page_data(PAGE_WIFI); // Shows STA count on WAN page
        break;
    default:
        send_page_data(g_current_page);
        break;
    }
}

void page_switch_to(PAGE page) {
    if (page >= PAGE_MIN && page <= PAGE_MAX) {
        /* MCU 只在页码**改变**时清屏；收到相同页码只做增量绘制，新数字会叠在
         * 旧数字上。若已停留在目标页，需绕相邻页一趟制造一次页码变化。
         * 绕行必须放在最后：采集（约 30ms）与数据发送都先完成，绕行期间仅剩
         * 两条切页命令，中间页的可见时间才最短；否则会明显看到来回切页，
         * 且 MCU 可能来不及处理而停在中间页。 */
        int same_page = (g_current_page == page);

        g_current_page = page;
        g_host_page = 0;
        /* 必须重新采集：周期刷新只更新 MCU 缓存、不提交页面，屏幕数值完全
         * 取决于切页这一刻采到的内容。 */
        page_update();
        page_refresh();

        if (same_page) {
            PAGE detour = (page == PAGE_MIN) ? (PAGE)(page + 1)
                                             : (PAGE)(page - 1);
            request_switch_page(detour);
            usleep(DETOUR_DELAY_US);
        }
        request_switch_page(page);
    }
}

void page_switch_next() {
    if (g_current_page != PAGE_HOSTS) {
        if (g_current_page < PAGE_MAX) {
            g_current_page++;
            page_update();      /* 见 page_switch_to */
            page_refresh();
            request_switch_page(g_current_page);
        }
    } else {
        /* In PAGE_HOSTS */
        if (get_hosts_count() - (g_host_page + 1) * HOSTS_PER_PAGE > 0) {
            g_host_page++;
            page_update();      /* 设备页内部翻页也需最新速率 */
            page_refresh();
            request_switch_page(g_current_page);
        }
    }
}

void page_switch_prev() {
    if (g_current_page != PAGE_HOSTS) {
        if (g_current_page > PAGE_MIN) {
            g_current_page--;
            page_update();      /* 见 page_switch_to */
            page_refresh();
            request_switch_page(g_current_page);
        }
    } else {
        /* In PAGE_HOSTS */
        if (g_host_page > 0) {
            g_host_page--;
        } else {
            g_current_page--;
            page_update();      /* 见 page_switch_to */
        }
        page_refresh();
        request_switch_page(g_current_page);
    }
}
