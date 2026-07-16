#ifndef SLE_SERVICE_H
#define SLE_SERVICE_H

#include <stdint.h>
#include "service_manager.h"
#include "platform/ws63/sle_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDP_DATA_HEAD   0xfe0e

#define SDP_MAGIC_NUM0 0x3040

#define SDP_MAGIC_NUM1 0x0403

enum SLE_PROFILE{
	SLE_PROFILE_INDEX_SDP =0,
	SLE_PROFILE_INDEX_WKP,
	SLE_PROFILE_INDEX_RFCOMM,
	SLE_PROFILE_INDEX_A2DP,
	SLE_PROFILE_INDEX_HSP,
	SLE_PROFILE_INDEX_MAX,
};

#define MAX_DEVICES 10
#define PAIR_TIMEOUT 10000

typedef struct{
	sle_addr_t  addr;
	int pair_code;
}sle_pair_data;

/* SLE配置结构 */
typedef struct {
    uint8_t enabled;         // 是否启用（对应复选框状态）
    uint8_t mode;           // 当前模式（主机/从机）
} SleConfig;


/* 配置管理函数声明 */
//extern int save_sle_config(void);
/* 连接类型定义 */
typedef enum {
    SLE_CONN_TYPE_NONE = 0,         /* 无连接 */
    SLE_CONN_TYPE_ACTIVE,           /* 主动连接（本设备发起） */
    SLE_CONN_TYPE_PASSIVE,          /* 被动连接（其他设备发起） */
} SleConnType;

/* 星闪事件类型 */
typedef enum {
    SLE_EVENT_ENABLE = 0,           /* 使能星闪 */
    SLE_EVENT_DISABLE,              /* 禁用星闪 */
    SLE_EVENT_START_SCAN,                 /* 开始扫描 */
    SLE_EVENT_STOP_SCAN,
    SLE_EVENT_CONNECT,              /* 连接设备 */
    SLE_EVENT_DISCONNECT,           /* 断开连接 */
    SLE_EVENT_SEND_DATA,            /* 发送数据 */
    SLE_EVENT_RECEIVE_DATA,         /* 接收数据 */
    SLE_EVENT_START_ANNOUNCE,       /* 启动设备公开 */
    SLE_EVENT_STOP_ANNOUNCE,        /* 停止设备公开 */
    SLE_EVENT_PAIR,                 /* 配对设备 */
    SLE_EVENT_UNPAIR,               /* 取消配对 */
    SLE_EVENT_SLAVE_PAIRED,
} SleEventType;

/* 星闪状态类型 */
typedef enum {
    SLE_STATUS_DISABLED = 0,        /* 星闪禁用 */
    SLE_STATUS_ENABLED,             /* 星闪使能 */
    SLE_STATUS_SCANNING,            /* 正在扫描 */
    SLE_STATUS_SCAN_DONE,           /* 扫描完成 */
    SLE_STATUS_ANNOUNCING,          /* 正在设备公开 */
    SLE_STATUS_ANNOUNCE_STOPPED,    /* 设备公开停止 */
    SLE_STATUS_DEVICE_LIST_UPDATED, /* 设备列表已更新 */
    SLE_STATUS_CONNECTION_LIST_UPDATED, /* 连接列表已更新 */
    SLE_STATUS_PAIRED_STATUS_UPDATED,
} SleStatusType;

/* 星闪事件结构 */
typedef struct {
    SleEventType type;              /* 事件类型 */
    void *data;                     /* 事件数据 */
} SleEvent;

enum{
	SDP_MSG_ID_PAIR =0,
	SDP_MSG_ID_PAIR_RSP,
	SDP_MSG_ID_GET_INFO,
	SDP_MSG_ID_GET_INFO_RSP,
	SDP_MSG_ID_MAX,
};

typedef struct{
	uint16_t profile_mask;
	uint8_t name[25];
}sdp_devinfo_msg;

typedef struct{
	int  pair_code;	
}sdp_pair_msg;

typedef struct{
	int  pair_result;	
}sdp_pair_rsp_msg;


typedef struct{
	uint16_t data_head;
	uint16_t magic_num0;
	uint16_t magic_num1;
	uint32_t msg_type;
	uint16_t msg_len;
}sdp_data_format_head;


typedef struct{
	uint16_t data_head;
	uint16_t magic_num0;
	uint16_t magic_num1;
	uint32_t msg_type;
	uint16_t msg_len;
	uint16_t buffer[1];
}sdp_data_format;

typedef struct {
    uint16_t data_head;
    uint16_t magic_num0;
    uint16_t magic_num1;
    uint16_t data_len;
    uint16_t data_index;
    uint16_t buffer[1];
} sle_wtp_data_format;

