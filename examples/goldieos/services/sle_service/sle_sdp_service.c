#define SUPPORT_SLE
#ifdef SUPPORT_SLE
#include "goldie_osal.h"
#include "service_manager.h"
#include "sle_sdp_service.h"
#include "driver_manager.h"
#include "platform/ws63/sle_drv.h"  /* 包含调试宏定义 */
#include "ff.h"   /* FATFS文件系统API */
#include <string.h>
#include <stdlib.h>

#define KEEP_ACTIVE_FOREVER
#define SLE_SWITCH_DISABLE
static uint16_t g_profile_mask = 0x1;	

static sle_connection_info_t pair_devices[MAX_DEVICES]={0};

static int pair_device_count = 0;

/* 默认设备名称 */
#define DEFAULT_SLE_NAME "GoldieOS"

/* 事件队列大小*/
#define SLE_EVENT_QUEUE_SIZE 64

/* 连接限制配置 */
#define MAX_ACTIVE_CONNECTIONS  3  /* 最大主动连接数 */
#define MAX_PASSIVE_CONNECTIONS 3  /* 最大被动连接数 */

/* 事件队列 */
static SleEvent event_queue[SLE_EVENT_QUEUE_SIZE];
static int event_queue_head = 0;
static int event_queue_tail = 0;
static goldie_mutex event_queue_mutex;
static goldie_mutex event_cb_mutex;
static goldie_mutex data_cb_mutex;

/* 状态跟踪 */
static bool last_enabled_status = false;
static int last_scanning_status = -1;
static int last_announcing_status = -1;

/* 连接状态管理 */
#define MAX_TRACKED_CONNECTIONS 8
static sle_connection_info_t tracked_connections[MAX_TRACKED_CONNECTIONS];
static uint32_t tracked_connection_count = 0;
static uint32_t last_tracked_connection_count = 0;

/* 连接管理 */
static SleExtendedConnInfo active_connection = {0};
static SleExtendedConnInfo passive_connection = {0};
static uint8_t max_active_connections = MAX_ACTIVE_CONNECTIONS;
static uint8_t max_passive_connections = MAX_PASSIVE_CONNECTIONS;

/* 设备发现管理 */
#define MAX_MANAGED_DEVICES 20
static sle_device_info_t managed_devices[MAX_MANAGED_DEVICES];
static uint32_t managed_device_count = 0;
static uint8_t device_list_updated = 0; /* 设备列表更新标记 */

static sle_connection_info_t sle_info;

/* 简单的延迟发送管理 */
static uint8_t pending_send_conn_id = 0;
static sle_addr_t pending_send_addr;
static uint8_t pending_send_valid = 0;

/*配对码*/
static int sle_pair_code = 0;

/* 回调函数 */
static SleEventCallback sle_event_callback[SLE_PROFILE_INDEX_MAX] = {NULL};

/* 数据接收回调函数 */
static SleDataReceiveCallback sle_service_data_receive_callback[SLE_PROFILE_INDEX_MAX] = {NULL};

/* 录音页面状态标志 */
static int is_in_recording_page = 0;

static unsigned char local_addr[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

/* 驱动指针 */
static SleDrv* sle_drv = NULL;

/* SLE全局配置变量 */
static SleConfig sle_config = {0};

static uint8_t config_loaded = 0;
/* SLE配置保存控制标志 */
uint8_t should_save_sle_config = 0;

static void send_pair_msg(int pair_code,uint8_t conn_id);
/* 事件队列操作函数原型声明 */
static void enqueue_event(SleEventType type, void* data);
static int dequeue_event(SleEvent* event);

/* 配对码处理函数声明 */
static void handle_pair_code_command(uint16_t conn_id, const uint8_t *data, uint32_t data_len);
static int svr_set_max_connections(uint8_t max_active, uint8_t max_passive);
/*获取配对码*/
static int svr_s_generate_pair_code();
static void sdp_data_process(uint16_t conn_id, const uint8_t *data, uint32_t data_len);

void update_pair_status_with_connid(uint16_t conn_id, int pair_status) {
    for (int i = 0; i < pair_device_count; i++) {
        if (pair_devices[i].conn_id == conn_id) {
            pair_devices[i].pair_state = pair_status;
            return;
        }
    }
    return;
}

void update_pair_status(const unsigned char *addr, int pair_status) {
    for (int i = 0; i < pair_device_count; i++) {
        if (memcmp(pair_devices[i].addr.addr, addr, SLE_ADDR_LEN) == 0) {
            pair_devices[i].pair_state = pair_status;
            return;
        }
    }
	
    if (pair_device_count < MAX_DEVICES) {
        memcpy(pair_devices[pair_device_count].addr.addr, addr, SLE_ADDR_LEN);
	pair_devices[pair_device_count].pair_state = pair_status;
         pair_devices[pair_device_count].state = 0;	
	pair_devices[pair_device_count].pair_code = 0;
	pair_devices[pair_device_count].conn_id = -1;
        pair_device_count++;
    }
}

void update_pair_code(const unsigned char *addr, int pair_code) {
    for (int i = 0; i < pair_device_count; i++) {
        if (memcmp(pair_devices[i].addr.addr, addr, SLE_ADDR_LEN) == 0) {
            pair_devices[i].pair_code = pair_code;
            return;
        }
    }
	
    if (pair_device_count < MAX_DEVICES) {
        memcpy(pair_devices[pair_device_count].addr.addr, addr, SLE_ADDR_LEN);
	pair_devices[pair_device_count].pair_state = 0;
        pair_devices[pair_device_count].state = 0;	
	pair_devices[pair_device_count].pair_code = pair_code;
	pair_devices[pair_device_count].conn_id = -1;
        pair_device_count++;
    }
}

void update_conn_status(const unsigned char *addr, int conn_status,uint8_t conn_id) {
    printf("update_pair_status conn_status %d   conn_id %d\r\n",conn_status,conn_id);
    for (int i = 0; i < pair_device_count; i++) {
        if (memcmp(pair_devices[i].addr.addr, addr, SLE_ADDR_LEN) == 0) {
            pair_devices[i].state = conn_status;
	   pair_devices[i].conn_id = conn_id;
            return;
        }
    }

    if (pair_device_count < MAX_DEVICES) {
        memcpy(pair_devices[pair_device_count].addr.addr, addr, SLE_ADDR_LEN);
        pair_devices[pair_device_count].state = conn_status;
        pair_devices[pair_device_count].pair_code = 0;	
	pair_devices[pair_device_count].pair_state = 0;
	pair_devices[pair_device_count].conn_id = conn_id;
        pair_device_count++;
    }
}


static int get_conn_status(const unsigned char *addr) {
    for (int i = 0; i < pair_device_count; i++) {
        if (memcmp(pair_devices[i].addr.addr, addr, SLE_ADDR_LEN) == 0) {
            return pair_devices[i].state;
        }
    }
    return 0;
}

static uint8_t get_conn_id(const unsigned char *addr) {
    for (int i = 0; i < pair_device_count; i++) {
        if (memcmp(pair_devices[i].addr.addr, addr, SLE_ADDR_LEN) == 0) {
            return pair_devices[i].conn_id;
        }
    }
    return 0;
}

static int get_pair_status(const unsigned char *addr) {
    for (int i = 0; i < pair_device_count; i++) {
        if (memcmp(pair_devices[i].addr.addr, addr, SLE_ADDR_LEN) == 0) {
            return pair_devices[i].pair_state;
        }
    }
    return 0;
}

static int get_pair_code(const unsigned char *addr) {
    for (int i = 0; i < pair_device_count; i++) {
        if (memcmp(pair_devices[i].addr.addr, addr, SLE_ADDR_LEN) == 0) {
            return pair_devices[i].pair_code;
        }
    }
    return 0;
}

void load_paired_devices_from_file(void)
{
    FRESULT res;
    FIL     fp;
    char    line[128];          // 行缓冲区
    int     loaded = 0;          // 成功加载的设备计数

    // 以只读方式打开文件
    res = f_open(&fp, "0:/sle.txt", FA_READ);
    if (res == FR_NO_FILE) {
        // 文件不存在，无需加载
        printf("[INFO] No sle.txt found, starting with empty device list\n");
        return;
    } else if (res != FR_OK) {
        SLE_ERROR("Failed to open sle.txt for reading, error=%d\n", res);
        return;
    }

    // 逐行读取
    while (f_gets(line, sizeof(line), &fp)) {
        char    addr_str[20] = {0};   // 存放地址部分 "XX:XX:XX:XX:XX:XX"
        int     pair_code = 0;
        char    *p;

        // 去除行首空白
        for (p = line; isspace((unsigned char)*p); p++);

        // 跳过空行和注释行（可选）
        if (*p == '\0' || *p == '#')
            continue;

        // 解析格式 "addr=... , pair=..."
        if (sscanf(p, "addr=%19[^,],pair=%d", addr_str, &pair_code) == 2) {
            unsigned int bytes[6];
            // 解析地址部分 "XX:XX:XX:XX:XX:XX"
            if (sscanf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                       &bytes[0], &bytes[1], &bytes[2],
                       &bytes[3], &bytes[4], &bytes[5]) == 6) {
                if (loaded >= MAX_DEVICES) {
                    SLE_ERROR("Too many devices in file, stopping at MAX_DEVICES=%d\n", MAX_DEVICES);
                    break;
                }
                // 填充设备结构
                sle_connection_info_t *dev = &pair_devices[loaded];
                for (int i = 0; i < 6; i++) {
                    dev->addr.addr[i] = (unsigned char)bytes[i];
                }
                dev->pair_code   = pair_code;
                dev->pair_state = 1;          // 文件中均为已配对设备
                dev->state = 0;           // 初始连接状态为断开
                dev->conn_id     = (uint16_t)-1; // 0xFFFF，表示无效连接ID

                loaded++;
            } else {
                SLE_ERROR("Invalid address format in line: %s", line);
            }
        } else {
            SLE_INFO("Skipping malformed line: %s", line);
        }
    }

    // 关闭文件
    f_close(&fp);

    // 更新全局设备数量（假设 device_count 原本为 0，或我们需要覆盖）
    pair_device_count = loaded;
    printf("[INFO] Loaded %d paired devices from sle.txt\n", loaded);
}

