#include <stdio.h>
#include <string.h>
#include <math.h>
#include "service_manager.h"
#include "driver_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif

#include "ntp_service.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* NTP server (was in cloud_service/ai_env_config.h) */
#define NTP_SERVER "203.107.6.88"

/* 仅在 WS63 平台编译 */
#ifdef PLATFORM_TYPE_WS63
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"

/* NTP 常量定义 */
#define NTP_PORT               123
#define NTP_EPOCH_OFFSET       2208988800UL  /* 1900-1970 秒数 */
#define NTP_SERVER_IP          NTP_SERVER    /* NTP 服务器（从 volc_rtc_auth.h 获取） */
#define NTP_TIMEOUT_MS         3000           /* 超时时间 3 秒 */
#define NTP_POLL_INTERVAL_MS   100            /* 轮询间隔 */

/* NTP 包格式 */
typedef struct {
    uint8_t  li_vn_mode;       /* 跳秒指示、版本、模式 */
    uint8_t  stratum;           /* 层数 */
    uint8_t  poll;              /* 轮询间隔 */
    uint8_t  precision;         /* 精度 */
    uint32_t root_delay;        /* 根延迟 */
    uint32_t root_dispersion;   /* 根离散 */
    uint32_t ref_id;            /* 参考标识 */
    uint32_t ref_timestamp_sec; /* 参考时间戳秒 */
    uint32_t ref_timestamp_frac;
    uint32_t orig_timestamp_sec;/* 原始时间戳秒 */
    uint32_t orig_timestamp_frac;
    uint32_t recv_timestamp_sec;/* 接收时间戳秒 */
    uint32_t recv_timestamp_frac;
    uint32_t trans_timestamp_sec;/* 发送时间戳秒 */
    uint32_t trans_timestamp_frac;
} ntp_packet_t;

/* 本地字节序转换（小端平台需转换） */
static uint16_t my_htons(uint16_t hostshort) {
    return (hostshort << 8) | (hostshort >> 8);
}
static uint32_t my_htonl(uint32_t hostlong) {
    return ((hostlong & 0x000000FF) << 24) |
           ((hostlong & 0x0000FF00) << 8)  |
           ((hostlong & 0x00FF0000) >> 8)  |
           ((hostlong & 0xFF000000) >> 24);
}

#ifdef SUPPORT_PCF8563_RTC
#ifdef DRV_CORE
static int mrtc_fd = -1;
#else
Rtc_Driver* mrtc_driver =NULL;
#endif
#endif

/* 全局状态 */
static struct tm      g_base_time = {0};      /* NTP 同步的绝对时间 */
static goldie_timeval g_base_rtc={0};        /* 同步时刻的 RTC 值 */
static int            g_time_synced = 0; /* 是否已同步 */

/* 临时同步用变量（仅用于 sync_time 内） */
static volatile int   g_resp_received = 0;
static ntp_packet_t   g_resp_packet;

/* NTP 接收回调（在 lwIP 上下文调用） */
static void ntp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                               const ip_addr_t *addr, u16_t port) {
    if (p == NULL) return;
    if (p->len >= sizeof(ntp_packet_t)) {
        memcpy(&g_resp_packet, p->payload, sizeof(ntp_packet_t));
        g_resp_received = 1;
    }
    pbuf_free(p);
}

/* 发送 NTP 请求（使用指定 PCB 和服务器地址） */
static err_t send_ntp_request(struct udp_pcb *pcb, const ip_addr_t *server_ip) {
    ntp_packet_t packet;
    struct pbuf *p;

    memset(&packet, 0, sizeof(packet));
    packet.li_vn_mode = 0x1B; /* LI=0, VN=3, Mode=3 (客户端) */

    p = pbuf_alloc(PBUF_TRANSPORT, sizeof(packet), PBUF_RAM);
    if (p == NULL) {
        return ERR_MEM;
    }
    memcpy(p->payload, &packet, sizeof(packet));

    err_t err = udp_sendto(pcb, p, server_ip, NTP_PORT);
    pbuf_free(p);
    return err;
}

