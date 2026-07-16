#define SUPPORT_SLE
#ifdef SUPPORT_SLE
#include "sle_drv.h"
#include "goldie_osal.h"
#include "driver_manager.h"
#include <string.h>

/* 星闪SDK头文件 */
#include "platform/ws63/bts/sle/sle_device_discovery.h"
#include "platform/ws63/bts/sle/sle_connection_manager.h"
#include "platform/ws63/bts/sle/sle_common.h"
#include "platform/ws63/bts/sle/sle_errcode.h"
#include "platform/ws63/bts/sle/sle_ssap_client.h"
#include "platform/ws63/bts/sle/sle_ssap_server.h"

/* 定义errcode.h中需要的宏 */
#define ERRCODE_SUCC 0
/* mtu */
#define SLE_MTU_SIZE_DEFAULT 520
/* 最大广播数据长度 */
#define SLE_ADV_DATA_LEN_MAX 100

/* 广播发送功率 */
#define SLE_ADV_TX_POWER  20

#define SPEED_DEFAULT_CONN_INTERVAL 32
#define SPEED_DEFAULT_TIMEOUT_MULTIPLIER 0x1f4
#define DEFAULT_SLE_SPEED_MCS 0
#define SPEED_DEFAULT_SCAN_INTERVAL 400
#define SPEED_DEFAULT_SCAN_WINDOW 400

typedef enum sle_adv_data {
    SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL                              = 0x01,   /* 发现等级 */
    SLE_ADV_DATA_TYPE_ACCESS_MODE                                  = 0x02,   /* 接入层能力 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_16BIT_UUID                      = 0x03,   /* 标准服务数据信息 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_128BIT_UUID                     = 0x04,   /* 自定义服务数据信息 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_16BIT_SERVICE_UUIDS         = 0x05,   /* 完整标准服务标识列表 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_128BIT_SERVICE_UUIDS        = 0x06,   /* 完整自定义服务标识列表 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_16BIT_SERVICE_UUIDS       = 0x07,   /* 部分标准服务标识列表 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_128BIT_SERVICE_UUIDS      = 0x08,   /* 部分自定义服务标识列表 */
    SLE_ADV_DATA_TYPE_SERVICE_STRUCTURE_HASH_VALUE                 = 0x09,   /* 服务结构散列值 */
    SLE_ADV_DATA_TYPE_SHORTENED_LOCAL_NAME                         = 0x0A,   /* 设备缩写本地名称 */
    SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME                          = 0x0B,   /* 设备完整本地名称 */
    SLE_ADV_DATA_TYPE_TX_POWER_LEVEL                               = 0x0C,   /* 广播发送功率 */
    SLE_ADV_DATA_TYPE_SLB_COMMUNICATION_DOMAIN                     = 0x0D,   /* SLB通信域域名 */
    SLE_ADV_DATA_TYPE_SLB_MEDIA_ACCESS_LAYER_ID                    = 0x0E,   /* SLB媒体接入层标识 */
    SLE_ADV_DATA_TYPE_EXTENDED                                     = 0xFE,   /* 数据类型扩展 */
    SLE_ADV_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA                   = 0xFF    /* 厂商自定义信息 */
} sle_adv_data_type;

/* Service UUID */
#define SLE_UUID_SERVER_SERVICE 0x2222

/* Property UUID */
#define SLE_UUID_SERVER_NTF_REPORT 0x2323

/* Property Property */
#define SLE_UUID_TEST_PROPERTIES  (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

/* Operation indication */
#define SLE_UUID_TEST_OPERATION_INDICATION  (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE)

/* Descriptor Property */
#define SLE_UUID_TEST_DESCRIPTOR   (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

#define SLE_ADV_HANDLE_DEFAULT                    1
/* 连接调度间隔单位125us */
#define SLE_CONN_INTV_MIN_DEFAULT                 32
/* 连接调度间隔单位125us */
#define SLE_CONN_INTV_MAX_DEFAULT                 32
/* 广播调度间隔单位125us */
#define SLE_ADV_INTERVAL_MIN_DEFAULT              400
/* 广播调度间隔单位125us */
#define SLE_ADV_INTERVAL_MAX_DEFAULT              400
/* 超时时间5000ms，单位10ms */
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT      0x1F4
/* 超时时间4990ms，单位10ms */
#define SLE_CONN_MAX_LATENCY                      0x1F3

typedef enum sle_adv_channel {
    SLE_ADV_CHANNEL_MAP_77                 = 0x01,
    SLE_ADV_CHANNEL_MAP_78                 = 0x02,
    SLE_ADV_CHANNEL_MAP_79                 = 0x04,
    SLE_ADV_CHANNEL_MAP_DEFAULT            = 0x07
} sle_adv_channel_map_t;
static ssapc_find_service_result_t   g_sle_find_service_result = {0};
static ssapc_write_param_t g_sle_send_param = { 0 };
static SleDrv sle_drv;
static FIL fp;
/* sle pair acb handle */
uint16_t g_sle_pair_hdl;
/* sle server handle */
static uint8_t g_server_id = 0;
/* sle server app uuid for test */
static char g_sle_uuid_app_uuid[2] = { 0x12, 0x34 };
/* sle service handle */
static uint16_t g_service_handle = 0;
/* sle ntf property handle */
static uint16_t g_property_handle = 0;
/* server notify property uuid for test */
static char g_sle_property_value[8] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
static uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};
static uint8_t g_sle_base[] = { 0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA, 0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static unsigned char local_addr[SLE_ADDR_LEN] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
//主从机模式
//0-client   1-server
static uint8_t sle_mode = 0;//0:主机，1：从机
/* 广播名称 */
static uint8_t sle_local_name[] = "GoldieOS_SLE";

/* 连接映射表项 */
typedef struct {
    uint16_t conn_id;                       /* 连接ID */
    sle_addr_t addr;                        /* 设备地址 */
    uint8_t valid;                          /* 是否有效 */
} sle_conn_map_entry_t;

typedef struct sle_adv_common_value {
    uint8_t type;
    uint8_t length;
    uint8_t value;
} sle_adv_common_t;

/* 内部数据结构定义 */
typedef struct {
    uint8_t initialized;                    /* 驱动是否已初始化 */
    uint8_t enabled;                        /* 设备是否已使能 */
    
    /* 扫描状态 */
    uint8_t scanning;                       /* 是否正在扫描 */
    
    /* 回调函数 */
    sle_data_receive_callback data_cb;      /* 数据接收回调 */
    sle_device_discovered_callback device_discovered_cb; /* 设备发现回调 */
    sle_connection_state_callback connection_state_cb; /* 连接状态回调 */
    /* 发现服务回调函数类型 */
    sle_ssapc_find_structure_callback ssapc_find_structure_callback;
    /* 发现特征回调函数类型 */
    sle_ssapc_find_property_callback ssapc_find_property_callback;
    /* 发现特征完成回调函数类型 */
    sle_ssapc_find_structure_complete_callback ssapc_find_structure_complete_callback;
    /* 收到读响应回调函数类型 */
    sle_ssapc_read_cfm_callback ssapc_read_cfm_callback;
    /* 读特征值完成回调函数类型 */
    sle_ssapc_read_by_uuid_complete_callback ssapc_read_by_uuid_complete_callback;
    /* 收到写响应回调函数类型 */
    sle_ssapc_write_cfm_callback ssapc_write_cfm_callback;
    /* 更新mtu大小回调函数类型 */
    sle_ssapc_exchange_info_callback ssapc_exchange_info_callback;
    /* 通知事件上报回调函数类型 */
    sle_ssapc_notification_callback ssapc_notification_callback;
    /* 指示事件上报回调函数类型 */
    sle_ssapc_indication_callback ssapc_indication_callback;
    
    
    /* 设备公开相关 */
    uint8_t announcing;                     /* 是否正在设备公开 */
    uint8_t announce_id;                    /* 设备公开ID */
    
    /* 连接映射表 */
    sle_conn_map_entry_t conn_map[SLE_MAX_CONNECTIONS]; /* 连接映射表 */
    
} sle_drv_context_t;

/* 驱动上下文实例 */
static sle_drv_context_t g_sle_ctx = {0};

/* 星闪SDK回调函数 */
static sle_announce_seek_callbacks_t g_sle_announce_seek_cbs = {0};
static sle_connection_callbacks_t g_sle_connection_cbs = {0};
static ssapc_callbacks_t g_ssapc_cbs = {0};

/* 内部函数声明 */
static void sle_announce_seek_callback_wrapper(sle_seek_result_info_t *seek_result_data);
static void sle_connect_state_changed_callback_wrapper(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
static void sle_register_ssapc_find_structure_callback_wrapper(uint8_t client_id, uint16_t conn_id, ssapc_find_service_result_t *service,
    errcode_t status);
static void sle_register_ssapc_find_property_callback_wrapper(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status);
static void sle_register_ssapc_find_structure_complete_callback_wrapper(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status);
static void sle_register_ssapc_exchange_info_callback(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
    errcode_t status);
static void sle_register_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);
static void sle_register_notification_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status);
static void sle_register_indication_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status);
static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id,  ssap_exchange_info_t *mtu_size,
    errcode_t status);