void save_paired_devices_to_file(void)
{
    FRESULT res;
    FIL     fp;
    char    addr_str[20];      // 存放格式化后的地址 "XX:XX:XX:XX:XX:XX"
    char    line[128];          // 每行缓冲区，足够容纳 "addr=...pair=...\n"
    int     i;

    // 以写入方式打开（若存在则覆盖，不存在则创建）
    res = f_open(&fp, "0:/sle.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        SLE_ERROR("Failed to open sle.txt for writing, error=%d\n", res);
        return;
    }

    // 遍历所有设备
    for (i = 0; i < pair_device_count; i++) {
        if (pair_devices[i].pair_state == 1) {
            // 格式化 MAC 地址
            snprintf(addr_str, sizeof(addr_str),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     pair_devices[i].addr.addr[0], pair_devices[i].addr.addr[1],
                     pair_devices[i].addr.addr[2], pair_devices[i].addr.addr[3],
                     pair_devices[i].addr.addr[4], pair_devices[i].addr.addr[5]);

            // 组装一行数据
            int len = snprintf(line, sizeof(line),
                               "addr=%s,pair=%d\n",
                               addr_str, pair_devices[i].pair_code);

            // 写入文件
            if (len > 0 && len < (int)sizeof(line)) {
                UINT bytes_written;
                res = f_write(&fp, line, len, &bytes_written);
                if (res != FR_OK || bytes_written != len) {
                    SLE_ERROR("Write error at device %d, res=%d\n", i, res);
                    break;  // 发生错误，终止写入
                }
            } else {
                SLE_ERROR("Line buffer overflow for device %d\n", i);
            }
        }
    }

    // 关闭文件
    f_close(&fp);
    printf("[INFO] Saved %d paired devices to sle.txt\n", i);  // 实际写入的设备数
}


int save_sle_config(void) {   
    FRESULT res;
    FIL fp;
    
    /* 1. 格式化配置为文本 */
    char config_line[64];
    int line_len = snprintf(config_line, 64, "enabled=%d,mode=%d\n", 
                            sle_config.enabled, sle_config.mode);
    
    if (line_len <= 0 || line_len >= 64) {
        printf("[SAVE CONFIG] ERROR: Failed to format config line\n");
        return 0;
    }
    
    printf("[SAVE CONFIG] Formatted config: %s", config_line);
    
    /* 2. 直接打开文件写入（不检查文件系统状态）*/
    res = f_open(&fp, "0:/slecfg.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("[SAVE CONFIG] ERROR: File open failed: %d\n", res);
        return 0;
    }
    
    /* 3. 写入文本行 */
    UINT bytes_written;
    res = f_write(&fp, config_line, line_len, &bytes_written);
    f_close(&fp);
    
    if (res != FR_OK) {
        printf("[SAVE CONFIG] ERROR: Write failed: %d\n", res);
        return 0;
    }
    
    if (bytes_written != (UINT)line_len) {
        printf("[SAVE CONFIG] WARNING: Incomplete write: %u of %d bytes\n", 
               bytes_written, line_len);
    } else {
        printf("[SAVE CONFIG] ✅ SUCCESS: Config saved as text\n");
    }
    
    return 0;
}