/* 同步网络时间（阻塞等待响应） */
static int ntp_sync_time(void) {
    ip_addr_t server_ip;
    struct udp_pcb *pcb;
    err_t err;

#ifdef SUPPORT_PCF8563_RTC
#ifdef DRV_CORE
	 if(mrtc_fd<0){
		 mrtc_fd = goldie_open(RTC_DRV_NAME, O_RDWR);
	}
#else
	if(!mrtc_driver){
		mrtc_driver =(Rtc_Driver* )wait_drv(RTC_DRV_INDEX);
		mrtc_driver->drv_init();
	}
#endif
#endif

    /* 解析服务器 IP（此处直接使用 IP，若需域名可添加 DNS 解析） */
    if (!ipaddr_aton(NTP_SERVER_IP, &server_ip)) {
        printf("Invalid NTP server IP\n");
        return -1;
    }

    /* 创建 UDP PCB */
    pcb = udp_new();
    if (pcb == NULL) {
        printf("udp_new failed\n");
        return -1;
    }

    /* 绑定任意本地端口 */
    err = udp_bind(pcb, IP_ADDR_ANY, 0);
    if (err != ERR_OK) {
        printf("udp_bind failed: %d\n", err);
        udp_remove(pcb);
        return -1;
    }

    /* 设置接收回调 */
    g_resp_received = 0;
    udp_recv(pcb, ntp_recv_callback, NULL);

    /* 发送请求 */
    err = send_ntp_request(pcb, &server_ip);
    if (err != ERR_OK) {
        printf("send_ntp_request failed: %d\n", err);
        udp_remove(pcb);
        return -1;
    }

    /* 等待响应（带超时） */
    int timeout = NTP_TIMEOUT_MS / NTP_POLL_INTERVAL_MS;
    while (timeout-- > 0 && !g_resp_received) {
        sys_msleep(NTP_POLL_INTERVAL_MS); /* 平台提供的延时函数 */
    }

    if (!g_resp_received) {
        printf("NTP response timeout\n");
        udp_remove(pcb);
        return -1;
    }

    /* 解析时间 */
    uint32_t ntp_sec = my_htonl(g_resp_packet.trans_timestamp_sec);
    uint32_t unix_time = ntp_sec - NTP_EPOCH_OFFSET;

    /* 转换为 struct tm (使用 UTC 时间) */
    time_t t = (time_t)unix_time;
    struct tm *tm_ptr = gmtime_r(&t, &g_base_time); /* 线程安全版本 */
    if (tm_ptr == NULL) {
        udp_remove(pcb);
        return -1;
    }

    /* 记录同步时刻的 RTC 时间 */
    goldie_gettimeofday(&g_base_rtc);
    g_time_synced = 1;
    printf("ntp_sync_time successed \r\n");
	printf("time sync success! time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
			   g_base_time.tm_year+1900, g_base_time.tm_mon+1, g_base_time.tm_mday,
			   g_base_time.tm_hour, g_base_time.tm_min, g_base_time.tm_sec,
			   g_base_time.tm_wday);	

    /* 释放 PCB */
    udp_remove(pcb);
    printf("NTP sync success, unix time: %lu\n", unix_time);
	
#ifdef SUPPORT_PCF8563_RTC
#ifdef DRV_CORE
	goldie_ioctl(mrtc_fd, PCF8563_IOCTL_SET_TIME, &g_base_time);
#else
    mrtc_driver->set_time(&g_base_time);
#endif
#endif

    return 0;
}