static void ssaps_server_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
    errcode_t status);
static void ssaps_server_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
    errcode_t status);
static errcode_t sle_server_add(void);

static int convert_sle_addr(const sle_addr_t *src, sle_addr_t *dst);
static int convert_to_sle_addr(const sle_addr_t *src, sle_addr_t *dst);
static int sle_drv_start_announce(void);
static int sle_drv_stop_announce(void);

/* 连接映射管理函数 */
static int sle_drv_add_conn_mapping(uint16_t conn_id, const sle_addr_t *addr);
static int sle_drv_remove_conn_mapping(uint16_t conn_id);
static int sle_drv_find_conn_id(const sle_addr_t *addr, uint16_t *conn_id);
static int sle_drv_find_addr_by_conn_id(uint16_t conn_id, sle_addr_t *addr);

/* 驱动函数声明 */
static int sle_drv_disable(void);
static void sle_set_default_announce_param(void);
static int sle_set_default_announce_data(void);
static void sle_speed_server_set_nv(void);
static uint8_t sle_drv_get_mode(void);
static void sle_drv_set_mode(uint8_t mode);
static void sle_drv_toggle_mode(void);
static int sle_get_pair_code(uint8_t *mac_addr);
static int sle_drv_set_local_addr(const sle_addr_t *addr);
static void sle_speed_connect_param_init(void);
/* ssapc回调函数注册 */
static void sle_ssapc_cbk_register(void)
{
    g_ssapc_cbs.exchange_info_cb = sle_register_ssapc_exchange_info_callback;
    g_ssapc_cbs.find_structure_cb = sle_register_ssapc_find_structure_callback_wrapper;
    g_ssapc_cbs.find_structure_cmp_cb = sle_register_ssapc_find_structure_complete_callback_wrapper;
    g_ssapc_cbs.ssapc_find_property_cbk = sle_register_ssapc_find_property_callback_wrapper;
    g_ssapc_cbs.notification_cb = sle_register_notification_cb;
    g_ssapc_cbs.indication_cb = sle_register_indication_cb;
}

static void sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = ssaps_server_read_request_cbk;
    ssaps_cbk.write_request_cb = ssaps_server_write_request_cbk;
    ssaps_register_callbacks(&ssaps_cbk);
    
}

/* 驱动初始化 */
static int sle_drv_init(void)
{
    SLE_DRV_DEBUG("sle_drv_init: called\n");
    
    if (g_sle_ctx.initialized) {
        SLE_DRV_INFO("sle_drv_init: already initialized\n");
        return 0; /* 已经初始化 */
    }
    
    /* 初始化上下文 */
    memset(&g_sle_ctx, 0, sizeof(sle_drv_context_t));
    
    /* 注册星闪SDK回调函数 */
    memset(&g_sle_announce_seek_cbs, 0, sizeof(sle_announce_seek_callbacks_t));
    g_sle_announce_seek_cbs.seek_result_cb = sle_announce_seek_callback_wrapper;
    
    memset(&g_sle_connection_cbs, 0, sizeof(sle_connection_callbacks_t));
    g_sle_connection_cbs.connect_state_changed_cb = sle_connect_state_changed_callback_wrapper;
    g_sle_connection_cbs.pair_complete_cb = sle_register_pair_complete_cbk;

    SLE_DRV_DEBUG("sle_drv_init: registering announce/seek callbacks\n");
    errcode_t ret = sle_announce_seek_register_callbacks(&g_sle_announce_seek_cbs);
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_init: sle_announce_seek_register_callbacks failed, ret=0x%x\n", ret);
        return -1;
    }
    
    SLE_DRV_DEBUG("sle_drv_init: registering connection callbacks\n");
    ret = sle_connection_register_callbacks(&g_sle_connection_cbs);
    
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_init: sle_connection_register_callbacks failed, ret=0x%x\n", ret);
        return -1;
    }

    //server
    sle_speed_server_set_nv();
    sle_ssaps_register_cbks();
    sle_server_add();
    ssap_exchange_info_t parameter = { 0 };
    parameter.mtu_size = SLE_MTU_SIZE_DEFAULT;
    parameter.version = 1;
    ssaps_set_info(g_server_id, &parameter);
    //client
    sle_speed_connect_param_init();
    memset(&g_ssapc_cbs,0,sizeof(ssapc_callbacks_t));
    sle_ssapc_cbk_register();
    ssapc_register_callbacks(&g_ssapc_cbs);

    g_sle_ctx.initialized = 1;
    SLE_DRV_INFO("sle_drv_init: initialized (mode: %s)\n", sle_drv_get_mode() == 0 ? "master" : "slave");
    return 0;
}