/* 加载SLE配置 - 文本格式 */
int load_sle_config(SleConfig *msle_config) {
    FRESULT res;
    FIL fp;
    
    printf("\r\n[SLE CONFIG] Loading SLE config (text format)...\r\n");
    
    res = f_open(&fp, "0:/slecfg.txt", FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) {
        if (res == FR_NO_FILE) {
            printf("[SLE CONFIG] No config file, using defaults\n");
            msle_config->enabled = 1;
            msle_config->mode = 0;
            return 0;
        }
        printf("[SLE CONFIG] Failed to open config file: %d\n", res);
        return -1;
    }
    
    /* 读取文本行 */
    char config_line[64];
    if (f_gets(config_line, sizeof(config_line), &fp) == NULL) {
        f_close(&fp);
        printf("[SLE CONFIG] Failed to read config line\n");
        return -1;
    }
    f_close(&fp);
    
    printf("[SLE CONFIG] Config line: %s", config_line);
    
    /* 解析文本行 */
    int enabled, mode;
    if (sscanf(config_line, "enabled=%d,mode=%d", &enabled, &mode) == 2) {
        msle_config->enabled = (uint8_t)enabled;
        msle_config->mode = (uint8_t)mode;
        printf("[SLE CONFIG] Config loaded: enabled=%d, mode=%d\n",
               msle_config->enabled, msle_config->mode);
    } else {
        printf("[SLE CONFIG] Failed to parse config line\n");
        return -1;
    }
    
    return 0;
}

/* 解析从机信息响应并保存到文件 */
static void parse_and_save_slave_info(const char *response, uint16_t conn_id) {
	printf("parse_and_save_slave_info do nothing \r\n");
}

/* 数据接收回调函数 */
static void sle_data_received_callback_wrapper(uint16_t conn_id, const uint8_t *data, uint32_t len) {
    if (!data || len == 0) {
        SLE_SVC_ERROR("sle_data_received_callback_wrapper: invalid data\n");
        return;
    }
    
    SLE_SVC_INFO("sle_data_received_callback_wrapper: conn_id=%d, data_len=%d\n", conn_id, len);
         if(len < sizeof(sleMultiProfileDataFormat))
         {
		printf("msg too small \r\n");
		return;
	}
	sleMultiProfileDataFormat *mpdata = (sleMultiProfileDataFormat*)data;
        if(mpdata->profile_id == SLE_PROFILE_INDEX_SDP){
        	     sdp_data_process(conn_id,data,len);
	}else {
        sle_data_format *event_data = (sle_data_format *)goldie_malloc(len + sizeof(sle_data_format_head));
        if (!event_data) {
            SLE_INFO("sle_data_received_callback_wrapper: failed to allocate memory for event data\n");
            return;
        }
        event_data->conn_id = conn_id;
        event_data->data_len = len;
        memcpy((char*)(&(event_data->buffer)), data, len);
        /* 将数据接收事件放入队列 */
        enqueue_event(SLE_EVENT_RECEIVE_DATA, event_data);
        
        /* 注意：事件处理函数需要负责释放event_data内存 */
    }
}

/* 处理延迟发送 */
static void send_pair_msg(int pair_code,uint8_t conn_id) {
    if (pair_code < 100000 || pair_code > 999999) {
        SLE_ERROR("[SLE] Invalid pair code: %d, using default 111111\n", pair_code);
        pair_code = 111111;
    }
    int msg_len = sizeof(sleMultiProfileDataFormat_head) + sizeof(sdp_data_format_head) +  sizeof(sdp_pair_msg);
    sleMultiProfileDataFormat * mpdata= (sleMultiProfileDataFormat*)goldie_malloc(msg_len);
    sdp_data_format *sdp_msg  = (sdp_data_format *)mpdata->buffer;
    sdp_pair_msg * pair_msg = (sdp_pair_msg *)sdp_msg->buffer;
    mpdata->data_len = sizeof(sdp_data_format_head) +  sizeof(sdp_pair_msg);
    mpdata->profile_id = SLE_PROFILE_INDEX_SDP;
    sdp_msg->data_head = SDP_DATA_HEAD;
    sdp_msg->magic_num0 = SDP_MAGIC_NUM0;
    sdp_msg->magic_num1 = SDP_MAGIC_NUM1;
    sdp_msg->msg_len = sizeof(sdp_pair_msg);
    sdp_msg->msg_type=SDP_MSG_ID_PAIR;
    pair_msg->pair_code = pair_code;
    
    /* 发送配对码给连接方 */
    int ret = sle_drv->send_data(conn_id, (const uint8_t*)mpdata, msg_len);
    printf("sle_drv->send_data ret %d \r\n",ret);
}

/* 连接状态回调函数 */
static void sle_connection_state_callback_wrapper(uint16_t conn_id, const sle_addr_t *addr,
    uint8_t conn_state, uint8_t pair_state, uint8_t disc_reason)
{
    SLE_SVC_DEBUG("sle_connection_state_callback_wrapper: conn_id=%d, conn_state=%d\n", conn_id, conn_state);
    
    if (addr) {
        SLE_SVC_DEBUG("Remote addr: ");
        for (int i = 0; i < SLE_ADDR_LEN; i++) {
            SLE_SVC_DEBUG("%02X:", addr->addr[i]);
        }
        SLE_SVC_DEBUG("\n");
    }
    if(conn_state == SLE_ACB_STATE_CONNECTED){
     	update_conn_status(addr->addr,1,conn_id);
     }else{
     	update_conn_status(addr->addr,0,-1);
    }
}



/* 事件队列操作 */
static void enqueue_event(SleEventType type, void* data) {
    goldie_mutex_lock(&event_queue_mutex);
    
    int next_tail = (event_queue_tail + 1) % SLE_EVENT_QUEUE_SIZE;
    if (next_tail != event_queue_head) {
        event_queue[event_queue_tail].type = type;
        event_queue[event_queue_tail].data = data;
        event_queue_tail = next_tail;
    } else {
        SLE_SVC_ERROR("SLE event queue full!\n");
    }
    
    goldie_mutex_unlock(&event_queue_mutex);
}

static int dequeue_event(SleEvent* event) {
    goldie_mutex_lock(&event_queue_mutex);
    
    int ret = 0;
    if (event_queue_head != event_queue_tail) {
        *event = event_queue[event_queue_head];
        event_queue_head = (event_queue_head + 1) % SLE_EVENT_QUEUE_SIZE;
        ret = 1;
    }
    
    goldie_mutex_unlock(&event_queue_mutex);
    return ret;
}

/* 设备发现回调函数 */
static void sle_device_discovered_callback_wrapper(const sle_device_info_t *device) {
    if (!device) return;
    SLE_SVC_DEBUG("sle_device_discovered_callback_wrapper managed_device_count %d\n",managed_device_count);
    
    /* 检查设备是否已存在（地址去重） */
    int device_exists = 0;
    for (uint32_t i = 0; i < managed_device_count; i++) {
        if (memcmp(&managed_devices[i].addr, &device->addr, sizeof(sle_addr_t)) == 0) {
            /* 设备已存在，更新信息 */
            managed_devices[i].rssi = device->rssi;
            device_exists = 1;
            break;
        }
    }
    
    /* 如果设备不存在且未达到上限，添加新设备 */
    if (!device_exists && managed_device_count < MAX_MANAGED_DEVICES) {
        managed_devices[managed_device_count] = *device;
        managed_device_count++;
        device_list_updated = 1; /* 设置设备列表更新标记 */
        SLE_SVC_INFO("SLE: New device discovered, total devices: %d\n", managed_device_count);
    }
}

