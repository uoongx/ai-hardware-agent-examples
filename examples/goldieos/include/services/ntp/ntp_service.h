#ifndef NTP_SERVICE_H
#define NTP_SERVICE_H
#include "goldie_osal.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NTP 服务接口结构体 */
typedef struct NTPService {
    int (*sync_time)(void);                 /* 同步网络时间，成功返回0，失败返回1 */
    int (*get_time)(struct tm*);             /* 获取当前时间（填充 tm 结构），成功返回0，失败返回1 */
} NTPService;

/* 获取全局 NTP 服务实例 */
NTPService* get_ntp_service(void);

#ifdef __cplusplus
}
#endif

#endif /* NTP_SERVICE_H */