/* 驱动去初始化 */
static int sle_drv_deinit(void)
{
    if (!g_sle_ctx.initialized) {
        return 0;
    }
    
    /* 如果设备已使能，先禁用 */
    if (g_sle_ctx.enabled) {
        sle_drv_disable();
    }
    
    /* 清空上下文 */
    memset(&g_sle_ctx, 0, sizeof(sle_drv_context_t));
    
    return 0;
}

/* 使能星闪设备 */
static int sle_drv_enable(void)
{
    if (g_sle_ctx.enabled) {
        return 0; /* 已经使能 */
    }

    errcode_t ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    if (!g_sle_ctx.initialized) {
        int ret = sle_drv_init();
        if (ret != 0) {
            return ret;
        }
    }
    
    g_sle_ctx.enabled = 1;

    SLE_DRV_INFO("sle_drv_enable: enabled with slave mode )\n");
    return 0;
}

/* 禁用星闪设备 */
static int sle_drv_disable(void)
{
    if (!g_sle_ctx.enabled) {
        return 0; /* 已经禁用 */
    }
    
    /* 停止扫描 */
    if (g_sle_ctx.scanning) {
        sle_stop_seek();
        g_sle_ctx.scanning = 0;
    }
    
    /* 停止设备公开 */
    if (g_sle_ctx.announcing) {
        sle_stop_announce(g_sle_ctx.announce_id);
        g_sle_ctx.announcing = 0;
    }
    
    /* 注意：不再管理连接列表，连接管理由服务层负责 */
    
    errcode_t ret = disable_sle();
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    g_sle_ctx.enabled = 0;
    SLE_DRV_INFO("sle_drv_disable: disabled\n");
    return 0;
}

/* 设置本地设备名称 */
static int sle_drv_set_local_name(const char *name)
{
    if (!g_sle_ctx.enabled) {
        return -1;
    }
    
    if (name == NULL) {
        return -1;
    }
    
    uint8_t name_len = strlen(name);
    if (name_len > SLE_NAME_MAX_LEN) {
        name_len = SLE_NAME_MAX_LEN;
    }
    
    errcode_t ret = sle_set_local_name((const uint8_t *)name, name_len);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    return 0;
}

/* 获取本地设备名称 */
static int sle_drv_get_local_name(char *name, uint32_t len)
{
    if (!g_sle_ctx.enabled || name == NULL || len == 0) {
        return -1;
    }
    
    uint8_t name_len = len - 1; /* 预留结束符 */
    if (name_len > SLE_NAME_MAX_LEN) {
        name_len = SLE_NAME_MAX_LEN;
    }
    
    uint8_t actual_len = name_len;
    errcode_t ret = sle_get_local_name((uint8_t *)name, &actual_len);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    name[actual_len] = '\0';
    return 0;
}

/* 设置本地设备地址 */
static int sle_drv_set_local_addr(const sle_addr_t *addr)
{
    if (!g_sle_ctx.enabled || addr == NULL) {
        return -1;
    }

    sle_addr_t sle_addr;
    if (convert_to_sle_addr(addr, &sle_addr) != 0) {
        return -1;
    }
    memcpy(local_addr,addr->addr,SLE_ADDR_LEN);
	
printf("sle_drv_set_local_addr %x:%x:%x:%x:%x:%x \r\n ",sle_addr.addr[0],
	sle_addr.addr[1],
	sle_addr.addr[2],
	sle_addr.addr[3],
	sle_addr.addr[4],
	sle_addr.addr[5]
	);
    errcode_t ret = sle_set_local_addr(&sle_addr);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    return 0;
}

/* 获取本地设备地址 */
static int sle_drv_get_local_addr(sle_addr_t *addr)
{
    if (!g_sle_ctx.enabled || addr == NULL) {
        return -1;
    }
    
    sle_addr_t sle_addr;
    errcode_t ret = sle_get_local_addr(&sle_addr);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    return convert_sle_addr(&sle_addr, addr);
}


static uint16_t sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    int ret;
    uint8_t index = 0;
    uint8_t *local_name = sle_local_name;
    uint8_t local_name_len = sizeof(sle_local_name) - 1;
    SLE_DRV_INFO("local_name_len = %d\r\n", local_name_len);
    SLE_DRV_INFO("local_name: ");
    for (uint8_t i = 0; i < local_name_len; i++) {
        SLE_DRV_INFO("0x%02x ", local_name[i]);
    }
    SLE_DRV_INFO("\r\n");
    adv_data[index++] = local_name_len + 1;
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    ret = memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len);
    if (ret != 0) {
        SLE_DRV_INFO("memcpy fail\r\n");
        return 0;
    }
    return (uint16_t)index + local_name_len;
}


static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data)
{
    uint16_t idx = 0;
    int ret;
    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);

    struct sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,
        .value = SLE_ADV_TX_POWER,
    };
    ret = memcpy_s(scan_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != 0) {
        SLE_ERROR("%s sle scan response data memcpy fail\r\n");
        return 0;
    }
    idx += scan_rsp_data_len;

    /* set local name */
    idx += sle_set_adv_local_name(&scan_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx);
    return idx;
}

/* 启动设备公开 */
static int sle_drv_start_announce(void)
{
    if (!g_sle_ctx.enabled) {
        return -1;
    }

    sle_set_default_announce_param();
    sle_set_default_announce_data();
    int ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    if (ret != ERRCODE_SLE_SUCCESS) {
        SLE_ERROR("sle_server_adv_init,sle_start_announce fail :%x\r\n", ret);
        return ret;
    }
   
    
    g_sle_ctx.announcing = 1;
    g_sle_ctx.announce_id = 1;
    
    SLE_DRV_INFO(": started (no name in advertising data)\n");
    return 0;
}

static uint16_t sle_set_adv_data(uint8_t *adv_data)
{
    size_t len = 0;
    uint16_t idx = 0;
    int  ret = 0;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,
        .value = SLE_ANNOUNCE_LEVEL_NORMAL,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_disc_level, len);
    if (ret != 0) {
        SLE_ERROR("%s adv_disc_level memcpy fail\r\n");
        return 0;
    }
    idx += len;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE,
        .value = 0,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_access_mode, len);
    if (ret != 0) {
       SLE_ERROR("adv_access_mode memcpy fail\r\n");
        return 0;
    }
    idx += len;

    return idx;
}

static void sle_set_default_announce_param(){
    int ret;
    sle_announce_param_t param = {0};
    uint8_t index;
    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = SLE_ADV_HANDLE_DEFAULT;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announce_interval_min = SLE_ADV_INTERVAL_MIN_DEFAULT;
    param.announce_interval_max = SLE_ADV_INTERVAL_MAX_DEFAULT;
    param.conn_interval_min = SLE_CONN_INTV_MIN_DEFAULT;
    param.conn_interval_max = SLE_CONN_INTV_MAX_DEFAULT;
    param.conn_max_latency = SLE_CONN_MAX_LATENCY;
    param.conn_supervision_timeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT;
    param.announce_tx_power = SLE_ADV_TX_POWER;
    param.own_addr.type = 0;
    ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    if (ret != 0) {
        SLE_ERROR("%s sle_set_default_announce_param data memcpy fail\r\n");
        return 0;
    }
    SLE_INFO("%s 11 sle local addr: ");
    for (index = 0; index < SLE_ADDR_LEN; index++) {
        SLE_INFO("0x%02x ", param.own_addr.addr[index]);
    }
    SLE_INFO("\r\n");
    sle_set_announce_param(param.announce_handle, &param);
}

