/**
 * @file convai_platform_ws63.c
 * @brief WS63 platform abstraction implementation.
 *
 * 基于goldieos项目架构实现:
 *  - OSAL: goldie_osal (LiteOS API封装)
 *  - NetAL: lwIP sockets
 *  - TLSAL: mbedTLS
 */

#include "convai_platform_ws63.h"
#include "goldie_osal.h"
#include "services/ntp/ntp_service.h"
#include "core/service_manager.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NTP_SERVICE_INDEX 7

static int g_hal_initialized = 0;
static uint64_t g_start_time_ms = 0;

/* ===== Opaque type definitions (must match SDK internal layout) ===== */
struct convai_mutex_s {
    goldie_mutex mutex;
};

struct convai_thread_s {
    void *handle;
};

struct convai_socket_s {
    mbedtls_net_context net;
};

/* Manual implementation of timegm for platforms that don't have it (like WS63) */
static time_t my_timegm(const struct tm *tm) {
    static const int mon_lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    int year = tm->tm_year + 1900 - 1970;
    if (year < 0) return (time_t)-1;
    
    time_t days = 0;
    
    for (int y = 1970; y < tm->tm_year + 1900; y++) {
        days += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            days++;
        }
    }
    
    for (int m = 0; m < tm->tm_mon; m++) {
        days += mon_lengths[m];
        if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
            days++;
        }
    }
    
    days += tm->tm_mday - 1;
    
    time_t seconds = days * 24 * 3600;
    seconds += tm->tm_hour * 3600;
    seconds += tm->tm_min * 60;
    seconds += tm->tm_sec;
    
    return seconds;
}

/* ===== OSAL – Memory ===== */
static void *ws63_malloc(size_t size) {
    return goldie_malloc(size);
}

static void ws63_free(void *ptr) {
    goldie_free(ptr);
}

/* ===== OSAL – Time ===== */

extern void* get_service(int service_index);
NTPService* ntp_service = NULL;
uint64_t ws63_get_time_ms(void) {
    uint64_t timestamp_s = 0;
    
    struct tm tm_now;
    if (!ntp_service) {
        ntp_service = get_service(NTP_SERVICE_INDEX);
    }

    if (ntp_service && ntp_service->get_time(&tm_now) == 0) {
        time_t t = my_timegm(&tm_now);
        
        timestamp_s = (uint64_t)t - 86400;
    }
    return timestamp_s * 1000;
}

static void ws63_sleep_ms(uint32_t ms) {
    goldie_msleep((int)ms);
}

/* ===== OSAL – Mutex ===== */
static int ws63_mutex_create(convai_mutex_t **mutex) {
    if (mutex == NULL) return -1;
    convai_mutex_t *m = (convai_mutex_t*)malloc(sizeof(*m));
    if (m == NULL) return -1;
    goldie_mutex_init(&m->mutex);
    *mutex = m;
    return 0;
}

static void ws63_mutex_destroy(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_destroy(&mutex->mutex);
    free(mutex);
}

static void ws63_mutex_lock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_lock(&mutex->mutex);
}

static void ws63_mutex_unlock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_unlock(&mutex->mutex);
}

/* ===== OSAL – Thread ===== */
static int ws63_thread_wrapper(void *data) {
    void **args = (void **)data;
    convai_thread_func_t func = (convai_thread_func_t)args[0];
    void *arg = args[1];
    goldie_free(args);
    if (func) func(arg);
    return 0;
}

static int ws63_thread_create(convai_thread_t **thread,
                              convai_thread_func_t func, void *arg,
                              const char *name, size_t stack_size, int priority) {
    (void)priority;
    if (thread == NULL || func == NULL) return -1;

    void **args = (void**)goldie_malloc(2 * sizeof(void*));
    if (args == NULL) return -1;
    args[0] = (void*)func;
    args[1] = arg;

    unsigned int ss = stack_size > 0 ? stack_size : 4096;
    const char *n = name ? name : "convai";

    *thread = (convai_thread_t*)goldie_thread_create(
        (goldie_thread_handler)ws63_thread_wrapper, args, n, ss);
    if (*thread == NULL) {
        goldie_free(args);
        return -1;
    }
    return 0;
}

static void ws63_thread_join(convai_thread_t *thread) {
    (void)thread;
}

static void ws63_thread_destroy(convai_thread_t *thread) {
    if (thread == NULL) return;
    goldie_free(thread);
}

/* ===== OSAL – Misc ===== */
static int ws63_fill_random(uint8_t *buf, size_t len) {
    if (buf == NULL) return -1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
    return 0;
}

