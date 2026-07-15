#ifndef SLE_DRV_H
#define SLE_DRV_H

#include <stdint.h>
#include "driver_manager.h"
#ifdef SUPPORT_SLE
#include "platform/ws63/bts/sle/sle_connection_manager.h"
#else

#endif
#include "platform/ws63/bts/sle/sle_common.h"
#include "platform/ws63/bts/sle/sle_ssap_client.h"



/* 调试宏定义 */
/* 定义SLE_DEBUG_LEVEL来控制调试信息输出：
   0: 关闭所有调试信息
   1: 只输出错误信息
   2: 输出错误和警告信息
   3: 输出所有调试信息（默认）
*/
#ifndef SLE_DEBUG_LEVEL
#define SLE_DEBUG_LEVEL 1  /* 默认输出所有调试信息 */
#endif

/* 调试输出宏 */
#if SLE_DEBUG_LEVEL >= 3
#define SLE_DEBUG(fmt, ...) printf("\r\n[SLE_DEBUG] " fmt, ##__VA_ARGS__)
#else
#define SLE_DEBUG(fmt, ...) ((void)0)
#endif

#if SLE_DEBUG_LEVEL >= 2
#define SLE_INFO(fmt, ...) printf("\r\n[SLE_INFO] " fmt, ##__VA_ARGS__)
#else
#define SLE_INFO(fmt, ...) ((void)0)
#endif

#if SLE_DEBUG_LEVEL >= 1
#define SLE_ERROR(fmt, ...) printf("\r\n[SLE_ERROR] " fmt, ##__VA_ARGS__)
#else
#define SLE_ERROR(fmt, ...) ((void)0)
#endif

/* 简化调试宏（用于驱动层） */
#define SLE_DRV_DEBUG SLE_DEBUG
#define SLE_DRV_INFO SLE_INFO
#define SLE_DRV_ERROR SLE_ERROR

/* 服务层调试宏 */
#define SLE_SVC_DEBUG SLE_DEBUG
#define SLE_SVC_INFO SLE_INFO
#define SLE_SVC_ERROR SLE_ERROR