static int sle_set_default_announce_data(void)
{
    int ret;
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;
    sle_announce_data_t data = {0};
    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t data_index = 0;

    announce_data_len = sle_set_adv_data(announce_data);
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;

    SLE_INFO(" data.announce_data_len = %d\r\n", data.announce_data_len);
    SLE_INFO(" data.announce_data: ");
    for (data_index = 0; data_index<data.announce_data_len; data_index++) {
        SLE_INFO("0x%02x ", data.announce_data[data_index]);
    }
    SLE_INFO("\r\n");

    seek_data_len = sle_set_scan_response_data(seek_rsp_data);
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = seek_data_len;

    SLE_INFO(" data.seek_rsp_data_len = %d\r\n", data.seek_rsp_data_len);
    SLE_INFO(" data.seek_rsp_data: ");
    for (data_index = 0; data_index<data.seek_rsp_data_len; data_index++) {
        SLE_INFO("0x%02x ", data.seek_rsp_data[data_index]);
    }
    SLE_INFO("\r\n");

    ret = sle_set_announce_data(adv_handle, &data);
    if (ret == ERRCODE_SLE_SUCCESS) {
        SLE_INFO(" set announce data success.\r\n" );
    } else {
        SLE_ERROR(" set adv param fail.\r\n");
    }
    return ERRCODE_SLE_SUCCESS;
}

/* 停止设备公开 */
static int sle_drv_stop_announce(void)
{
    if (!g_sle_ctx.enabled || !g_sle_ctx.announcing) {
        return 0;
    }

    errcode_t ret = sle_stop_announce(g_sle_ctx.announce_id);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }

    g_sle_ctx.announcing = 0;
    SLE_DRV_INFO("sle_drv_stop_announce: stopped\n");
    return 0;
}

static void sle_speed_connect_param_init(void)
{
    sle_default_connect_param_t param = {0};
    param.enable_filter_policy = 0;
    param.gt_negotiate = 0;
    param.initiate_phys = 1;
    param.max_interval = SPEED_DEFAULT_CONN_INTERVAL;
    param.min_interval = SPEED_DEFAULT_CONN_INTERVAL;
    param.scan_interval = SPEED_DEFAULT_SCAN_INTERVAL;
    param.scan_window = SPEED_DEFAULT_SCAN_WINDOW;
    param.timeout = SPEED_DEFAULT_TIMEOUT_MULTIPLIER;
    sle_default_connection_param_set(&param);
}

/* 开始设备扫描 */
int sle_drv_start_scan(void)
{
    SLE_DRV_DEBUG("sle_drv_start_scan: called, enabled=%d, scanning=%d\n", g_sle_ctx.enabled, g_sle_ctx.scanning);
    
    if (!g_sle_ctx.enabled) {
        SLE_DRV_ERROR("sle_drv_start_scan: SLE not enabled\n");
        return -1;
    }
    
    /* 设置默认扫描参数 - 参考sle_hybrid_demo */
    sle_seek_param_t scan_param = {0};
    scan_param.own_addr_type = SLE_ADDRESS_TYPE_PUBLIC;
    scan_param.filter_duplicates = 0; /* 不过滤重复设备 */
    scan_param.seek_filter_policy = SLE_SEEK_FILTER_ALLOW_ALL;
    scan_param.seek_phys = SLE_SEEK_PHY_1M;
    scan_param.seek_type[0] = SLE_SEEK_PASSIVE; /* 被动扫描 */
    scan_param.seek_interval[0] = SPEED_DEFAULT_SCAN_INTERVAL; /* 扫描间隔 */
    scan_param.seek_window[0] = SPEED_DEFAULT_SCAN_INTERVAL;    /* 扫描窗口 */
    
    SLE_DRV_DEBUG("sle_drv_start_scan: setting seek params\n");
    errcode_t ret = sle_set_seek_param(&scan_param);
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_start_scan: sle_set_seek_param failed, ret=0x%x\n", ret);
        return -1;
    }
    
    SLE_DRV_DEBUG("sle_drv_start_scan: starting seek\n");
    ret = sle_start_seek();
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_start_scan: sle_start_seek failed, ret=0x%x\n", ret);
        return -1;
    }
    
    g_sle_ctx.scanning = 1;
    SLE_DRV_INFO("sle_drv_start_scan: scan started successfully\n");
    return 0;
}

/* 停止设备扫描 */
static int sle_drv_stop_scan(void)
{
    SLE_DRV_DEBUG("sle_drv_stop_scan: called, scanning=%d\n", g_sle_ctx.scanning);
    
    if (!g_sle_ctx.enabled) {
        SLE_DRV_ERROR("sle_drv_stop_scan: SLE not enabled\n");
        return -1;
    }
    
    if (!g_sle_ctx.scanning) {
        SLE_DRV_INFO("sle_drv_stop_scan: not scanning\n");
        return 0; /* 没有在扫描，直接返回成功 */
    }
    
    errcode_t ret = sle_stop_seek();
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_stop_scan: sle_stop_seek failed, ret=0x%x\n", ret);
        return -1;
    }
    
    g_sle_ctx.scanning = 0;
    SLE_DRV_INFO("sle_drv_stop_scan: scan stopped successfully\n");
    return 0;
}

/* 获取扫描结果 */
static int sle_drv_get_scan_results(sle_device_info_t *devices, uint32_t *count)
{
    /* 驱动不再管理设备列表，设备列表由服务层管理 */
    /* 这个函数应该返回空，或者返回错误 */
    (void)devices;
    
    if (count != NULL) {
        *count = 0;
    }
    
    SLE_DRV_DEBUG("sle_drv_get_scan_results: device list managed by service layer, returning empty list\n");
    return 0;
}