/* 获取当前时间（基于 RTC 差值计算） */
static int ntp_get_time(struct tm *tm_out) {

    goldie_timeval now_rtc;
    if (tm_out == NULL) {
        return -1;
    }

#ifdef SUPPORT_PCF8563_RTC
#ifdef DRV_CORE
                  if(mrtc_fd<0){
		 	mrtc_fd = goldie_open(RTC_DRV_NAME, O_RDWR);
		 }
		if(!goldie_ioctl(mrtc_fd, PCF8563_IOCTL_GET_TIME, tm_out)){
			return 0;
		}
#else
		if(!mrtc_driver){
			mrtc_driver =(Rtc_Driver* )wait_drv(RTC_DRV_INDEX);
			mrtc_driver->drv_init();
		}
		if(!mrtc_driver->read_time(tm_out))
		{
			return 0;
		}else{
		     printf("rtc is error,use system time \r\n");
		}
#endif
#endif

    goldie_gettimeofday(&now_rtc);

    /* 计算 RTC 流逝时间（秒 + 微秒） */
    long sec_diff = now_rtc.tv_sec - g_base_rtc.tv_sec;
    long usec_diff = now_rtc.tv_usec - g_base_rtc.tv_usec;
    if (usec_diff < 0) {
        sec_diff--;
        usec_diff += 1000000;
    }

    /* 将流逝时间加到基准时间戳上 */
    time_t base_ts = mktime(&g_base_time);          /* 转换为 time_t */
    if (base_ts == (time_t)-1)
    {
	base_ts = 0;
    	printf("g_base_time not init \r\n");
    }

    /* 微秒部分四舍五入到秒（或保留微秒，此处简化：秒 + 微秒/1000000） */
    time_t current_ts = base_ts + sec_diff + (usec_diff >= 500000 ? 1 : 0);

    struct tm *result = gmtime_r(&current_ts, tm_out);
#if 1
    printf("ntp_get_time current time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
		   tm_out->tm_year+1900, tm_out->tm_mon+1, tm_out->tm_mday,
		   tm_out->tm_hour, tm_out->tm_min, tm_out->tm_sec,
		   tm_out->tm_wday);
#endif	
    return (result == NULL) ? -1 : 0;
}

/* 全局 NTP 服务实例 */
static NTPService g_ntp_service = {
    .sync_time = ntp_sync_time,
    .get_time  = ntp_get_time,
};

static void ntp_service_entry(void){
	memset(&g_base_time,0,sizeof(g_base_time));
	memset(&g_base_rtc,0,sizeof(g_base_rtc));	
    register_service(NTP_SERVICE_INDEX,(void*)&g_ntp_service);
}
#else /* 非 WS63 平台，提供空实现 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")  /* 链接Winsock库 */
/* NTP常量定义 */
#define NTP_PORT               123
#define NTP_EPOCH_OFFSET       2208988800UL  /* 1900-1970 秒数 */
#define NTP_TIMEOUT_MS          3000

/* NTP包格式（与之前相同） */
typedef struct {
    uint8_t  li_vn_mode;
    uint8_t  stratum;
    uint8_t  poll;
    uint8_t  precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;
    uint32_t ref_timestamp_sec;
    uint32_t ref_timestamp_frac;
    uint32_t orig_timestamp_sec;
    uint32_t orig_timestamp_frac;
    uint32_t recv_timestamp_sec;
    uint32_t recv_timestamp_frac;
    uint32_t trans_timestamp_sec;
    uint32_t trans_timestamp_frac;
} ntp_packet_t;

/* 全局状态 */
static struct tm      g_base_time={0};      /* NTP 同步的绝对时间 */
static goldie_timeval g_base_rtc={0};        /* 同步时刻的 RTC 值 */
static int            g_time_synced = 0; /* 是否已同步 */

/* 字节序转换（Windows网络字节序为大端） */
static uint32_t htonl_custom(uint32_t hostlong) {
    return htonl(hostlong);  /* Winsock提供htonl */
}

static uint32_t ntohl_custom(uint32_t netlong) {
    return ntohl(netlong);
}

/* Windows下的毫秒延时 */
static void sys_msleep(int ms) {
    Sleep(ms);
}