#ifdef __cplusplus
extern "C" {
#endif

/* 星闪设备最大连接数 */
#define SLE_MAX_CONNECTIONS 8

/* 星闪设备发现最大设备数 */
#define SLE_MAX_DISCOVERED_DEVICES 10

/* 星闪设备名称最大长度 */
#define SLE_DEVICE_NAME_MAX_LEN 15

/* 使用SDK中定义的sle_addr_t，不再重新定义 */

/* 星闪设备信息结构 */
typedef struct {
    sle_addr_t addr;        /* 设备地址 */
    char name[SLE_DEVICE_NAME_MAX_LEN + 1]; /* 设备名称 */
    int8_t rssi;            /* 信号强度 */
    uint8_t data_len;       /* 广播数据长度 */
    uint8_t *data;          /* 广播数据 */
} sle_device_info_t;

/* 星闪连接信息结构 */
typedef struct {
    uint16_t conn_id;       /* 连接ID */
    sle_addr_t addr;        /* 对端地址 */
    uint8_t state;          /* 连接状态 */
    uint8_t pair_state;     /* 配对状态 */
    int pair_code;
} sle_connection_info_t;

/* 星闪数据接收回调函数类型 */
typedef void (*sle_data_receive_callback)(uint16_t conn_id, const uint8_t *data, uint32_t len);

/* 星闪设备发现回调函数类型 */
typedef void (*sle_device_discovered_callback)(const sle_device_info_t *device);

/* 星闪连接状态回调函数类型 */
typedef void (*sle_connection_state_callback)(uint16_t conn_id, const sle_addr_t *addr, 
    uint8_t conn_state, uint8_t pair_state, uint8_t disc_reason);

/* 发现服务回调函数类型 */
typedef void (*sle_ssapc_find_structure_callback)(uint8_t client_id, uint16_t conn_id,
    ssapc_find_service_result_t *service, uint32_t status);

/* 发现特征回调函数类型 */
typedef void (*sle_ssapc_find_property_callback)(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, uint32_t status);

/* 发现特征完成回调函数类型 */
typedef void (*sle_ssapc_find_structure_complete_callback)(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, uint32_t status);

/* 收到读响应回调函数类型 */
typedef void (*sle_ssapc_read_cfm_callback)(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *read_data,
    uint32_t status);

/* 读特征值完成回调函数类型 */
typedef void (*sle_ssapc_read_by_uuid_complete_callback)(uint8_t client_id, uint16_t conn_id,
    ssapc_read_by_uuid_cmp_result_t *cmp_result, uint32_t status);

/* 收到写响应回调函数类型 */
typedef void (*sle_ssapc_write_cfm_callback)(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result,
    uint32_t status);

/* 更新mtu大小回调函数类型 */
typedef void (*sle_ssapc_exchange_info_callback)(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
    uint32_t status);

/* 通知事件上报回调函数类型 */
typedef void (*sle_ssapc_notification_callback)(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    uint32_t status);

/* 指示事件上报回调函数类型 */
typedef void (*sle_ssapc_indication_callback)(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    uint32_t status);





/* 星闪驱动操作结构体 */
typedef struct SleDrv {
    /* 设备管理接口 */
    int (*init)(void);                              /* 初始化驱动 */
    int (*deinit)(void);                            /* 去初始化驱动 */
    
    /* 设备使能接口 */
    int (*enable)(void);                            /* 使能星闪设备 */
    int (*disable)(void);                           /* 禁用星闪设备 */
    
    /* 设备信息管理接口 */
    int (*set_local_name)(const char *name);        /* 设置本地设备名称 */
    int (*get_local_name)(char *name, uint32_t len); /* 获取本地设备名称 */
    int (*set_local_addr)(const sle_addr_t *addr);  /* 设置本地设备地址 */
    int (*get_local_addr)(sle_addr_t *addr);        /* 获取本地设备地址 */
    
    /* 设备扫描接口 */
    int (*start_scan)(void);                        /* 开始设备扫描 */
    int (*stop_scan)(void);                         /* 停止设备扫描 */
    int (*get_scan_results)(sle_device_info_t *devices, uint32_t *count); /* 获取扫描结果 */
    
    /* 设备连接接口 */
    int (*connect)(const sle_addr_t *addr, uint16_t *conn_id); /* 连接设备，返回conn_id */
    int (*disconnect)(uint16_t conn_id);            /* 断开连接，使用conn_id */
    int (*get_connections)(sle_connection_info_t *conns, uint32_t *count); /* 获取连接列表 */
    
    /* 数据收发接口 */
    int (*send_data)(uint16_t conn_id, const uint8_t *data, uint32_t len); /* 发送数据，使用conn_id */
    int (*register_data_receive_callback)(sle_data_receive_callback cb); /* 注册数据接收回调 */
    
    /* 设备发现回调注册 */
    int (*register_device_discovered_callback)(sle_device_discovered_callback cb); /* 注册设备发现回调 */
    
    /* 连接状态回调注册 */
    int (*register_connection_state_callback)(sle_connection_state_callback cb); /* 注册连接状态回调 */

    /* 发现服务回调注册 */
    int (*register_ssapc_find_structure_callback)(sle_ssapc_find_structure_callback cb);
    
    /* 发现特征回调注册 */
    int (*register_ssapc_find_property_callback)(sle_ssapc_find_property_callback cb);
    
    /* 发现特征完成回调注册 */
    int (*register_ssapc_find_structure_complete_callback)(sle_ssapc_find_structure_complete_callback cb);

    /* 收到读响应回调注册 */
    int (*register_ssapc_read_cfm_callback)(sle_ssapc_read_cfm_callback cb);

    /* 读特征值完成回调注册 */
    int (*register_ssapc_read_by_uuid_complete_callback)(sle_ssapc_read_by_uuid_complete_callback cb);
    
    /* 收到写响应回调注册 */
    int (*register_ssapc_write_cfm_callback)(sle_ssapc_write_cfm_callback cb);

    /* 更新mtu大小回调注册 */
    int (*register_ssapc_exchange_info_callback)(sle_ssapc_exchange_info_callback cb);
    
    /* 通知事件上报回调注册 */
    int (*register_ssapc_notification_callback)(sle_ssapc_notification_callback cb);

    /* 指示事件上报回调注册 */
    int (*register_ssapc_indication_callback)(sle_ssapc_indication_callback cb);

    
    /* 配对管理接口 */
    int (*pair)(const sle_addr_t *addr);            /* 配对设备 */
    int (*unpair)(const sle_addr_t *addr);          /* 取消配对 */
    int (*get_pair_state)(const sle_addr_t *addr, uint8_t *state); /* 获取配对状态 */
    
    /* 高级功能接口 */
    int (*set_tx_power)(int8_t power);              /* 设置发射功率 */
    int (*get_rssi)(uint16_t conn_id, int8_t *rssi); /* 获取信号强度，使用conn_id */
    
    /* 主从一体相关接口 */
    int (*start_announce)(void);                    /* 启动设备公开 */
    int (*stop_announce)(void);                     /* 停止设备公开 */
    int (*is_announcing)(void);                     /* 是否正在设备公开 */
    
    /* 扫描状态查询 */
    int (*is_scanning)(void);                       /* 是否正在扫描 */

    /* 获取最新连接的id */
    int (*get_new_conn_id)(void);
    
    /* 获取SLE模式 */
    uint8_t (*get_mode)(void);
    
    /* 设置SLE模式 */
    void (*set_mode)(uint8_t mode);
    
    /* 切换SLE模式 */
    void (*toggle_mode)(void);
    
} SleDrv;

#ifdef __cplusplus
}
#endif

#endif /* SLE_DRV_H */