/* 连接设备 - 同步接口，返回conn_id */
static int sle_drv_connect(const sle_addr_t *addr, uint16_t *conn_id)
{
    if (!g_sle_ctx.enabled || addr == NULL || conn_id == NULL) {
        return -1;
    }

    sle_addr_t sle_addr;
    if (convert_to_sle_addr(addr, &sle_addr) != 0) {
        return -1;
    }
    
    /* 检查地址是否已存在连接 */
    uint16_t existing_conn_id = 0;
    if (sle_drv_find_conn_id(&sle_addr, &existing_conn_id) == 0) {
        /* 地址已存在连接，返回现有的conn_id */
        SLE_DRV_INFO("sle_drv_connect: address already connected, conn_id=%d\n", existing_conn_id);
        *conn_id = existing_conn_id;
        return 0; /* 返回成功，表示连接已存在 */
    }
    
    /* 注意：sle_connect_remote_device是异步接口，连接建立后会在回调中返回conn_id */
    /* 这里需要等待连接建立，然后从回调中获取conn_id */
    /* 简化实现：先发起连接，然后等待回调 */
    errcode_t ret = sle_connect_remote_device(&sle_addr);
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_connect: sle_connect_remote_device failed, ret=0x%x\n", ret);
        return -1;
    }
    
    /* 由于是异步接口，这里无法立即返回conn_id */
    /* 实际实现需要等待连接建立回调，然后返回conn_id */
    /* 简化实现：返回0表示成功，conn_id会在回调中通知 */
    *conn_id = 0; /* 临时值，实际应该在回调中设置 */
    
    SLE_DRV_INFO("sle_drv_connect: connection initiated, conn_id will be provided in callback\n");
    return 0;
}

/* 断开连接 - 使用conn_id */
static int sle_drv_disconnect(uint16_t conn_id)
{
    if (!g_sle_ctx.enabled) {
        return -1;
    }
    
    /* 根据conn_id查找设备地址 */
    sle_addr_t addr;
    if (sle_drv_find_addr_by_conn_id(conn_id, &addr) != 0) {
        SLE_DRV_ERROR("sle_drv_disconnect: no mapping found for conn_id=%d\n", conn_id);
        return -1;
    }
    
    /* 调用星闪SDK接口断开连接 */
    errcode_t ret = sle_disconnect_remote_device(&addr);
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_disconnect: sle_disconnect_remote_device failed, ret=0x%x\n", ret);
        return -1;
    }
    
    SLE_DRV_INFO("sle_drv_disconnect: disconnecting conn_id=%d\n", conn_id);
    return 0;
}

/* 获取连接列表 */
static int sle_drv_get_connections(sle_connection_info_t *conns, uint32_t *count)
{
    /* 首先检查全局上下文是否有效 */
    if (&g_sle_ctx == NULL) {
        if (count != NULL) {
            *count = 0;
        }
        SLE_DRV_ERROR("sle_drv_get_connections: g_sle_ctx is NULL\n");
        return -1;
    }
    
    if (!g_sle_ctx.initialized) {
        if (count != NULL) {
            *count = 0;
        }
        SLE_DRV_ERROR("sle_drv_get_connections: SLE driver not initialized\n");
        return -1;
    }
    
    if (!g_sle_ctx.enabled || conns == NULL || count == NULL) {
        if (count != NULL) {
            *count = 0;
        }
        return -1;
    }
    
    uint32_t max_count = *count;
    uint32_t actual_count = 0;
    
    /* 遍历连接映射表，收集有效的连接 */
    for (int i = 0; i < SLE_MAX_CONNECTIONS; i++) {
        if (g_sle_ctx.conn_map[i].valid && actual_count < max_count) {
            conns[actual_count].conn_id = g_sle_ctx.conn_map[i].conn_id;
            memcpy(&conns[actual_count].addr, &g_sle_ctx.conn_map[i].addr, sizeof(sle_addr_t));
            conns[actual_count].state = 1; /* 已连接状态 */
            conns[actual_count].pair_state = 0; /* 默认未配对，实际应从SDK获取 */
            actual_count++;
        }
    }
    
    *count = actual_count;
    
    //SLE_DRV_INFO("sle_drv_get_connections: returning %d connections\n", actual_count);
    return 0;
}

/* 发送数据 - 使用conn_id */
static int sle_drv_send_data(uint16_t conn_id, const uint8_t *data, uint32_t len)
{
    if (!g_sle_ctx.enabled || data == NULL || len == 0) {
        return -1;
    }
    
    /* TODO: 实现SSAP数据发送，需要调用以下接口之一：
       - ssapc_write_cmd: 发送写命令（无响应）
       - ssapc_write_req: 发送写请求（有响应）
       由于需要SSAP client注册和连接管理，这里先返回成功 */
    if(sle_drv_get_mode() == 0){
        g_sle_send_param.data_len = len;
        g_sle_send_param.data = data;
        ssapc_write_cmd(0, conn_id, &g_sle_send_param);   
        printf("sle_drv_send_data mode = 0,ssapc_write_cmd \r\n");
    }else{
        ssaps_ntf_ind_t param = {0};
        param.handle = g_property_handle;
        param.type = SSAP_PROPERTY_TYPE_VALUE;
        param.value = data;
        param.value_len = len;
        ssaps_notify_indicate(g_server_id, conn_id, &param);
	printf("sle_drv_send_data mode =1,ssaps_notify_indicate \r\n");
    }
    
    return 0;
}

/* 配对设备 */
static int sle_drv_pair(const sle_addr_t *addr)
{
    if (!g_sle_ctx.enabled || addr == NULL) {
        return -1;
    }
    
    sle_addr_t sle_addr;
    if (convert_to_sle_addr(addr, &sle_addr) != 0) {
        return -1;
    }
    
    errcode_t ret = sle_pair_remote_device(&sle_addr);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    return 0;
}

/* 取消配对 */
static int sle_drv_unpair(const sle_addr_t *addr)
{
    if (!g_sle_ctx.enabled || addr == NULL) {
        return -1;
    }
    
    sle_addr_t sle_addr;
    if (convert_to_sle_addr(addr, &sle_addr) != 0) {
        return -1;
    }
    
    errcode_t ret = sle_remove_paired_remote_device(&sle_addr);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    return 0;
}

/* 获取配对状态 */
static int sle_drv_get_pair_state(const sle_addr_t *addr, uint8_t *state)
{
    if (!g_sle_ctx.enabled || addr == NULL || state == NULL) {
        return -1;
    }
    
    sle_addr_t sle_addr;
    if (convert_to_sle_addr(addr, &sle_addr) != 0) {
        return -1;
    }
    
    sle_pair_state_t pair_state;
    errcode_t ret = sle_get_pair_state(&sle_addr, &pair_state);
    if (ret != ERRCODE_SUCC) {
        return -1;
    }
    
    *state = (uint8_t)pair_state;
    return 0;
}

/* 设置发射功率 */
static int sle_drv_set_tx_power(int8_t power)
{
    if (!g_sle_ctx.enabled) {
        return -1;
    }
    
    /* 注意：星闪SDK的set_tx_power接口可能需要不同的参数格式 */
    /* 简化实现：返回成功 */
    SLE_DRV_INFO("sle_drv_set_tx_power: setting tx power to %d dBm\n", power);
    return 0;
}