/* 清空扫描结果 */
static int svr_clear_scan_results(void) {
    managed_device_count = 0;
    memset(managed_devices, 0, sizeof(managed_devices));
    
    SLE_SVC_INFO("svr_clear_scan_results: all devices cleared\n");
    return 0;
}

/* 注册回调函数 */
int sle_service_register_callback(SleEventCallback cb,int profile_id) {
    if((profile_id <0)||(profile_id >= SLE_PROFILE_INDEX_MAX ))
    {
	return -1;
     }
    goldie_mutex_lock(&event_cb_mutex);
    g_profile_mask = g_profile_mask |(0x1<<profile_id);
    sle_event_callback[profile_id] = cb;
    goldie_mutex_unlock(&event_cb_mutex);
    return 0;
}

/* 注册数据接收回调函数 */
int sle_service_register_data_receive_callback(SleDataReceiveCallback cb,int profile_id) {
    if((profile_id <0)||(profile_id >= SLE_PROFILE_INDEX_MAX ))
    {
	return -1;
     }
    goldie_mutex_lock(&data_cb_mutex);
    sle_service_data_receive_callback[profile_id] = cb;
    goldie_mutex_unlock(&data_cb_mutex);
    return 0;
}

/* 触发事件 */
static int sle_service_trigger_event(SleEventType type, void* data) {
    enqueue_event(type, data);
    return 0;
}

/* 使能星闪 */
static int svr_enable(void) {
    if (!sle_drv) 
		return -1;
	
    if(sle_config.enabled)
    {
	return 0;
    }
    svr_clear_scan_results();
    sle_drv->set_mode(sle_config.mode);
    #ifndef SLE_SWITCH_DISABLE
    if(sle_drv->enable()){
		 sle_config.enabled = 0;
		 printf("enable sle error \r\n");
		 return -1;
	}else{
		 sle_config.enabled = 1;
	}
    #else
		 sle_config.enabled = 1;
    #endif

     /* 设置默认设备名称 */
    sle_drv->set_local_name(DEFAULT_SLE_NAME);
    /*设置地址*/
    sle_addr_t addr;
    addr.type = 0;
    memcpy(addr.addr,local_addr,6);
    printf("sle addr is:%2x-%2x-%2x-%2x-%2x-%2x \r\n",
		addr.addr[0],addr.addr[1],addr.addr[2],addr.addr[3],addr.addr[4],addr.addr[5]);
    sle_drv->set_local_addr(&addr);
	
    /* 根据配置设置初始状态 */
    if (sle_config.mode == 0) {
	/* 注册设备发现回调 */
	sle_drv->register_device_discovered_callback(sle_device_discovered_callback_wrapper);
	sle_drv->stop_announce();
	goldie_msleep(200);
	sle_drv->start_scan();
    }else{
	sle_drv->stop_scan();
	goldie_msleep(200);
	sle_drv->start_announce();	
    }
    
    /* 注册数据接收回调 */
    sle_drv->register_data_receive_callback(sle_data_received_callback_wrapper);
    
    /* 注册连接状态回调 */
    sle_drv->register_connection_state_callback(sle_connection_state_callback_wrapper);	
    return 0;
}

/* 禁用星闪 */
static int svr_disable(void) {
    if (!sle_drv) 
		return -1;

   
   svr_clear_scan_results();
    if(!sle_config.enabled)
    {
	return 0;
    }

    sle_drv->stop_scan();
    sle_drv->stop_announce();
    #ifndef SLE_SWITCH_DISABLE
    sle_drv->disable();
    #endif
    sle_config.enabled = 0;
    return 0;
}

/* 是否已使能 */
static bool svr_is_enabled(void) {
	return sle_config.enabled?true:false;
}

/* 设置本地设备名称 */
static int svr_set_local_name(const char *name) {
    if (!sle_drv) return -1;
    return sle_drv->set_local_name(name);
}

/* 获取本地设备名称 */
static int svr_get_local_name(sle_addr_t *addr) {
    if (!sle_drv) return -1;
    return sle_drv->get_local_addr(addr);
}


/* 开始设备扫描 */
static int svr_start_scan(void) {
    SLE_SVC_DEBUG("svr_start_scan: called\n");
    if (!sle_drv) {
        SLE_SVC_ERROR("svr_start_scan: sle_drv is NULL\n");
        return -1;
    }
    
    /* 清空之前的扫描结果 */
    svr_clear_scan_results();
        
    int ret = sle_drv->start_scan();
    SLE_SVC_DEBUG("svr_start_scan: returned %d\n", ret);
    return ret;
}

/* 停止设备扫描 */
static int svr_stop_scan(void) {
#ifdef KEEP_ACTIVE_FOREVER
	return 0;
#else
    if (!sle_drv) return -1;
    return sle_drv->stop_scan();
#endif	
}

/* 是否正在扫描 */
static int svr_is_scanning(void) {
    //SLE_SVC_DEBUG("svr_is_scanning: called\n");
    if (!sle_drv) {
        SLE_SVC_ERROR("svr_is_scanning: sle_drv is NULL\n");
        return 0;
    }

   return sle_drv->is_scanning();
}

#if 0
/* 获取扫描结果 */
static int svr_get_scan_results(sle_device_info_t *devices, uint32_t *count) {
    if (!devices || !count) return -1;
    
    uint32_t copy_count = (*count < managed_device_count) ? *count : managed_device_count;
    
    for (uint32_t i = 0; i < copy_count; i++) {
        devices[i] = managed_devices[i];
    }
    
    *count = copy_count;
    
    SLE_SVC_DEBUG("svr_get_scan_results: returning %d devices\n", copy_count);
    return 0;
}
#else
static int svr_get_paired_devices(sle_connection_info_t *out_devices, uint32_t *count)
{
    if (!out_devices || !count) {
		return -1;
     }
    uint32_t max_out = *count;
    uint32_t out_index = 0;

	 for (int i = 0; i < pair_device_count && out_index < max_out; i++) {
		 if (pair_devices[i].pair_state == 1) {
			 // 填充基本信息（地址 type=0，名称、RSSI 等置空）
			 memcpy(&out_devices[out_index].addr, &pair_devices[i].addr, sizeof(sle_addr_t));
			out_devices[out_index].pair_code = pair_devices[i].pair_code;
			out_devices[out_index].pair_state = pair_devices[i].pair_state;
			out_devices[out_index].state = pair_devices[i].state;
			out_devices[out_index].conn_id = pair_devices[i].conn_id;
			out_index++;
		 }
	 }
	
	 *count = out_index;
	 SLE_INFO("svr_get_paired_devices: returning %d devices \n",out_index);
	 return 0;

}
static int svr_get_scan_results(sle_device_info_t *out_devices, uint32_t *count) {
    if (!out_devices || !count) return -1;
    uint32_t max_out = *count;
    uint32_t out_index = 0;

	 uint32_t copy_managed = (managed_device_count < max_out) ? managed_device_count : max_out;
	 for (uint32_t i = 0; i < copy_managed; i++) {
		 out_devices[out_index++] = managed_devices[i];
	 }
    *count = out_index;
    SLE_INFO("svr_get_scan_results: returning %d devices \n", out_index);
  return 0;
}