/* 从NTP服务器获取时间（阻塞） */
static int ntp_get_time_from_server(const char *server_ip, struct tm *tm_out) {
    WSADATA wsaData;
    SOCKET sockfd;
    struct sockaddr_in server_addr;
    ntp_packet_t packet = {0};
    int ret = -1;  /* 默认失败 */

    /* 初始化Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return -1;
    }

    /* 创建UDP Socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        printf("socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    /* 设置接收超时 */
    int timeout = NTP_TIMEOUT_MS;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    /* 构造NTP请求包 */
    packet.li_vn_mode = 0x1B;  /* LI=0, VN=3, Mode=3 */

    /* 设置服务器地址 */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NTP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    /* 发送NTP请求 */
    int sent_len = sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
                          (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (sent_len != sizeof(packet)) {
        printf("sendto failed: %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }

    /* 接收响应 */
    int addr_len = sizeof(server_addr);
    int recv_len = recvfrom(sockfd, (char*)&packet, sizeof(packet), 0,
                            (struct sockaddr*)&server_addr, &addr_len);
    if (recv_len >= sizeof(packet)) {
        /* 解析NTP时间戳（网络字节序转主机字节序） */
        uint32_t ntp_sec = ntohl_custom(packet.trans_timestamp_sec);
        uint32_t unix_time = ntp_sec - NTP_EPOCH_OFFSET;

        /* 转换为struct tm（UTC时间） */
        time_t t = (time_t)unix_time;
        int error = gmtime_s(tm_out,&t);  /* Windows可用gmtime_s替代 */
        if (error == 0) {
            ret = 0;  /* 成功 */
	}
    } else {
        printf("recvfrom failed or timeout\n");
    }

    closesocket(sockfd);
    WSACleanup();
    return ret;
}

static int ntp_sync_time_windows(void) {
    struct tm ntp_time;
    goldie_timeval now_rtc;

    /* 从 NTP 服务器获取时间（使用 volc_rtc_auth.h 中定义的 NTP_SERVER） */
    if (ntp_get_time_from_server(NTP_SERVER, &ntp_time) != 0) {
        printf("ntp_sync_time_windows failed \r\n");
        return -1;  /* 同步失败 */
    }

    /* 保存基准时间 */
    g_base_time = ntp_time;
    memcpy(&g_base_time,&ntp_time,sizeof(g_base_time));

    /* 获取同步时刻的RTC时间 */
    goldie_gettimeofday(&g_base_rtc);
    g_time_synced = 1;
    printf("ntp_sync_time_windows successed \r\n");
	printf("time sync success! time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
			   g_base_time.tm_year+1900, g_base_time.tm_mon+1, g_base_time.tm_mday,
			   g_base_time.tm_hour, g_base_time.tm_min, g_base_time.tm_sec,
			   g_base_time.tm_wday);	

    return 0;
}

static int ntp_get_time_windows(struct tm *tm_out) {
    if (tm_out == NULL) return -1;

    /* 如果尚未同步，先触发 NTP 同步 */
    if (!g_time_synced) {
        ntp_sync_time_windows();
    }

    goldie_timeval now_rtc;
    goldie_gettimeofday(&now_rtc);

    /* 计算RTC流逝时间（秒+微秒） */
    long sec_diff = now_rtc.tv_sec - g_base_rtc.tv_sec;
    long usec_diff = now_rtc.tv_usec - g_base_rtc.tv_usec;
    if (usec_diff < 0) {
        sec_diff--;
        usec_diff += 1000000;
    }

    /* 将基准时间（UTC）转换为 time_t，注意使用 _mkgmtime 而非 mktime */
    time_t base_ts = _mkgmtime(&g_base_time);
    if (base_ts == (time_t)-1) {
	printf("ntp_get_time_windows g_base_time not init \r\n");
	base_ts = 0;
    }
    /* 当前时间戳 = 基准时间 + 流逝秒数（微秒四舍五入） */
    time_t current_ts = base_ts + sec_diff + (usec_diff >= 500000 ? 1 : 0);

    /* 转换为 UTC 的 struct tm */
    if (gmtime_s(tm_out,&current_ts) != 0){ 
		printf("ntp_get_time_windows 2\r\n");
		return -1;
    }
#if 1
    printf("ntp_get_time_windows current time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
		   tm_out->tm_year+1900, tm_out->tm_mon+1, tm_out->tm_mday,
		   tm_out->tm_hour, tm_out->tm_min, tm_out->tm_sec,
		   tm_out->tm_wday);
#endif
    return 0;
}

static NTPService g_ntp_service_windows = {
    .sync_time = ntp_sync_time_windows,
    .get_time  = ntp_get_time_windows,
};

static void ntp_service_entry(void){
	memset(&g_base_time,0,sizeof(g_base_time));
	memset(&g_base_rtc,0,sizeof(g_base_rtc));
    	register_service(NTP_SERVICE_INDEX,(void*)&g_ntp_service_windows);
}

#endif /* PLATFORM_TYPE_WS63 */

GOLDIE_INIT_CALL_(ntp_service_entry);