/* 获取信号强度 - 使用conn_id */
static int sle_drv_get_rssi(uint16_t conn_id, int8_t *rssi)
{
    if (!g_sle_ctx.enabled || rssi == NULL) {
        return -1;
    }
    
    /* 根据conn_id查找设备地址 */
    sle_addr_t addr;
    if (sle_drv_find_addr_by_conn_id(conn_id, &addr) != 0) {
        SLE_DRV_ERROR("sle_drv_get_rssi: no mapping found for conn_id=%d\n", conn_id);
        return -1;
    }
    
    SLE_DRV_DEBUG("sle_drv_get_rssi: getting RSSI for device, conn_id=%d, addr=", conn_id);
    for (int i = 0; i < SLE_ADDR_LEN; i++) {
        SLE_DRV_DEBUG("%02X:", addr.addr[i]);
    }
    SLE_DRV_DEBUG("\n");
    
    /* 调用星闪SDK接口读取RSSI */
    errcode_t ret = sle_read_remote_device_rssi(conn_id);
    if (ret != ERRCODE_SUCC) {
        SLE_DRV_ERROR("sle_drv_get_rssi: sle_read_remote_device_rssi failed, ret=0x%x\n", ret);
        /* 如果SDK接口失败，返回默认值 */
        *rssi = -60;
        return -1;
    }
    
    /* 注意：sle_read_remote_device_rssi是异步接口，RSSI值会在回调中返回 */
    /* 这里暂时返回默认值，实际实现需要等待回调 */
    *rssi = -60; /* 默认RSSI值 */
    
    return 0;
}

/* 是否正在扫描 */
static int sle_drv_is_scanning(void)
{
    return g_sle_ctx.scanning ? 1 : 0;
}

/* 是否正在设备公开 */
static int sle_drv_is_announcing(void)
{
    return g_sle_ctx.announcing ? 1 : 0;
}

/* SLE模式读写函数实现 - 驱动层实现 */
static uint8_t sle_drv_get_mode(void)
{
    return sle_mode;
}

static void sle_drv_set_mode(uint8_t mode)
{
    if (mode <= 1) { /* 只允许0或1 */
        sle_mode = mode;
    }
}

/* SLE模式切换函数 - 驱动层实现 */
static void sle_drv_toggle_mode(void)
{
    /* 切换sle_mode值：0->1 或 1->0 */
    uint8_t current_mode = sle_drv_get_mode();
    if (current_mode == 0) {
        sle_drv_set_mode(1);
    } else {
        sle_drv_set_mode(0);
    }
}

/* 注册数据接收回调 */
static int sle_drv_register_data_receive_callback(sle_data_receive_callback cb)
{
    g_sle_ctx.data_cb = cb;
    return 0;
}

/* 注册设备发现回调 */
static int sle_drv_register_device_discovered_callback(sle_device_discovered_callback cb)
{
    g_sle_ctx.device_discovered_cb = cb;
    return 0;
}

/* 注册连接状态回调 */
static void sle_drv_register_connection_state_callback(sle_connection_state_callback cb)
{
    g_sle_ctx.connection_state_cb = cb;
    return 0;
}

/* 发现服务回调注册 */
static void sle_register_ssapc_find_structure_callback_wrapper(uint8_t client_id, uint16_t conn_id, ssapc_find_service_result_t *service,
    errcode_t status){
    SLE_DRV_INFO("find structure cbk client: %d conn_id:%d status: %d \r\n",
                client_id, conn_id, status);
    SLE_DRV_INFO("find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n",
                service->start_hdl, service->end_hdl, service->uuid.len);
    g_sle_find_service_result.start_hdl = service->start_hdl;
    g_sle_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_sle_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
    if(g_sle_ctx.ssapc_find_structure_callback!=NULL){
        g_sle_ctx.ssapc_find_structure_callback(client_id, conn_id, service, status);
    }
}

/* 发现特征回调注册 */
static void sle_register_ssapc_find_property_callback_wrapper(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status){
        SLE_DRV_INFO("sle_client_sample_find_property_cbk, client id: %d, conn id: %d, operate ind: %d, "
                "descriptors count: %d status:%d property->handle %d\r\n",
                client_id, conn_id, property->operate_indication,
                property->descriptors_count, status, property->handle);
    g_sle_send_param.handle = property->handle;
    g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
    if(g_sle_ctx.ssapc_find_property_callback!=NULL){
        g_sle_ctx.ssapc_find_property_callback(client_id, conn_id, property, status);
    }
}

/* 发现特征完成回调注册 */
static void sle_register_ssapc_find_structure_complete_callback_wrapper(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status){
     unused(conn_id);
    SLE_DRV_INFO(" sle_client_sample_find_structure_cmp_cbk,client id:%d status:%d type:%d uuid len:%d \r\n",
                client_id, status, structure_result->type, structure_result->uuid.len); 
    if(g_sle_ctx.ssapc_find_structure_complete_callback!=NULL){
        g_sle_ctx.ssapc_find_structure_complete_callback(client_id,conn_id,structure_result,status);
    }
}

/* 更新mtu大小回调注册 */
static void sle_register_ssapc_exchange_info_callback(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
    errcode_t status){
    SLE_DRV_INFO("[ssap client] pair complete client id:%d status:%d\n", client_id, status);
    SLE_DRV_INFO("[ssap client] exchange mtu, mtu size: %d, version: %d.\n",
        param->mtu_size, param->version);

    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    ssapc_find_structure(0, conn_id, &find_param);
    if(g_sle_ctx.ssapc_exchange_info_callback != NULL){
        g_sle_ctx.ssapc_exchange_info_callback(client_id, conn_id, param,status);
    }
}

/* 配对完成回调注册 */
static void  sle_register_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    SLE_DRV_INFO("pair complete conn_id:%d, addr:%02x***%02x%02x\n", conn_id,
                addr->addr[0], addr->addr[4], addr->addr[5]);
	#if 1
	if(sle_mode == 0){
		//client
	        ssap_exchange_info_t info = {0};
	        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
	        info.version = 1;
	        ssapc_exchange_info_req(0, conn_id, &info);
	}else{
	        //server
	        sle_set_phy_t phy_parm = {
	        .tx_format = SLE_RADIO_FRAME_2,
	        .rx_format = SLE_RADIO_FRAME_2,
	        .tx_phy = SLE_PHY_1M,//SLE_PHY_4M,
	        .rx_phy = SLE_PHY_1M,//SLE_PHY_4M,
	        .tx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1,
	        .rx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1,
	        .g_feedback = 0,
	        .t_feedback = 0,
	        };
	        sle_set_phy_param(conn_id, &phy_parm);
	        sle_set_mcs(conn_id, DEFAULT_SLE_SPEED_MCS);
	        sle_set_data_len(conn_id, SLE_MTU_SIZE_DEFAULT);
	}
	#endif
}

/* 解析从机信息数据 - 已移动到sle_service.c */
/* 保存从机信息到文件 - 已移动到sle_service.c */

/* 接收通知回调注册 */
static void sle_register_notification_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(client_id);
    unused(status);
    
    if (data == NULL || data->data == NULL) {
        return;
    }
    
    /* 将接收到的数据传递给服务层 */
    if (g_sle_ctx.data_cb != NULL) {
        g_sle_ctx.data_cb(conn_id, (const uint8_t *)data->data, data->data_len);
    }
}