static int svr_get_paired_and_discovered_devices(sle_device_info_t *out_devices, uint32_t *count){
    if (!out_devices || !count) return -1;

    uint32_t max_out = *count;
    uint32_t out_index = 0;

    // 1. 先返回扫描到的设备（managed_devices），复制尽可能多的设备
    uint32_t copy_managed = (managed_device_count < max_out) ? managed_device_count : max_out;
    for (uint32_t i = 0; i < copy_managed; i++) {
        out_devices[out_index++] = managed_devices[i];
    }

    // 2. 再返回配对成功的设备（devices 中 pair_status == 1），但跳过已存在的地址
    for (int i = 0; i < pair_device_count && out_index < max_out; i++) {
        if (pair_devices[i].pair_state == 1) {
            // 检查地址是否已在输出中（包括已添加的扫描设备和之前的配对设备）
            int duplicate = 0;
            for (uint32_t j = 0; j < out_index; j++) {
                if (memcmp(out_devices[j].addr.addr, pair_devices[i].addr.addr, SLE_ADDR_LEN) == 0) {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate) {
                // 填充基本信息（地址 type=0，名称、RSSI 等置空）
                sle_device_info_t *info = &out_devices[out_index];
                memcpy(&(info->addr), &(pair_devices[i].addr), sizeof(sle_addr_t));
                info->name[0] = '\0';
                info->rssi = 0;
                info->data_len = 0;
                info->data = NULL;
                out_index++;
            }
        }
    }

    *count = out_index;
    SLE_INFO("svr_get_paired_and_discovered_devices: returning %d devices (%d scanned, %d paired)\n",
                  out_index, copy_managed, out_index - copy_managed);
    return 0;
}
#endif

/* 连接设备（主动连接） */
static int svr_connect(const sle_addr_t *addr) {
    if (!sle_drv || !addr) return -1;
        
    /* 发起新连接 - 注意：connect函数现在需要conn_id参数 */
    uint16_t conn_id = 0;
    int ret = sle_drv->connect(addr, &conn_id);
    return ret;
}

/* 断开连接 */
static int svr_disconnect(uint16_t conn_id) {
    if (!sle_drv) return -1;
    
    /* 先通过驱动断开连接 */
    int ret = sle_drv->disconnect(conn_id);
    return ret;
}

/* 获取连接列表 */
static int svr_get_connections(sle_connection_info_t *conns, uint32_t *count) {
    if (!sle_drv || !conns || !count) return -1;
    
    // 直接调用驱动层的get_connections函数，获取所有连接
    return sle_drv->get_connections(conns, count);
}

/* 发送数据 */
static int svr_send_data(uint16_t conn_id, const uint8_t *data, uint32_t len) {
    if (!sle_drv) return -1;
    return sle_drv->send_data(conn_id, data, len);
}

/* 启动设备公开 */
static int svr_start_announce(void) {
    if (!sle_drv) return -1;
    return sle_drv->start_announce();
}

/* 停止设备公开 */
static int svr_stop_announce(void) {
#ifdef KEEP_ACTIVE_FOREVER
		return 0;
#else
    if (!sle_drv) return -1;
    return sle_drv->stop_announce();
#endif	
}

/* 是否正在设备公开 */
static int svr_is_announcing(void) {
    if (!sle_drv) return 0;
    return sle_drv->is_announcing();
}

/* 配对设备 */
static int svr_pair(sle_pair_data* pair_data){
    sle_addr_t *addr = &(pair_data->addr);
    int pair_code = pair_data->pair_code;
    uint16_t conn_id = 0;
    int piar_timeout = 0;

    if (!sle_drv) return -1;
    if((sle_config.mode !=0)||(sle_config.enabled !=1))
    {
         printf("sle pair failed mode %d  enabled %d \r\n",sle_config.mode,sle_config.enabled);
	return -1;
    }

  // 验证配对码格式
  if (pair_code >= 100000 && pair_code <= 999999) {
		update_pair_status(addr->addr,0);
		update_pair_code(addr->addr,pair_code);
		int conn_status = get_conn_status(addr->addr);
		if(!conn_status){
			printf("start sle connecting \r\n");
    			sle_drv->connect(addr, &conn_id);
		}
		printf("con status %d ,wait for connected \r\n",conn_status);
		while(!conn_status){
			conn_status = get_conn_status(addr->addr);
			goldie_msleep(100);
			piar_timeout++;
			if(piar_timeout>(PAIR_TIMEOUT/100))
				break;
		}

		if(!conn_status)
		{
			printf("piar connect failed \r\n");
			return -1;
		}
		conn_status = get_conn_status(addr->addr);
		uint8_t connid= get_conn_id(addr->addr);
		printf("con status %d , connid %d wait for piared \r\n",conn_status,connid);
		goldie_msleep(1000);

		send_pair_msg(pair_code,connid);
		piar_timeout = 0;
		int pair_status = get_pair_status(addr->addr);
		while(!pair_status){
			pair_status = get_pair_status(addr->addr);
			goldie_msleep(100);
			piar_timeout++;
			if(piar_timeout>(PAIR_TIMEOUT/100))
				break;
		}
		
		sle_drv->disconnect(connid);
		conn_status = get_conn_status(addr->addr);
		connid= get_conn_id(addr->addr);
		printf("con status %d , connid %d wait for disconnect \r\n",conn_status,connid);
		while(conn_status){
			conn_status = get_conn_status(addr->addr);
			goldie_msleep(100);
			piar_timeout++;
			if(piar_timeout>(PAIR_TIMEOUT/100))
				break;
		}
		if(pair_status){			
			printf("paired successed \r\n");
			return 0;
		}else{
			printf("pair failed \r\n");
			return -1;
		}
   }else{
	printf("error pair_code \r\n");
	return -1;
   }
  
}


/* 取消配对 */
static int svr_unpair(sle_pair_data* pair_data) {
    update_pair_status(pair_data->addr.addr,0);
    return 0;
}

/* 获取配对状态 */
static int svr_get_pair_state(const sle_addr_t *addr) {
    if (!sle_drv) return -1;
    return get_pair_status(addr->addr);
}

/* 设置发射功率 */
static int svr_set_tx_power(int8_t power) {
    if (!sle_drv) return -1;
    return sle_drv->set_tx_power(power);
}

/* 获取信号强度 */
static int svr_get_rssi(uint16_t conn_id, int8_t *rssi) {
    if (!sle_drv) return -1;
    return sle_drv->get_rssi(conn_id, rssi);
}

/* 获取SLE模式 - 服务层实现 */
static uint8_t svr_get_mode(void)
{
    return sle_config.mode;
}

/* 设置SLE模式 - 服务层实现 */
static void svr_set_mode(uint8_t mode)
{
    sle_config.mode = mode;
    return;
}

/* 检查指定设备是否已连接 */
static int svr_is_connected(const sle_addr_t *addr) {
    if (!addr || !sle_drv) return 0;
    
    SLE_INFO("[SLE Service] svr_is_connected checking addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
           addr->addr[0], addr->addr[1], addr->addr[2],
           addr->addr[3], addr->addr[4], addr->addr[5]);
    
    /* 首先检查驱动中的实际连接状态 - 这是最权威的连接状态 */
    sle_connection_info_t conns[8];
    uint32_t count = 8;
    
    if (sle_drv->get_connections(conns, &count) == 0) { 
        /* 检查驱动中的连接列表 */
        for (uint32_t i = 0; i < count; i++) {
            
            if (memcmp(&conns[i].addr, addr, sizeof(sle_addr_t)) == 0) {
                SLE_INFO("[SLE Service] Found matching addr in driver connections, returning 1\n");
                return 1;
            }
        }
        
        /* 如果地址不在驱动连接列表中，直接返回0 */
        SLE_INFO("[SLE Service] Address not found in driver connections, returning 0\n");
        return 0;
    } else {
	    /* 检查跟踪的连接列表 */
	    for (uint32_t i = 0; i < tracked_connection_count; i++) {       
	        if (memcmp(&tracked_connections[i].addr, addr, sizeof(sle_addr_t)) == 0) {
	            SLE_INFO("[SLE Service] Found matching addr in tracked connections, returning 1\n");
	            return 1;
	        }
	    }
    }
    SLE_INFO("[SLE Service] Address not found in any connection list, returning 0\n");
    return 0;
}

static void send_dev_info_msg(uint16_t conn_id,int flag){
	  int msg_len = sizeof(sleMultiProfileDataFormat_head) + sizeof(sdp_data_format_head) +  sizeof(sdp_devinfo_msg);
	 sleMultiProfileDataFormat * mpdata= (sleMultiProfileDataFormat*)goldie_malloc(msg_len);
	 sdp_data_format *sdp_msg  = (sdp_data_format *)mpdata->buffer;
	 sdp_devinfo_msg * dev_info_msg = (sdp_devinfo_msg *)sdp_msg->buffer;
	 mpdata->data_len = sizeof(sdp_data_format_head) +	sizeof(sdp_pair_rsp_msg);
	 mpdata->profile_id = SLE_PROFILE_INDEX_SDP;
	 sdp_msg->data_head = SDP_DATA_HEAD;
	 sdp_msg->magic_num0 = SDP_MAGIC_NUM0;
	 sdp_msg->magic_num1 = SDP_MAGIC_NUM1;
	 sdp_msg->msg_type= (flag?SDP_MSG_ID_GET_INFO_RSP:SDP_MSG_ID_GET_INFO);
	 sdp_msg->msg_len = sizeof(sdp_pair_rsp_msg);
	 dev_info_msg->profile_mask = g_profile_mask;
	 int slave_pair_code = svr_s_generate_pair_code(); 
	 snprintf(dev_info_msg->name, 24, "%s%d",DEFAULT_SLE_NAME,slave_pair_code);
	 dev_info_msg->name[24] = 0;
	sle_drv->send_data(conn_id, (const uint8_t*)mpdata, msg_len);
}

static void send_pair_rsp(uint16_t conn_id,int rsp_value){
     int msg_len = sizeof(sleMultiProfileDataFormat_head) + sizeof(sdp_data_format_head) +  sizeof(sdp_pair_rsp_msg);
    sleMultiProfileDataFormat * mpdata= (sleMultiProfileDataFormat*)goldie_malloc(msg_len);
    sdp_data_format *sdp_msg  = (sdp_data_format *)mpdata->buffer;
    sdp_pair_rsp_msg * pair_rsp = (sdp_pair_rsp_msg *)sdp_msg->buffer;
    mpdata->data_len = sizeof(sdp_data_format_head) +  sizeof(sdp_pair_rsp_msg);
    mpdata->profile_id = SLE_PROFILE_INDEX_SDP;
    sdp_msg->data_head = SDP_DATA_HEAD;
    sdp_msg->magic_num0 = SDP_MAGIC_NUM0;
    sdp_msg->magic_num1 = SDP_MAGIC_NUM1;
    sdp_msg->msg_type=SDP_MSG_ID_PAIR_RSP;
    sdp_msg->msg_len = sizeof(sdp_pair_rsp_msg);
    pair_rsp->pair_result = rsp_value;
    /* 发送配对码给连接方 */
   sle_drv->send_data(conn_id, (const uint8_t*)mpdata, msg_len);
}
static void sdp_data_process(uint16_t conn_id, const uint8_t *data, uint32_t data_len){
	sleMultiProfileDataFormat* mpdata = (sleMultiProfileDataFormat*)data;
	if(data_len < sizeof(sleMultiProfileDataFormat_head)+sizeof(sdp_data_format))
	{
		printf("sdp_data_process msg too small \r\n");
		return;
    }
	if(mpdata->profile_id != SLE_PROFILE_INDEX_SDP){
		printf("sdp_data_process profile_id error \r\n",mpdata->profile_id);
		return;
	}
	
	sdp_data_format* sdp_data = (sdp_data_format*)mpdata->buffer;
	if((sdp_data->data_head != SDP_DATA_HEAD)||
		(sdp_data->magic_num0 != SDP_MAGIC_NUM0)||
		(sdp_data->magic_num1 != SDP_MAGIC_NUM1)){
		
		printf("sdp_data_process data head error:%04x %04x %04x \r\n",sdp_data->data_head,
			sdp_data->magic_num0,sdp_data->magic_num1);
		return;
	}
	sdp_pair_msg* pair_msg  = (sdp_pair_msg*)sdp_data->buffer;
	int slave_pair_code = svr_s_generate_pair_code(); 
	sdp_pair_rsp_msg* pair_rsp_msg  = (sdp_pair_rsp_msg*)sdp_data->buffer;
	
	switch(sdp_data->msg_type){
		case SDP_MSG_ID_PAIR:
			 SLE_INFO("Received pair code: %d, Slave pair code: %d\n", pair_msg->pair_code, slave_pair_code);
			if (pair_msg->pair_code == slave_pair_code) {
				printf("Saved verified pair code: %d\n", pair_msg->pair_code);
				send_pair_rsp(conn_id,1);
				uint32_t conn_count = 1;
				if (sle_drv->get_connections(&sle_info, &conn_count) == 0 && conn_count > 0) {
					/* 查找匹配的conn_id */
					if (sle_info.conn_id == conn_id) {
					SLE_INFO("Found device with conn_id=%d, addr=%02X:%02X:%02X:%02X:%02X:%02X\n",
					conn_id,
					sle_info.addr.addr[0], sle_info.addr.addr[1],
					sle_info.addr.addr[2], sle_info.addr.addr[3],
					sle_info.addr.addr[4], sle_info.addr.addr[5]);
					update_pair_status(sle_info.addr.addr,1);
					sle_service_trigger_event(SLE_EVENT_SLAVE_PAIRED,NULL);	
					}
				}
			}else{
				update_pair_status(sle_info.addr.addr,0);
				send_pair_rsp(conn_id,0);
				sle_service_trigger_event(SLE_EVENT_SLAVE_PAIRED,NULL);	
			}
			break;
		case SDP_MSG_ID_PAIR_RSP:
			printf("sdp_pair_rsp_msg %d \r\n",pair_rsp_msg->pair_result);
			if(pair_rsp_msg->pair_result){
			 	send_dev_info_msg(conn_id,0);
				update_pair_status_with_connid(conn_id,1);
			}else{
				update_pair_status_with_connid(conn_id,0);
			}
			break;
		case SDP_MSG_ID_GET_INFO:
			send_dev_info_msg(conn_id,1);
		case SDP_MSG_ID_GET_INFO_RSP:			
			/*save devinfo*/
			sle_service_trigger_event(SLE_EVENT_SLAVE_PAIRED,NULL);
			break;
		default:
			printf("unkown sdp msg type %d \r\n",sdp_data->msg_type);
			break;
	}
}

/**
 * @brief 基于星闪MAC地址生成固定6位配对码
 * @param mac_addr 输入的6字节星闪MAC地址数组（如{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}）
 * @return 6位整数配对码（范围：100000 ~ 999999）
 */
static int svr_s_generate_pair_code()
{
    int hash = 5381; // 经典哈希初始值，保证散列均匀性

    // 遍历6字节MAC地址，计算哈希值（DJB2哈希算法，保持原逻辑）
    for (int i = 0; i < 6; i++) {
        hash = ((hash << 5) + hash) + local_addr[i]; // hash = hash*33 + mac_addr[i]
    }

    // 转换为无符号数避免负数取模异常，计算6位配对码
    unsigned int unsigned_hash = (unsigned int)hash;
    unsigned int mod_result = unsigned_hash % 900000; // 6位：999999-100000=900000
    int pair_code = (int)mod_result + 100000; // 偏移100000，得到100000~999999范围

    return pair_code;
}

static void process_event_cb(int eventid,void* data){
	int i  =0;
	for(i =0;i<SLE_PROFILE_INDEX_MAX;i++)
	{
		goldie_mutex_lock(&event_cb_mutex);
		if(sle_event_callback[i])
		{
			sle_event_callback[i](eventid,data);
		}
		goldie_mutex_unlock(&event_cb_mutex);
	}
}

static void process_data_cb(sle_data_format* data){
	int i  =0;
	sleMultiProfileDataFormat *mpdata = &(data->buffer);
	int profile_id = mpdata->profile_id;
	printf("process_data_cb profile_id %d \r\n",profile_id);
	if((profile_id <0)||(profile_id >SLE_PROFILE_INDEX_MAX))
	{
		return;
	}

	goldie_mutex_lock(&data_cb_mutex);
	if(sle_service_data_receive_callback[profile_id])
	{
		sle_service_data_receive_callback[profile_id](data->conn_id, mpdata, data->data_len);
	}		
	goldie_mutex_lock(&data_cb_mutex);
}


/* 处理事件 */
static void process_events(void) {
    SleEvent event;
    static int last_status = 0;
    uint16_t* conn_id;
    while (dequeue_event(&event)) {
        switch (event.type) {
            case SLE_EVENT_ENABLE:
	       svr_enable();
#ifdef KEEP_ALWAYS_SCAN_OR_ANNOUNCE
	       always_scan_flag = 0;
	       always_announce_flag =0;
#endif           
                break;
                
            case SLE_EVENT_DISABLE:
		svr_disable();
#ifdef KEEP_ALWAYS_SCAN_OR_ANNOUNCE
		always_scan_flag = 0;
		always_announce_flag = 0;
#endif        
                break;
                
            case SLE_EVENT_START_SCAN:
                if (sle_drv) {
                    sle_drv->start_scan();
		   
#ifdef KEEP_ALWAYS_SCAN_OR_ANNOUNCE
		   always_scan_flag = 1;
		  kick_scan_and_announce_timer();
#endif		  
                }
                break;
            case SLE_EVENT_STOP_SCAN:
                if (sle_drv) {
                    sle_drv->stop_scan();
#ifdef KEEP_ALWAYS_SCAN_OR_ANNOUNCE					
		   always_scan_flag = 0;
#endif
                }
                break;
            case SLE_EVENT_CONNECT:
                if (sle_drv && event.data) {
		  printf("SLE_EVENT_CONNECT \r\n");
                    svr_connect((sle_addr_t*)event.data);
                }
                break;
                
            case SLE_EVENT_DISCONNECT:
                if (sle_drv && event.data) {
                    uint16_t* conn_id = (uint16_t*)event.data;
                    svr_disconnect(*conn_id);
                }
                break;
                
            case SLE_EVENT_SEND_DATA:
                /* 数据发送需要更复杂的处理，这里简化 */
                break;
                
            case SLE_EVENT_START_ANNOUNCE:
                if (sle_drv) {
                    sle_drv->start_announce();
#ifdef KEEP_ALWAYS_SCAN_OR_ANNOUNCE					
		  kick_scan_and_announce_timer();
		 always_announce_flag = 1; 
#endif		 
                }
                break;

            case SLE_EVENT_STOP_ANNOUNCE:
                if (sle_drv) {
                    sle_drv->stop_announce();
#ifdef KEEP_ALWAYS_SCAN_OR_ANNOUNCE					
		  always_announce_flag = 0;
#endif
                }
                break;
                
            case SLE_EVENT_PAIR:
                if (sle_drv && event.data) {
                    svr_pair((sle_pair_data*)event.data);
                }

                	process_event_cb(SLE_STATUS_PAIRED_STATUS_UPDATED, NULL);
	        save_paired_devices_to_file();
                break;

	    case SLE_EVENT_SLAVE_PAIRED:
                	process_event_cb(SLE_STATUS_PAIRED_STATUS_UPDATED, NULL);
	        save_paired_devices_to_file();
		break;
                
            case SLE_EVENT_UNPAIR:
                if (sle_drv && event.data) {
                   svr_unpair((sle_pair_data*)event.data);
                }
		process_event_cb(SLE_STATUS_PAIRED_STATUS_UPDATED, NULL);
		 save_paired_devices_to_file();
                break;
                
            case SLE_EVENT_RECEIVE_DATA:
                /* 处理接收到的数据（现在只处理非配对码命令的数据，如音频数据） */
                if (event.data) {
                    process_data_cb(event.data);
                    /* 释放事件数据内存 */
                    goldie_free(event.data);
                }
                break;
                
            default:
                break;
        }
    }
}     

/* 检查状态变化 */
static void check_sle_status(void) {
    if (!sle_drv) return;
    
    /* 检查基本状态 */
    bool current_enabled = svr_is_enabled();
    int current_scanning = svr_is_scanning();
    int current_announcing = svr_is_announcing();
    SLE_SVC_DEBUG("current_enabled %d current_scanning %d current_announcing %d\n",
		current_enabled,current_scanning,current_announcing);
    
    /* 检查连接状态变化 */
    sle_connection_info_t current_conns[MAX_TRACKED_CONNECTIONS];
    uint32_t current_count = MAX_TRACKED_CONNECTIONS;
    
    if (sle_drv->get_connections(current_conns, &current_count) == 0) {
        /* 检查连接列表是否有变化 */
        int connection_changed = 0;
        
        /* 检查连接数量变化 */
        if (current_count != tracked_connection_count) {
            connection_changed = 1;
            SLE_SVC_INFO("SLE: Connection count changed from %d to %d\n", 
                   tracked_connection_count, current_count);
        } else {
            /* 检查连接地址变化 */
            for (uint32_t i = 0; i < current_count; i++) {
                int found = 0;
                for (uint32_t j = 0; j < tracked_connection_count; j++) {
                    if (memcmp(&current_conns[i].addr, &tracked_connections[j].addr, sizeof(sle_addr_t)) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    connection_changed = 1;
                    SLE_SVC_INFO("SLE: New connection detected\n");
                    break;
                }
            }
        }
        
        /* 如果连接有变化，更新跟踪的连接列表并通知应用程序 */
        if (connection_changed) {
            /* 更新跟踪的连接列表 */
            tracked_connection_count = (current_count < MAX_TRACKED_CONNECTIONS) ? current_count : MAX_TRACKED_CONNECTIONS;
            for (uint32_t i = 0; i < tracked_connection_count; i++) {
                tracked_connections[i] = current_conns[i];
            }
            
            /* 通知应用程序连接状态已变化 */
            process_event_cb(SLE_STATUS_CONNECTION_LIST_UPDATED, NULL);
            SLE_SVC_INFO("SLE: Connection list updated notification sent\n");
        }
    }
    
    goldie_mutex_lock(&event_queue_mutex);
    
    if (last_enabled_status != current_enabled) {
         process_event_cb(current_enabled ? SLE_STATUS_ENABLED : SLE_STATUS_DISABLED, NULL);
	save_sle_config();
        last_enabled_status = current_enabled;
    }
    
    if (last_scanning_status != current_scanning) {
         process_event_cb(current_scanning ? SLE_STATUS_SCANNING : SLE_STATUS_SCAN_DONE, NULL);
        last_scanning_status = current_scanning;
    }
    
    if (last_announcing_status != current_announcing) {
        process_event_cb(current_announcing ? SLE_STATUS_ANNOUNCING : SLE_STATUS_ANNOUNCE_STOPPED, NULL);
        last_announcing_status = current_announcing;
    }
    
    /* 检查设备列表更新标记 */
    if (device_list_updated) {
         process_event_cb(SLE_STATUS_DEVICE_LIST_UPDATED, NULL);
        device_list_updated = 0; /* 清除标记 */
        SLE_SVC_INFO("SLE: Device list updated notification sent\n");
    }    
    goldie_mutex_unlock(&event_queue_mutex);

#ifdef KEEP_ALWAYS_SCAN_OR_ANNOUNCE
    if(current_scanning ||current_announcing ){
    	check_scan_and_announce_timeout();
	if(scan_timeout){
		if(current_announcing){			
			printf("restart announce \r\n");
			sle_drv->stop_announce();
			goldie_msleep(100);
			sle_drv->start_announce();
			kick_scan_and_announce_timer();
		}else{
			printf("restart scan \r\n");
			sle_drv->stop_scan();
			goldie_msleep(100);
			sle_drv->start_scan();
			kick_scan_and_announce_timer();
		}
	}
    }
#endif

}

/* 服务操作结构体 */
static SleSdpService sleservice = {
    .svr_enable = svr_enable,
    .svr_disable = svr_disable,
    .svr_is_enabled = svr_is_enabled,
    .svr_set_local_name = svr_set_local_name,
    .svr_get_local_name = svr_get_local_name,
    .svr_start_scan = svr_start_scan,
    .svr_stop_scan = svr_stop_scan,
    .svr_is_scanning = svr_is_scanning,
    .svr_get_scan_results = svr_get_scan_results,
    .svr_connect = svr_connect,
    .svr_disconnect = svr_disconnect,
    .svr_get_connections = svr_get_connections,
    .svr_send_data = svr_send_data,
    .svr_start_announce = svr_start_announce,
    .svr_stop_announce = svr_stop_announce,
    .svr_is_announcing = svr_is_announcing,
    .svr_get_pair_state = svr_get_pair_state,
    .svr_set_tx_power = svr_set_tx_power,
    .svr_get_rssi = svr_get_rssi,
    .register_callback = sle_service_register_callback,
    .register_data_receive_callback = sle_service_register_data_receive_callback,
    .trigger_event = sle_service_trigger_event,
    .svr_is_connected = svr_is_connected,
    .svr_get_mode = svr_get_mode,
    .svr_set_mode = svr_set_mode,
    .svr_s_generate_pair_code = svr_s_generate_pair_code,
    .svr_get_paired_devices = svr_get_paired_devices,
    .svr_get_paired_and_discovered_devices = svr_get_paired_and_discovered_devices,
};


/* 服务任务 */
static int sle_service_Task(void *param) {    
    SdCardService *sd_card_service = NULL;
     WifiService* wifi_service = NULL;
    
    SleConfig msle_config={0};
    (void)param;
    
    /* 初始化事件队列互斥锁 */
    goldie_mutex_init(&event_queue_mutex);    
    goldie_mutex_init(&event_cb_mutex);
    goldie_mutex_init(&data_cb_mutex);
	
    /* 等待驱动就绪 */
    sle_drv = wait_drv(SLE_DRV_INDEX);
    if (!sle_drv) {
        SLE_SVC_ERROR("Failed to get SLE driver\n");
        return -1;
    }

    sd_card_service = (SdCardService *)wait_service(SDCARD_SERVICE_INDEX);
    wifi_service = wait_service(WIFI_SERVICE_INDEX);
    /* 系统启动时加载SLE配置 */
    load_sle_config(&msle_config);
	
    load_paired_devices_from_file();
    SLE_SVC_INFO("SLE driver ready\n");	
    printf("\r\n[SLE CONFIG] Config already loading...\r\n");
    sle_config.mode = msle_config.mode;
    wifi_service->svr_get_hwddr(local_addr,6);
    #ifdef SLE_SWITCH_DISABLE
    if(sle_drv->enable()){
		 printf(" sle init error \r\n");
		 return -1;
	}
    #endif
    if(msle_config.enabled){
    	svr_enable();
    }
    /* 注册服务 */
    register_service(SLE_SDP_SERVICE_INDEX, &sleservice);
    
    SLE_SVC_INFO("SLE service registered\n");
    
    /* 主循环 */
    while (1) {
        process_events();
        check_sle_status();
        goldie_msleep(100); /* 100ms检查间隔 */
    }
    
    return 0;
}

/* 任务句柄 */
static void *task_handle = NULL;

/* 服务入口函数 */
static void sle_service_entry(void) {
    goldie_thread_lock();
    task_handle = goldie_thread_create(sle_service_Task, 0, "sle_service_Task", 0x1200);
    if (task_handle != NULL) {
        goldie_thread_set_priority(task_handle, 24);
    }
    goldie_thread_unlock();
}

/* 系统初始化时自动启动服务 */
GOLDIE_INIT_CALL_(sle_service_entry);
#endif