typedef struct {
    uint16_t data_head;
    uint16_t magic_num0;
    uint16_t magic_num1;
    uint16_t data_len;
    uint16_t data_index;
} sle_wtp_data_head_format;

typedef struct{
	uint16_t profile_id;
	uint16_t data_len;
	uint16_t  buffer[1];
}sleMultiProfileDataFormat;

typedef struct{
	uint16_t profile_id;
	uint16_t data_len;
}sleMultiProfileDataFormat_head;


typedef struct{
	uint16_t  conn_id;
	uint16_t data_len;
	sleMultiProfileDataFormat buffer;
}sle_data_format;

typedef struct{
	uint16_t  conn_id;
	uint16_t data_len;
}sle_data_format_head;



/* 星闪事件回调函数类型 */
typedef void (*SleEventCallback)(SleStatusType status, void *data);

/* 星闪数据接收回调函数类型 */
typedef void (*SleDataReceiveCallback)(uint16_t conn_id, const uint8_t *data, uint32_t len);

/* 连接信息扩展结构 */
typedef struct {
    sle_connection_info_t conn_info;    /* 基础连接信息 */
    SleConnType conn_type;              /* 连接类型：主动/被动 */
    uint8_t is_active;                  /* 是否活跃 */
} SleExtendedConnInfo;

/* 星闪服务操作结构体 */
typedef struct SleSdpService {
    /* 设备管理接口 */
    int (*svr_enable)(void);                            /* 使能星闪 */
    int (*svr_disable)(void);                           /* 禁用星闪 */
    bool (*svr_is_enabled)(void);                        /* 是否已使能 */
    
    /* 设备信息管理接口 */
    int (*svr_set_local_name)(const char *name);        /* 设置本地设备名称 */
    int (*svr_get_local_name)(sle_addr_t *addr); /* 获取本地设备名称 */
    
    /* 设备扫描接口 */
    int (*svr_start_scan)(void);                        /* 开始设备扫描 */
    int (*svr_stop_scan)(void);                         /* 停止设备扫描 */
    int (*svr_is_scanning)(void);                       /* 是否正在扫描 */

    int (*svr_get_scan_results)(sle_device_info_t *devices, uint32_t *count); /* 获取扫描结果 */
    int (*svr_get_paired_devices)(sle_connection_info_t *devices, uint32_t *count);
    int (*svr_get_paired_and_discovered_devices)(sle_device_info_t *devices, uint32_t *count);	
   
    /* 设备连接接口 */
    int (*svr_connect)(const sle_addr_t *addr);         /* 连接设备（主动连接） */
    int (*svr_disconnect)(uint16_t conn_id);            /* 断开连接 */
    int (*svr_get_connections)(sle_connection_info_t *conns, uint32_t *count); /* 获取连接列表 */
    
    /* 数据收发接口 */
    int (*svr_send_data)(uint16_t conn_id, const uint8_t *data, uint32_t len); /* 发送数据 */
    
    /* 设备公开接口（主从一体） */
    int (*svr_start_announce)(void);                    /* 启动设备公开 */
    int (*svr_stop_announce)(void);                     /* 停止设备公开 */
    int (*svr_is_announcing)(void);                     /* 是否正在设备公开 */
    
    int (*svr_get_pair_state)(const sle_addr_t *addr); /* 获取配对状态 */
    
    /* 高级功能接口 */
    int (*svr_set_tx_power)(int8_t power);              /* 设置发射功率 */
    int (*svr_get_rssi)(uint16_t conn_id, int8_t *rssi); /* 获取信号强度 */
    
    /* 事件管理接口 */
    int (*register_callback)(SleEventCallback cb,int);      /* 注册事件回调 */
    int (*register_data_receive_callback)(SleDataReceiveCallback cb,int); /* 注册数据接收回调 */
    int (*set_recording_page_status)(int status);       /* 设置录音页面状态 */
    int (*trigger_event)(SleEventType type, void *data); /* 触发事件 */
    
    /* 连接状态检查接口 */
    int (*svr_is_connected)(const sle_addr_t *addr); /* 检查指定设备是否已连接 */
    
    /* SLE模式管理接口 */
    uint8_t (*svr_get_mode)(void);                     /* 获取SLE模式 */
    void (*svr_set_mode)(uint8_t mode);                /* 设置SLE模式 */
    /*从机生成配对码*/
    int (*svr_s_generate_pair_code)();
}SleSdpService;

#ifdef __cplusplus
}
#endif

#endif /* SLE_SERVICE_H */