/* 接收指示回调注册 */
static void sle_register_indication_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    SLE_DRV_INFO("\n sle recived indication data : %s\r\n", data->data);
}



static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id,  ssap_exchange_info_t *mtu_size,
    errcode_t status)
{
    SLE_DRV_INFO(" ssaps ssaps_mtu_changed_cbk callback server_id:%x, conn_id:%x, mtu_size:%x, status:%x\r\n",
         server_id, conn_id, mtu_size->mtu_size, status);
    if (g_sle_pair_hdl == 0) {
        g_sle_pair_hdl = conn_id + 1;
    }
}

static void ssaps_server_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
    errcode_t status)
{
    SLE_DRV_INFO(" ssaps read request cbk callback server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        server_id, conn_id, read_cb_para->handle, status);
}

static void ssaps_server_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
    errcode_t status)
{
    if ((write_cb_para->length > 0) && write_cb_para->value) {
        /* 将接收到的数据传递给服务层 */
        if (g_sle_ctx.data_cb != NULL) {
            g_sle_ctx.data_cb(conn_id, (const uint8_t *)write_cb_para->value, write_cb_para->length);
        }
    }
}

/* 内部回调函数包装器 - 设备发现回调 */
static void sle_announce_seek_callback_wrapper(sle_seek_result_info_t *seek_result_data)
{
    
    if (seek_result_data == NULL) {
        SLE_DRV_ERROR("sle_announce_seek_callback_wrapper: seek_result_data is NULL\n");
        return;
    }
    
    /* 打印调试信息 */
    SLE_DRV_DEBUG("sle_announce_seek_callback_wrapper: device found - addr_type=%d, rssi=%d, data_len=%d\n",
           seek_result_data->addr.type, seek_result_data->rssi, seek_result_data->data_length);
    
    /* 创建临时设备结构体 */
    sle_device_info_t device;
    memset(&device, 0, sizeof(sle_device_info_t));
    
    /* 转换地址 */
    convert_sle_addr(&seek_result_data->addr, &device.addr);
    
    /* 保存RSSI */
    device.rssi = seek_result_data->rssi;
    
    /* 注意：不再提取设备名称，设备名称由服务层管理 */
    /* 设备名称字段留空 */
    device.name[0] = '\0';
    
    /* 保存广播数据长度（不保存实际数据） */
    device.data_len = seek_result_data->data_length;
    device.data = NULL; /* 不保存广播数据 */
    /* 调用用户回调 */
    if (g_sle_ctx.device_discovered_cb != NULL) {
        SLE_DRV_DEBUG("sle_announce_seek_callback_wrapper: calling user callback\n");
        g_sle_ctx.device_discovered_cb(&device);
    }
    sle_drv_stop_scan();
}

/* 内部回调函数包装器 - 连接状态变化回调 */
static void sle_connect_state_changed_callback_wrapper(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    SLE_DRV_DEBUG("sle_connect_state_changed_callback_wrapper: called, conn_id=%d, conn_state=%d, pair_state=%d\n",
           conn_id, conn_state, pair_state);
    
    if (addr != NULL) {
        SLE_DRV_DEBUG("Remote addr: ");
        for (int i = 0; i < SLE_ADDR_LEN; i++) {
            SLE_DRV_DEBUG("%02X:", addr->addr[i]);
        }
        SLE_DRV_DEBUG("\n");
    }
    
    /* 转换地址格式 */
    sle_addr_t converted_addr;
    if (convert_sle_addr(addr, &converted_addr) != 0) {
        SLE_DRV_ERROR("sle_connect_state_changed_callback_wrapper: failed to convert address\n");
        return;
    }
    sle_addr_t addrs;
    addrs.type = 0;
    memcpy(addrs.addr,addr->addr,6);
    
    /* 管理连接映射 */
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        /* 连接建立，添加映射 */
        sle_drv_add_conn_mapping(conn_id, &converted_addr); 
	   if(sle_mode == 0){
	        //client
		ssap_exchange_info_t info = {0};
		info.mtu_size = SLE_MTU_SIZE_DEFAULT;
		info.version = 1;
		sle_pair_remote_device(addr);
		ssapc_exchange_info_req(0, conn_id, &info);
	   }else{
            //server
#if 1
		sle_connection_param_update_t parame = {0};
		parame.conn_id = conn_id;
		parame.interval_min = SPEED_DEFAULT_CONN_INTERVAL;
		parame.interval_max = SPEED_DEFAULT_CONN_INTERVAL;
		parame.max_latency = 0;
		parame.supervision_timeout = SPEED_DEFAULT_TIMEOUT_MULTIPLIER;
		sle_set_phy_t phy_parm = {
		.tx_format = SLE_RADIO_FRAME_2,
		.rx_format = SLE_RADIO_FRAME_2,
		.tx_phy = SLE_PHY_1M,//SLE_PHY_4M,
		.rx_phy = SLE_PHY_1M,//SLE_PHY_4M,
		.tx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1,
		.rx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1,
		.g_feedback = 0,
		.t_feedback = 0,
		};
		sle_set_phy_param(conn_id, &phy_parm);
		sle_set_mcs(conn_id, DEFAULT_SLE_SPEED_MCS);
		sle_set_data_len(conn_id, SLE_MTU_SIZE_DEFAULT);
#endif
	}
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        /* 连接断开，移除映射 */
        sle_drv_remove_conn_mapping(conn_id);
	 if(sle_mode == 0){	
        sle_remove_paired_remote_device(&addrs);
		sle_drv_start_scan();
	}else{
		 sle_drv_start_announce();
		}
    }
    
   
    /* 调用用户回调 */
    if (g_sle_ctx.connection_state_cb != NULL) {
        SLE_DRV_DEBUG("sle_connect_state_changed_callback_wrapper: calling user callback\n");
        g_sle_ctx.connection_state_cb(conn_id, &converted_addr, conn_state, pair_state, disc_reason);
    }
}

static void encode2byte_little(uint8_t *_ptr, uint16_t data)
{
    *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 0x8);
    *(uint8_t *)(_ptr) = (uint8_t)(data);
}

static void sle_uuid_set_base(sle_uuid_t *out)
{
    errcode_t ret;
    ret = memcpy_s(out->uuid, SLE_UUID_LEN, g_sle_base, SLE_UUID_LEN);
    if (ret != 0) {
        SLE_ERROR("sle_uuid_set_base memcpy fail\n");
        out->len = 0;
        return ;
    }
    out->len = 2;
}

static void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out);
    out->len = 2;
    encode2byte_little(&out->uuid[14], u2);
}

static void sle_speed_server_set_nv(void)
{
    uint16_t nv_value_len = 0;
    uint8_t nv_value = 0;
    uapi_nv_read(0x20A0, sizeof(uint16_t), &nv_value_len, &nv_value);
    if (nv_value != 7) {     // 7:btc功率档位
        nv_value = 7;       // 7:btc功率档位
        uapi_nv_write(0x20A0, (uint8_t *)&(nv_value), sizeof(nv_value));
    }
    printf("[speed server] The value of nv is set to %d.\r\n", nv_value);
}