static char *ws63_strdup(const char *s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = (char*)goldie_malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

/* ===== NetAL – Socket ===== */
static int ws63_socket_create(convai_socket_t **sock) {
    if (sock == NULL) return -1;
    *sock = (convai_socket_t*)goldie_malloc(sizeof(**sock));
    if (*sock == NULL) return -1;
    memset(*sock, 0, sizeof(**sock));
    mbedtls_net_init(&(*sock)->net);
    return 0;
}

static int ws63_socket_destroy(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    mbedtls_net_free(&sock->net);
    goldie_free(sock);
    return 0;
}

static int ws63_socket_connect(convai_socket_t *sock, const char *host, uint16_t port) {
    if (sock == NULL || host == NULL) return -1;
    char service[6];
    snprintf(service, sizeof(service), "%u", port);
    return mbedtls_net_connect(&sock->net, host, service, MBEDTLS_NET_PROTO_TCP);
}

static int ws63_socket_send(convai_socket_t *sock, const uint8_t *buf, size_t len, size_t *sent) {
    if (sent) *sent = 0;
    if (sock == NULL || buf == NULL) return -1;
    int ret = mbedtls_net_send(&sock->net, buf, len);
    if (ret < 0) return ret;
    if (sent) *sent = (size_t)ret;
    return 0;
}

static int ws63_socket_recv(convai_socket_t *sock, uint8_t *buf, size_t len, size_t *recvd) {
    if (recvd) *recvd = 0;
    if (sock == NULL || buf == NULL) return -1;
    int ret = mbedtls_net_recv(&sock->net, buf, len);
    if (ret < 0) return ret;
    if (recvd) *recvd = (size_t)ret;
    return 0;
}

static int ws63_socket_set_nonblock(convai_socket_t *sock, int non_block) {
    (void)sock;
    (void)non_block;
    return 0;
}

static int ws63_socket_is_connected(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    return sock->net.fd >= 0 ? 1 : 0;
}

static int ws63_socket_get_fd(convai_socket_t *sock) {
    if (sock == NULL) return -1;
    return sock->net.fd;
}

/* ===== TLSAL (stub) ===== */
static int ws63_tls_create(convai_tls_t **tls) { (void)tls; return 0; }
static int ws63_tls_destroy(convai_tls_t *tls) { (void)tls; return 0; }
static int ws63_tls_connect(convai_tls_t *tls, void *sock, const char *host) { (void)tls; (void)sock; (void)host; return 0; }
static int ws63_tls_read(convai_tls_t *tls, uint8_t *buf, size_t len, size_t *nread) { (void)tls; (void)buf; (void)len; if (nread) *nread = 0; return 0; }
static int ws63_tls_write(convai_tls_t *tls, const uint8_t *buf, size_t len, size_t *nwrite) { (void)tls; (void)buf; (void)len; if (nwrite) *nwrite = 0; return 0; }
static int ws63_tls_close(convai_tls_t *tls) { (void)tls; return 0; }

/* ===== Misc ===== */
static void ws63_log(int level, const char *file, int line, const char *fmt, ...) {
    char buf[256];
    va_list args;
    uint64_t now_ms = ws63_get_time_ms();
    uint32_t sec = (uint32_t)(now_ms / 1000);
    uint32_t ms = (uint32_t)(now_ms % 1000);
    int pos = snprintf(buf, sizeof(buf), "[%u.%03u] [%c] [%s:%d] ",
                       sec, ms,
                       level == 0 ? 'E' : level == 1 ? 'W' : level == 2 ? 'I' : 'D',
                       file ? file : "???", line);
    va_start(args, fmt);
    vsnprintf(buf + pos, sizeof(buf) - pos - 2, fmt, args);
    va_end(args);
    int len = strlen(buf);
    if (len < (int)sizeof(buf) - 1) {
        buf[len] = '\n';
        buf[len + 1] = '\0';
    }
    printf("%s", buf);
}

static int ws63_device_id(char *buf, size_t len) {
    if (buf == NULL || len == 0) return -1;
    snprintf(buf, len, "ws63-device");
    return (int)strlen(buf);
}

static int ws63_random(uint8_t *buf, size_t len) {
    return ws63_fill_random(buf, len);
}

static int ws63_uuid(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "00000000-0000-0000-0000-000000000000");
    return (int)strlen(buf);
}

static int ws63_info(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "ws63-liteos");
    return (int)strlen(buf);
}

static int ws63_network_available(void) {
    return 1;
}

static int ws63_network_get_type(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "wifi");
    return (int)strlen(buf);
}

/* ===== Platform structure instance ===== */
const convai_platform_t g_convai_platform = {
    .abi_version = CONVAI_ABI_VERSION,
    ._reserved = 0,
    .osal = {
        .malloc = ws63_malloc,
        .free = ws63_free,
        .get_time_ms = ws63_get_time_ms,
        .sleep_ms = ws63_sleep_ms,
        .mutex_create = ws63_mutex_create,
        .mutex_destroy = ws63_mutex_destroy,
        .mutex_lock = ws63_mutex_lock,
        .mutex_unlock = ws63_mutex_unlock,
        .thread_create = ws63_thread_create,
        .thread_join = ws63_thread_join,
        .thread_destroy = ws63_thread_destroy,
        .fill_random = ws63_fill_random,
        .strdup = ws63_strdup,
    },
    .netal = {
        .socket_create = ws63_socket_create,
        .socket_destroy = ws63_socket_destroy,
        .socket_connect = ws63_socket_connect,
        .socket_send = ws63_socket_send,
        .socket_recv = ws63_socket_recv,
        .socket_set_nonblock = ws63_socket_set_nonblock,
        .socket_is_connected = ws63_socket_is_connected,
        .socket_get_fd = ws63_socket_get_fd,
    },
    .tlsal = {
        .tls_create = ws63_tls_create,
        .tls_destroy = ws63_tls_destroy,
        .tls_connect = ws63_tls_connect,
        .tls_read = ws63_tls_read,
        .tls_write = ws63_tls_write,
        .tls_close = ws63_tls_close,
    },
    .misc = {
        .log = ws63_log,
        .device_id = ws63_device_id,
        .random = ws63_random,
        .uuid = ws63_uuid,
        .info = ws63_info,
        .network_available = ws63_network_available,
        .network_get_type = ws63_network_get_type,
    },
};

int convai_platform_ws63_init(void) {
    return convai_platform_init(&g_convai_platform);
}