static errcode_t sle_uuid_server_service_add(void)
{
    errcode_t ret;
    sle_uuid_t service_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        SLE_ERROR("sle uuid add service fail, ret:%x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t sle_uuid_server_property_add(void)
{
    errcode_t ret;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SLE_UUID_TEST_PROPERTIES;
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);
    property.value = (uint8_t *)osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value,
        sizeof(g_sle_property_value)) != 0) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property,  &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        SLE_ERROR("sle add property fail, ret:%x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    descriptor.permissions = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);
    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        SLE_ERROR("sle add descriptor fail, ret:%x\r\n",  ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }
    osal_vfree(property.value);
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t sle_server_add(void)
{
    errcode_t ret;
    sle_uuid_t app_uuid = {0};
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != 0) {
        return ERRCODE_SLE_FAIL;
    }
    
    ret = ssaps_register_server(&app_uuid, &g_server_id);
    
    /* 检查ssaps_register_server是否成功 */
    if (ret != ERRCODE_SLE_SUCCESS) {
        SLE_ERROR("ssaps_register_server failed, ret:%x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }

    if (sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    if (sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    SLE_INFO(" sle add service, server_id:%x, service_handle:%x, property_handle:%x\r\n",
         g_server_id, g_service_handle, g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        SLE_ERROR(" sle add service fail, ret:%x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

/* 地址转换函数 */
static int convert_sle_addr(const sle_addr_t *src, sle_addr_t *dst)
{
    if (src == NULL || dst == NULL) {
        return -1;
    }
    
    memcpy(dst, src, sizeof(sle_addr_t));
    return 0;
}

static int convert_to_sle_addr(const sle_addr_t *src, sle_addr_t *dst)
{
    if (src == NULL || dst == NULL) {
        return -1;
    }
    
    memcpy(dst, src, sizeof(sle_addr_t));
    return 0;
}

/* 连接映射管理函数 */

/* 添加连接映射 */
static int sle_drv_add_conn_mapping(uint16_t conn_id, const sle_addr_t *addr)
{
    if (addr == NULL) {
        return -1;
    }
    
    /* 查找空闲位置 */
    for (int i = 0; i < SLE_MAX_CONNECTIONS; i++) {
        if (!g_sle_ctx.conn_map[i].valid) {
            g_sle_ctx.conn_map[i].conn_id = conn_id;
            memcpy(&g_sle_ctx.conn_map[i].addr, addr, sizeof(sle_addr_t));
            g_sle_ctx.conn_map[i].valid = 1;
            
            SLE_DRV_DEBUG("sle_drv_add_conn_mapping: added mapping conn_id=%d -> addr=", conn_id);
            for (int j = 0; j < SLE_ADDR_LEN; j++) {
                SLE_DRV_DEBUG("%02X:", addr->addr[j]);
            }
            SLE_DRV_DEBUG("\n");
            
            return 0;
        }
    }
    
    SLE_DRV_ERROR("sle_drv_add_conn_mapping: no space for new mapping\n");
    return -1;
}

/* 移除连接映射 */
static int sle_drv_remove_conn_mapping(uint16_t conn_id)
{
    for (int i = 0; i < SLE_MAX_CONNECTIONS; i++) {
        if (g_sle_ctx.conn_map[i].valid && g_sle_ctx.conn_map[i].conn_id == conn_id) {
            g_sle_ctx.conn_map[i].valid = 0;
            SLE_DRV_DEBUG("sle_drv_remove_conn_mapping: removed mapping for conn_id=%d\n", conn_id);
            return 0;
        }
    }
    
    SLE_DRV_ERROR("sle_drv_remove_conn_mapping: mapping not found for conn_id=%d\n", conn_id);
    return -1;
}

/* 根据地址查找conn_id */
static int sle_drv_find_conn_id(const sle_addr_t *addr, uint16_t *conn_id)
{
    if (addr == NULL || conn_id == NULL) {
        return -1;
    }
    
    for (int i = 0; i < SLE_MAX_CONNECTIONS; i++) {
        if (g_sle_ctx.conn_map[i].valid) {
            if (memcmp(&g_sle_ctx.conn_map[i].addr, addr, sizeof(sle_addr_t)) == 0) {
                *conn_id = g_sle_ctx.conn_map[i].conn_id;
                return 0;
            }
        }
    }
    
    return -1;
}

/* 根据conn_id查找地址 */
static int sle_drv_find_addr_by_conn_id(uint16_t conn_id, sle_addr_t *addr)
{
    if (addr == NULL) {
        return -1;
    }
    
    for (int i = 0; i < SLE_MAX_CONNECTIONS; i++) {
        if (g_sle_ctx.conn_map[i].valid && g_sle_ctx.conn_map[i].conn_id == conn_id) {
            memcpy(addr, &g_sle_ctx.conn_map[i].addr, sizeof(sle_addr_t));
            return 0;
        }
    }
    
    return -1;
}

/* 驱动操作结构体 */
static SleDrv sle_drv = {
    .init = sle_drv_init,
    .deinit = sle_drv_deinit,
    .enable = sle_drv_enable,
    .disable = sle_drv_disable,
    .set_local_name = sle_drv_set_local_name,
    .get_local_name = sle_drv_get_local_name,
    .set_local_addr = sle_drv_set_local_addr,
    .get_local_addr = sle_drv_get_local_addr,
    .start_scan = sle_drv_start_scan,
    .stop_scan = sle_drv_stop_scan,
    .is_scanning = sle_drv_is_scanning,
    .get_scan_results = sle_drv_get_scan_results,
    .connect = sle_drv_connect,
    .disconnect = sle_drv_disconnect,
    .get_connections = sle_drv_get_connections,
    .send_data = sle_drv_send_data,
    .start_announce = sle_drv_start_announce,
    .stop_announce = sle_drv_stop_announce,
    .is_announcing = sle_drv_is_announcing,
    .pair = sle_drv_pair,
    .unpair = sle_drv_unpair,
    .get_pair_state = sle_drv_get_pair_state,
    .set_tx_power = sle_drv_set_tx_power,
    .get_rssi = sle_drv_get_rssi,
    .get_mode = sle_drv_get_mode,
    .set_mode = sle_drv_set_mode,
    .toggle_mode = sle_drv_toggle_mode,
    .register_data_receive_callback = sle_drv_register_data_receive_callback,
    .register_device_discovered_callback = sle_drv_register_device_discovered_callback,
    .register_connection_state_callback = sle_drv_register_connection_state_callback,
};

/* 驱动注册函数 */
void register_sle_drv(void)
{
    SLE_DRV_INFO("register_sle_drv: registering SLE driver\n");
    register_drv(SLE_DRV_INDEX, &sle_drv);
}

/* 系统初始化时自动注册驱动 */
//GOLDIE_INIT_CALL_(register_sle_drv);
#endif

