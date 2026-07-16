/*
 * sle_wtp_service.c
 * 星闪 WTP 服务实现
 * 提供设备管理、扫描、连接、数据收发及文件存储功能
 */
#include "goldie_osal.h"
#include "service_manager.h"
#include "sle_wtp_service.h"
#include "driver_manager.h"
#include "platform/ws63/sle_drv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "tiny_codec_opus.h"
#define CHECK_MSG_TIMEOUT
/*============================================================================
 * 数据类型定义
 *============================================================================*/

/*============================================================================
 * 静态变量
 *============================================================================*/

static void *task_handle = NULL;                 // 任务句柄
static SleSdpService* msle_service = NULL;          // 星闪服务指针
static EventService *evt_service = NULL;

// 队列相关
static recv_queue_item_t recv_queue[RECV_QUEUE_SIZE]={0};
static int recv_queue_head = 0;
static int recv_queue_tail = 0;
static int recv_queue_count = 0;
static goldie_mutex recv_queue_mutex;

static send_file_queue_item_t send_file_queue[SEND_FILE_QUEUE_SIZE]={0};
static int send_file_queue_head = 0;
static int send_file_queue_tail = 0;
static int send_file_queue_count = 0;
static goldie_mutex send_file_queue_mutex;

static recv_file_queue_item_t recv_file_queue[RECV_FILE_QUEUE_SIZE]={0};
static int recv_file_queue_head = 0;
static int recv_file_queue_tail = 0;
static int recv_file_queue_count = 0;
static goldie_mutex recv_file_queue_mutex;


static goldie_mutex notify_mutex; // 需要初始化

static transfer_conn_context_t recv_contexts[MAX_CONNECTIONS] = {0};
static goldie_mutex recv_context_mutex; // 需要初始化
static goldie_mutex file_write_mutex;

// 接收文件与连接ID映射表
static file_conn_map_t file_conn_map[RECV_FILE_QUEUE_SIZE]={0};
static goldie_mutex file_conn_map_mutex;

// 发送状态数组（假设最多16个连接）
static send_conn_state_t send_conn_states[MAX_CONNECTIONS]={0};
static goldie_mutex send_states_mutex;

static encoder_t*  voice_enc =NULL;
static decoder_t*  voice_dec =NULL;


// 全局标志
static volatile uint8_t sle_enabled = 0;          // 星闪是否使能

// 文件名计数器
static int send_file_index = 0;
static int recv_file_index = 0;
static goldie_mutex file_index_mutex;

static uint16_t encoded_buf[MAX_ENCODE_DATA_PACKET_SIZE/2];
static uint16_t read_file_buffer[MAX_ENCODE_DATA_PACKET_SIZE/2];

static uint8_t send_out_buf[MAX_DATA_PACKET_SIZE];

#ifdef CHECK_MSG_TIMEOUT
static goldie_timeval last_recv_time;
static int recv_timeout = 0;
#define SCAN_TIMEOUT_SECONDS   5
#endif



/*============================================================================
 * 静态函数声明
 *============================================================================*/

static void sle_wkp_service_Task(void *arg);
static void sle_status_callback(SleStatusType status, void* data);
static void sle_record_data_receive_callback(uint16_t conn_id, const uint8_t *data, uint32_t len);
static int create_send_file(void);
static int create_recv_file(uint16_t conn_id, char *out_filename, size_t out_len);
static void process_received_data(const uint8_t *data, uint32_t len, uint16_t conn_id);
static void send_file_to_connections(const char *filename);
static void send_notify_weak_signal(uint16_t conn_id);
static void update_connection_states(void);
static int get_conn_index_by_id(uint16_t conn_id);
static void close_send_file(send_conn_state_t *state);
static int encode(uint16_t *wav_data, uint16_t in_size, uint8_t *out_buffer, uint16_t max_outsize);
static int decode(uint8_t *wav_data, uint16_t in_size, uint8_t *out_buffer, uint16_t max_outsize);
static int sle_wtp_write_data(const uint8_t *data, uint32_t len, uint8_t is_start, uint8_t is_stop);
static int sle_wtp_read_data(uint8_t *buffer, uint32_t size, uint16_t *conn_id);
static void flush_buffer_to_file(transfer_conn_context_t *ctx);
static void send_broadcast();
static int read_notify(wtp_notify_data *notify_data);
static int clear_notify();
static void voice_codec_init();
static void voice_codec_destroy();


#ifdef CHECK_MSG_TIMEOUT
#define RECV_TIMEOUT_SECONDS  3

static int transfer_flag  = 0;
static void kick_recv_timer(void)
{
    goldie_gettimeofday(&last_recv_time);
    recv_timeout = 0;
}

static int check_recv_timeout(void)
{
    goldie_timeval tem_time;
    goldie_gettimeofday(&tem_time);
    if (tem_time.tv_sec > (last_recv_time.tv_sec + SCAN_TIMEOUT_SECONDS))
    {
        recv_timeout = 1;
    }
    return 0;
}

static void check_msg_timeout()
{
	if(!transfer_flag){
		return;
	}
	check_recv_timeout();
	if(recv_timeout)
	{
		printf("transfer timeout need finished !\r\n");
		goldie_mutex_lock(&recv_context_mutex);
		// 查找当前连接对应的上下文
		transfer_conn_context_t *ctx = NULL;
		for (int i = 0; i < MAX_CONNECTIONS; i++) {
			if (recv_contexts[i].in_use) {
				   ctx = &recv_contexts[i];
			            flush_buffer_to_file(ctx);
			            f_close(&ctx->file);
			            ctx->in_use =0;
			            // 将完成的文件加入接收队列
			            recv_file_queue_item_t item;
			            strncpy(item.filename, ctx->filename, FILE_NAME_MAX - 1);
			            item.filename[FILE_NAME_MAX - 1] = '\0';
			            item.conn_id = ctx->conn_id;
				   printf("transfer timeout finish msg: file name  %s connid %d \r\n",ctx->filename,ctx->conn_id);
			            goldie_mutex_lock(&recv_file_queue_mutex);
			            if (recv_file_queue_count < RECV_FILE_QUEUE_SIZE) {
			                recv_file_queue[recv_file_queue_tail] = item;
			                recv_file_queue_tail = (recv_file_queue_tail + 1) % RECV_FILE_QUEUE_SIZE;
			                recv_file_queue_count++;
				       send_broadcast();
			            } else {
			                printf("[SLE WTP] Recv file queue full, drop file %s\n", ctx->filename);
			            }
				 goldie_mutex_unlock(&recv_file_queue_mutex);
			}
		}
		transfer_flag = 0;
		goldie_mutex_unlock(&recv_context_mutex);
	}
}
#endif


static void dump_wtp_data_head(sle_wtp_data_head_format *wtp_hd)
{
	printf("\r\n data_head: %x  %x  %x   index:%d  data_len:%d \r\n",wtp_hd->data_head,wtp_hd->magic_num0,
		wtp_hd->magic_num1,wtp_hd->data_index,wtp_hd->data_len);
}

static sleWtpService sle_wtp_service = {	
	.write_data = sle_wtp_write_data,
	.read_data = sle_wtp_read_data,
	.read_notify = read_notify,
	.clear_notify = clear_notify,
	.voice_codec_init =  voice_codec_init,
         .voice_codec_destroy = voice_codec_destroy,
};

/*============================================================================
 * 服务启动入口
 *============================================================================*/

static void sle_wkp_service_entry(void) {
    goldie_thread_lock();
    task_handle = goldie_thread_create(sle_wkp_service_Task, 0, "sle_wkp_service_Task", 0x1500);
    if (task_handle != NULL) {
        goldie_thread_set_priority(task_handle, 24);
    }
    goldie_thread_unlock();
}

static void creat_work_dir()
{
	FRESULT res = f_mkdir(SEND_DIR); 
	if (res == FR_OK || res == FR_EXIST) {
		printf("creat %s ok \r\n",SEND_DIR);
	}else{
		printf("creat %s failed \r\n",SEND_DIR);
	}

	res = f_mkdir(RECV_DIR); 
	if (res == FR_OK || res == FR_EXIST) {
		printf("creat %s ok \r\n",RECV_DIR);
	}else{
		printf("creat %s failed \r\n",RECV_DIR);
	}

}
// 系统初始化调用
GOLDIE_INIT_CALL_(sle_wkp_service_entry);


static void trigger_connect()
{
	if (!msle_service->svr_is_enabled()) 
		return;
	if(msle_service->svr_get_mode() ==1) 
		return;

	// 获取扫描结果
	sle_device_info_t scan_devices[20];
	uint32_t scan_count = 20;
	if (msle_service->svr_get_scan_results(scan_devices, &scan_count) == 0) {
		// 获取已配对设备列表
		sle_connection_info_t paired_devices[20];
		uint32_t paired_count = 20;
		if (msle_service->svr_get_paired_devices(paired_devices, &paired_count) == 0) {
		    // 对扫描到的设备，如果已在配对列表中且未连接，则发起连接
		    for (uint32_t i = 0; i < scan_count; i++) {
		        for (uint32_t j = 0; j < paired_count; j++) {
		            // 比较地址（假设 sle_addr_t 可逐字节比较）
		            if (memcmp(&scan_devices[i].addr, &paired_devices[j].addr, sizeof(sle_addr_t)) == 0) {
		                // 检查是否已连接
		                if (paired_devices[j].state != 1) { // 未连接
		                    printf("[SLE WTP] Connecting to paired device: %s\n", scan_devices[i].name);
		                    msle_service->svr_connect(&scan_devices[i].addr);
		                }
		                break;
		            }
		        }
		    }
		}
	}
}
/*============================================================================
 * 主任务
 *============================================================================*/

static void sle_wkp_service_Task(void *arg) {
    // 初始化互斥锁
    goldie_mutex_init(&recv_queue_mutex);
    goldie_mutex_init(&send_file_queue_mutex);
    goldie_mutex_init(&recv_file_queue_mutex);
    goldie_mutex_init(&file_conn_map_mutex);
    goldie_mutex_init(&send_states_mutex);
    goldie_mutex_init(&file_index_mutex);
    goldie_mutex_init(&recv_context_mutex);
    goldie_mutex_init(&notify_mutex);
    goldie_mutex_init(&file_write_mutex);
    // 清空映射表
    memset(file_conn_map, 0, sizeof(file_conn_map));
    memset(send_conn_states, 0, sizeof(send_conn_states));

    // 等待星闪服务
    msle_service = (SleSdpService*)wait_service(SLE_SDP_SERVICE_INDEX);
    evt_service = (EventService*)wait_service(EVENT_SERVICE_INDEX);
    // 注册状态回调
    msle_service->register_callback(sle_status_callback, SLE_PROFILE_INDEX_WKP);

    // 注册数据接收回调
    msle_service->register_data_receive_callback(sle_record_data_receive_callback, SLE_PROFILE_INDEX_WKP);
    register_service(SLE_WTP_SERVICE_INDEX,(void*)&sle_wtp_service);
    printf("[SLE WTP] sle_wkp_service_Task started, waiting for events...\n");
    if (msle_service && msle_service->svr_get_mode() == 0) {
	   msle_service->trigger_event(SLE_EVENT_START_SCAN, NULL);
	} else {
	   msle_service->trigger_event(SLE_EVENT_START_ANNOUNCE, NULL);
    }
    creat_work_dir();

    // 主循环
    while (1) {
        // 1. 处理发送队列中的文件
        if (send_file_queue_count > 0) {
            printf("[SLE WTP DEBUG] Send file queue count = %d\n", send_file_queue_count); // 添加日志
            send_file_queue_item_t item;
            goldie_mutex_lock(&send_file_queue_mutex);
            if (send_file_queue_count > 0) {
                item = send_file_queue[send_file_queue_head];
                send_file_queue_head = (send_file_queue_head + 1) % SEND_FILE_QUEUE_SIZE;
                send_file_queue_count--;
                printf("[SLE WTP DEBUG] Dequeue file for sending: %s\n", item.filename); // 添加日志
            }
            goldie_mutex_unlock(&send_file_queue_mutex);

            // 发送文件到已连接的设备
            send_file_to_connections(item.filename);
        }

        // 2. 处理接收队列中的数据
        while (recv_queue_count > 0) {
            recv_queue_item_t item;
            goldie_mutex_lock(&recv_queue_mutex);
            if (recv_queue_count > 0) {
                item = recv_queue[recv_queue_head];
                recv_queue_head = (recv_queue_head + 1) % RECV_QUEUE_SIZE;
                recv_queue_count--;
            }
            goldie_mutex_unlock(&recv_queue_mutex);

            // 解析数据
            process_received_data(item.data, item.len, item.conn_id); // conn_id 在 item 中?
            // 注意：回调中传入的 conn_id 在 sle_data_format 里，但这里 item 只存了 raw 数据，需要从 data 中解析出 conn_id
            // 实际 item.data 前几个字节就是 sle_data_format，包含 conn_id 和 data_len
            // 因此 process_received_data 需要从 data 中提取 conn_id
        }

       trigger_connect();
        // 3. 更新连接状态（获取最新 rssi）
        update_connection_states();
#ifdef CHECK_MSG_TIMEOUT		
        check_msg_timeout();
#endif
        goldie_msleep(50); // 50ms 循环，避免 CPU 占用过高
    }
}

/*============================================================================
 * 状态回调函数
 *============================================================================*/


static void sle_status_callback(SleStatusType status, void* data) {
    switch (status) {
        case SLE_STATUS_ENABLED:
            printf("[SLE WTP] SLE enabled\n");
            // 根据模式触发扫描或广播
            if (msle_service && msle_service->svr_get_mode() == 0) {
                msle_service->trigger_event(SLE_EVENT_START_SCAN, NULL);
            } else {
                msle_service->trigger_event(SLE_EVENT_START_ANNOUNCE, NULL);
            }
            break;

        case SLE_STATUS_DISABLED:
            printf("[SLE WTP] SLE disabled\n");
            sle_enabled = 0;
            break;

        case SLE_STATUS_SCANNING:
            printf("[SLE WTP] Scanning started\n");
            break;

        case SLE_STATUS_SCAN_DONE:
            printf("[SLE WTP] Scanning stopped\n");
            break;

        case SLE_STATUS_DEVICE_LIST_UPDATED: {
            printf("[SLE WTP] Device list updated\n");
	   trigger_connect();
            break;
        }

        case SLE_STATUS_CONNECTION_LIST_UPDATED: {
            printf("[SLE WTP] Connection list updated\n");
            // 可更新本地连接状态表
            break;
        }

        case SLE_STATUS_ANNOUNCING:
            printf("[SLE WTP] Announcing started\n");
            break;

        case SLE_STATUS_ANNOUNCE_STOPPED:
            printf("[SLE WTP] Announcing stopped\n");
            break;

        case SLE_STATUS_PAIRED_STATUS_UPDATED:
            printf("[SLE WTP] Paired status changed\n");
            // 可选：更新UI或设备列表
            break;

        default:
            printf("[SLE WTP] Unknown status: %d\n", status);
            break;
    }
}

/*============================================================================
 * 数据接收回调（在中断/低级别上下文调用，禁止文件操作）
 *============================================================================*/

static void sle_record_data_receive_callback(uint16_t conn_id, const uint8_t *data, uint32_t len) {
    // 快速入队，不做任何处理
    if (len > RECV_BUFFER_SIZE) {
        printf("[SLE WTP] Receive data too large: %u > %d\n", len, RECV_BUFFER_SIZE);
        return;
    }

    goldie_mutex_lock(&recv_queue_mutex);
    if (recv_queue_count < RECV_QUEUE_SIZE) {
        // 拷贝数据到队列
        memcpy(recv_queue[recv_queue_tail].data, data, len);
        recv_queue[recv_queue_tail].len = len;
        recv_queue[recv_queue_tail].conn_id = conn_id;
        recv_queue_tail = (recv_queue_tail + 1) % RECV_QUEUE_SIZE;
        recv_queue_count++;
    } else {
        printf("[SLE WTP] Receive queue full, dropping packet\n");
    }
    goldie_mutex_unlock(&recv_queue_mutex);
}

// 根据 conn_id 查找或分配上下文
static transfer_conn_context_t* find_or_alloc_context(uint16_t conn_id) {
    transfer_conn_context_t *free_ctx = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (recv_contexts[i].in_use && recv_contexts[i].conn_id == conn_id) {
            return &recv_contexts[i];
        }
        if (!recv_contexts[i].in_use && free_ctx == NULL) {
            free_ctx = &recv_contexts[i];
        }
    }
    return free_ctx; // 返回空闲项（未初始化）
}


// 将缓冲区数据写入文件
static void flush_buffer_to_file(transfer_conn_context_t *ctx) {
    if (ctx->buffer_len == 0) 
    		return;
    goldie_mutex_lock(&file_write_mutex);	
    UINT bw;
    FRESULT res = f_write(&ctx->file, ctx->buffer, ctx->buffer_len, &bw);
    if (res != FR_OK || bw != ctx->buffer_len) {
        printf("[SLE WTP] File write error, res=%d\n", res);
    }
    ctx->buffer_len = 0;
     goldie_mutex_unlock(&file_write_mutex);
}

// 将接收到的数据包存入缓冲区，必要时写入文件
static void append_data_to_buffer(transfer_conn_context_t *ctx, const uint8_t *data, uint16_t len) {
	int deltasize = 0;
    if (ctx->buffer_len + len > sizeof(ctx->buffer)) {
	deltasize = sizeof(ctx->buffer) - ctx->buffer_len;
	memcpy(ctx->buffer + ctx->buffer_len, data, deltasize);
	ctx->buffer_len = sizeof(ctx->buffer);
	flush_buffer_to_file(ctx); 
    }
    memcpy(ctx->buffer + ctx->buffer_len, data+deltasize, len-deltasize);
    ctx->buffer_len += len-deltasize;
	
    if (ctx->buffer_len == sizeof(ctx->buffer)) {
        flush_buffer_to_file(ctx);
    }
}

static void send_broadcast(){
	evt_service->send_keyevent(EVENT_KEY_CODE_SLE_WTP_EVENT);
}

static wtp_notify_data g_notify={0};
	
static int read_notify(wtp_notify_data *notify_data){
	int ret =0;
	goldie_mutex_lock(&notify_mutex);
	if(g_notify.flag){
		ret = g_notify.flag;
		memcpy(notify_data,&g_notify,sizeof(wtp_notify_data));
	}
	goldie_mutex_unlock(&notify_mutex);
	return ret;
}

static int clear_notify(){
	goldie_mutex_lock(&notify_mutex);
	memset(&g_notify,0,sizeof(wtp_notify_data));
	goldie_mutex_unlock(&notify_mutex);
	return 0;
}

static void set_notify(uint16_t connid,uint8_t id){
	goldie_mutex_lock(&notify_mutex);
	g_notify.conn_id = connid;
	g_notify.notify_id = id;
	g_notify.flag = 1;
	goldie_mutex_unlock(&notify_mutex);
}


static void process_received_data(const uint8_t *data, uint32_t len, uint16_t conn_id) {
    // 解析协议头
    sleMultiProfileDataFormat *mp = (sleMultiProfileDataFormat*)data;
    if (mp->profile_id != SLE_PROFILE_INDEX_WKP) {
        printf("error profile id %d\r\n", mp->profile_id);
        return;
    }

    sle_wtp_data_format *wtp = (sle_wtp_data_format*)mp->buffer;
    if ((wtp->data_head != WTP_DATA_HEAD) ||
        (wtp->magic_num0 != WTP_MAGIC_NUM0) ||
        (wtp->magic_num1 != WTP_MAGIC_NUM1)) {
        printf("error data format return\r\n");
        return;
    }

    if((wtp->data_len ==1)&&(wtp->buffer[0] == WEAK_SIGNAL_NOTIFY_CODE)&&(wtp->data_index=0xffff))
    {
    	set_notify(conn_id,1);
	send_broadcast();
	return;
    }

    #ifdef CHECK_MSG_TIMEOUT
	kick_recv_timer();
    #endif
    int wtp_index = wtp->data_index;
   
    // 加锁保护上下文数组
    goldie_mutex_lock(&recv_context_mutex);

    // 查找当前连接对应的上下文
    transfer_conn_context_t *ctx = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (recv_contexts[i].in_use && recv_contexts[i].conn_id == conn_id) {
            ctx = &recv_contexts[i];
            break;
        }
    }

    printf("conn[%d]: wtp_index is %d \r\n",conn_id,wtp_index);
    // 判断是否为新文件开始（首包或序号重置）
    int is_new_file = (wtp_index == 0) || (ctx && wtp_index < 5 && ctx->last_index > 50);

    if (is_new_file) {
	transfer_flag = 1;
        // 如果已有打开的文件，先关闭并加入队列
        if (ctx && ctx->in_use) {
            // 刷新缓冲区剩余数据
            flush_buffer_to_file(ctx);
            f_close(&ctx->file);

            // 将完成的文件加入接收队列
            recv_file_queue_item_t item;
            strncpy(item.filename, ctx->filename, FILE_NAME_MAX - 1);
            item.filename[FILE_NAME_MAX - 1] = '\0';
            item.conn_id = ctx->conn_id;

            goldie_mutex_lock(&recv_file_queue_mutex);
            if (recv_file_queue_count < RECV_FILE_QUEUE_SIZE) {
                recv_file_queue[recv_file_queue_tail] = item;
                recv_file_queue_tail = (recv_file_queue_tail + 1) % RECV_FILE_QUEUE_SIZE;
                recv_file_queue_count++;
	       send_broadcast();
            } else {
                printf("[SLE WTP] Recv file queue full, drop file %s\n", ctx->filename);
            }
            goldie_mutex_unlock(&recv_file_queue_mutex);

            // 标记上下文为空闲
            ctx->in_use = 0;
            ctx = NULL;
        }

        // 为新文件分配或复用上下文
        if (ctx == NULL) {
            // 找空闲上下文
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (!recv_contexts[i].in_use) {
                    ctx = &recv_contexts[i];
                    break;
                }
            }
        }

        if (ctx == NULL) {
            printf("[SLE WTP] No free context for conn %d\n", conn_id);
            goldie_mutex_unlock(&recv_context_mutex);
            return;
        }

        // 初始化新上下文
        memset(ctx, 0, sizeof(transfer_conn_context_t));
        ctx->conn_id = conn_id;
        if (create_recv_file(conn_id, ctx->filename, sizeof(ctx->filename)) != 0) {
            printf("[SLE WTP] Failed to create filename for conn %d\n", conn_id);
            ctx->in_use = 0;
            goldie_mutex_unlock(&recv_context_mutex);
            return;
        }

        FRESULT res = f_open(&ctx->file, ctx->filename, FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            printf("[SLE WTP] Failed to open file %s, err=%d\n", ctx->filename, res);
            ctx->in_use = 0;
            goldie_mutex_unlock(&recv_context_mutex);
            return;
        }

        ctx->in_use = 1;
        ctx->buffer_len = 0;
        ctx->last_index = 0; // 新文件开始，last_index 将从此包开始
    }

    // 确保此时 ctx 有效
    if (!ctx || !ctx->in_use) {
        printf("[SLE WTP] No context for conn %d after new file check\n", conn_id);
        goldie_mutex_unlock(&recv_context_mutex);
        return;
    }

    // 构造待写入的数据（包含头部和数据）
    // 注意：sle_wtp_data_format 结构包含头部，其后是数据，直接使用 wtp 指针即可
    // 数据总长度为 sizeof(sle_wtp_data_head_format) + wtp->data_len
    uint16_t total_pkt_len = sizeof(sle_wtp_data_head_format) + wtp->data_len;
    uint8_t *pkt_data = (uint8_t*)wtp; // 指向头部开始

    // 将数据包存入缓冲区
    append_data_to_buffer(ctx, pkt_data, total_pkt_len);

    // 如果是末包，刷新缓冲区、关闭文件、加入队列并释放上下文
    if (wtp_index == 0xFFFF) {
        transfer_flag = 0;
        flush_buffer_to_file(ctx);
        f_close(&ctx->file);

        recv_file_queue_item_t item;
        strncpy(item.filename, ctx->filename, FILE_NAME_MAX - 1);
        item.filename[FILE_NAME_MAX - 1] = '\0';
        item.conn_id = ctx->conn_id;

        goldie_mutex_lock(&recv_file_queue_mutex);
        if (recv_file_queue_count < RECV_FILE_QUEUE_SIZE) {
            recv_file_queue[recv_file_queue_tail] = item;
            recv_file_queue_tail = (recv_file_queue_tail + 1) % RECV_FILE_QUEUE_SIZE;
            recv_file_queue_count++;
	   send_broadcast();
        } else {
            printf("[SLE WTP] Recv file queue full, drop file %s\n", ctx->filename);
        }
        goldie_mutex_unlock(&recv_file_queue_mutex);

        // 释放上下文
        ctx->in_use = 0;
    }

    // 更新 last_index
    ctx->last_index = wtp_index;

    goldie_mutex_unlock(&recv_context_mutex);
}

/*============================================================================
 * 创建发送文件
 *============================================================================*/

static int create_send_file(void) {
    char path[FILE_PATH_MAX];
    goldie_mutex_lock(&file_index_mutex);
    snprintf(path, sizeof(path), "%s%d.sle", SEND_DIR, send_file_index);
    send_file_index = (send_file_index + 1) % (MAX_FILE_INDEX + 1);
    goldie_mutex_unlock(&file_index_mutex);

    FIL fp;
    FRESULT res = f_open(&fp, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("[SLE WTP] Failed to create send file: %s, err=%d\n", path, res);
        return -1;
    }
    f_close(&fp);

    // 将文件名加入发送队列
    send_file_queue_item_t item;
    strncpy(item.filename, path, FILE_NAME_MAX - 1);
    item.filename[FILE_NAME_MAX - 1] = '\0';

    goldie_mutex_lock(&send_file_queue_mutex);
    if (send_file_queue_count < SEND_FILE_QUEUE_SIZE) {
        send_file_queue[send_file_queue_tail] = item;
        send_file_queue_tail = (send_file_queue_tail + 1) % SEND_FILE_QUEUE_SIZE;
        send_file_queue_count++;
    } else {
        printf("[SLE WTP] Send file queue full, drop file %s\n", path);
        goldie_mutex_unlock(&send_file_queue_mutex);
        return -1;
    }
    goldie_mutex_unlock(&send_file_queue_mutex);

    return 0;
}

/*============================================================================
 * 创建接收文件
 *============================================================================*/

static int create_recv_file(uint16_t conn_id, char *out_filename, size_t out_len) {
    char path[FILE_PATH_MAX];
    goldie_mutex_lock(&file_index_mutex);
    snprintf(path, sizeof(path), "%s%d.sle", RECV_DIR, recv_file_index);
    recv_file_index = (recv_file_index + 1) % (MAX_FILE_INDEX + 1);
    goldie_mutex_unlock(&file_index_mutex);

    strncpy(out_filename, path, out_len - 1);
    out_filename[out_len - 1] = '\0';
    return 0;
}

#if 1
static void send_file_to_connections(const char *filename) {
    printf("[SLE WTP DEBUG] Enter send_file_to_connections, file: %s\n", filename);
    if (!msle_service->svr_is_enabled()) {
        printf("[SLE WTP DEBUG] SLE not enabled, abort send\n");
        return;
    }

    // 获取所有连接
    sle_connection_info_t conns[MAX_CONNECTIONS];
    uint32_t count = MAX_CONNECTIONS;
    if (msle_service->svr_get_connections(conns, &count) != 0) {
        printf("[SLE WTP DEBUG] Failed to get connections\n");
        return;
    }
    printf("[SLE WTP DEBUG] Got %u connections\n", count);

    // 先遍历连接，区分弱信号和强信号设备
    uint32_t weak_ids[MAX_CONNECTIONS];
    uint32_t weak_count = 0;
    uint32_t strong_ids[MAX_CONNECTIONS];
    uint32_t strong_count = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (conns[i].state != 1) {
            printf("[SLE WTP DEBUG] Conn %d state=%d, skip\n", conns[i].conn_id, conns[i].state);
            continue;
        }

        int8_t rssi;
        if (msle_service->svr_get_rssi(conns[i].conn_id, &rssi) != 0) {
            printf("[SLE WTP DEBUG] Failed to get RSSI for conn %d\n", conns[i].conn_id);
            continue;
        }
        printf("[SLE WTP DEBUG] Conn %d RSSI = %d\n", conns[i].conn_id, rssi);

        if (rssi < RSSI_WEAK_THRESHOLD) {
            weak_ids[weak_count++] = conns[i].conn_id;
        } else {
            strong_ids[strong_count++] = conns[i].conn_id;
        }
    }

    // 先给弱信号设备发送通知
    for (uint32_t i = 0; i < weak_count; i++) {
        printf("[SLE WTP DEBUG] RSSI weak, send notify to conn %d\n", weak_ids[i]);
        send_notify_weak_signal(weak_ids[i]);
    }

    // 如果没有强信号设备，直接返回
    if (strong_count == 0) {
        printf("[SLE WTP DEBUG] No strong signal devices, exit\n");
        return;
    }

    // 打开文件一次
    FIL file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        printf("[SLE WTP] Failed to open send file: %s, err=%d\n", filename, res);
        return;
    }

    // 构造 sleMultiProfileDataFormat 头部（假设 profile_id = WKP）
    sleMultiProfileDataFormat *mp = (sleMultiProfileDataFormat *)send_out_buf;
    mp->profile_id = SLE_PROFILE_INDEX_WKP;

    uint32_t total_packets = 0;
    uint32_t total_bytes = 0;

    // 循环读取文件数据块
    while (1) {
        sle_wtp_data_format *wtp_buf = mp->buffer;

        // 读取头部
        UINT br;
        res = f_read(&file, wtp_buf, sizeof(sle_wtp_data_head_format), &br);
        if (res != FR_OK || br == 0) {
            printf("[SLE WTP DEBUG] Read header failed or EOF, res=%d, br=%u\n", res, br);
            break; // 文件结束或出错
        }

        // 验证头部魔数
        if ((wtp_buf->data_head != WTP_DATA_HEAD) ||
            (wtp_buf->magic_num0 != WTP_MAGIC_NUM0) ||
            (wtp_buf->magic_num1 != WTP_MAGIC_NUM1)) {
            printf("[SLE WTP DEBUG] send file has error data_head \r\n");
            dump_wtp_data_head(wtp_buf);
            break;
        }

        // 读取数据部分
        res = f_read(&file, wtp_buf->buffer, wtp_buf->data_len, &br);
        if (res != FR_OK || br == 0 || br != wtp_buf->data_len) {
            printf("[SLE WTP DEBUG] Read data failed, res=%d, br=%u, expected=%u\n", res, br, wtp_buf->data_len);
            break;
        }
        printf("send index  is %d  \r\n:",wtp_buf->data_index);
        // 更新 mp 中的 data_len（整个 payload 长度）
        mp->data_len = sizeof(sle_wtp_data_head_format) + br;
        
        // 将这一数据块发送给所有强信号设备
        for (uint32_t j = 0; j < strong_count; j++) {
            int ret = msle_service->svr_send_data(strong_ids[j], send_out_buf,
                                                   sizeof(sleMultiProfileDataFormat_head) + mp->data_len);
            if (ret != 0) {
                printf("[SLE WTP] Send failed to conn %d, ret=%d\n", strong_ids[j], ret);
                // 继续发送给其他设备
            } else {
                // 发送成功计数（可选）
            }
        }

        total_packets++;
        total_bytes += br;
    }

    f_close(&file);
    printf("[SLE WTP DEBUG] Finished sending to %u strong devices, total packets=%d, bytes=%u\n",
           strong_count, total_packets, total_bytes);
    printf("[SLE WTP DEBUG] Exit send_file_to_connections\n");
}
#endif
/*============================================================================
 * 发送弱信号通知
 *============================================================================*/

static void send_notify_weak_signal(uint16_t conn_id) {
    uint8_t notify_buf[sizeof(sleMultiProfileDataFormat) + sizeof(sle_wtp_data_format)];
    sleMultiProfileDataFormat *mp = (sleMultiProfileDataFormat*)notify_buf;
    mp->profile_id = SLE_PROFILE_INDEX_WKP;
    sle_wtp_data_format *wtp = (sle_wtp_data_format*)mp->buffer;
    wtp->data_head = WTP_DATA_HEAD;
    wtp->magic_num0 = WTP_MAGIC_NUM0;
    wtp->magic_num1 = WTP_MAGIC_NUM1;
    wtp->data_len = 1;
    wtp->data_index = 0xFFFF; // 末包标记
    wtp->buffer[0] = WEAK_SIGNAL_NOTIFY_CODE;

    mp->data_len = sizeof(sle_wtp_data_format);

    msle_service->svr_send_data(conn_id, notify_buf, sizeof(sleMultiProfileDataFormat) + mp->data_len);
    printf("[SLE WTP DEBUG] Sent weak signal notify to conn %d\n", conn_id);
}

/*============================================================================
 * 更新连接状态（获取最新 RSSI）
 *============================================================================*/

static void update_connection_states(void) {
    sle_connection_info_t conns[MAX_CONNECTIONS];
    uint32_t count = MAX_CONNECTIONS;
    if (msle_service->svr_get_connections(conns, &count) != 0) {
        return;
    }

    goldie_mutex_lock(&send_states_mutex);
    // 重置所有连接状态为未连接
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        send_conn_states[i].connected = 0;
    }
    for (uint32_t i = 0; i < count; i++) {
        int idx = get_conn_index_by_id(conns[i].conn_id);
        if (idx < 0) {
            // 新连接，找一个空闲槽
            for (int j = 0; j < MAX_CONNECTIONS; j++) {
                if (!send_conn_states[j].connected) {
                    idx = j;
                    break;
                }
            }
        }
        if (idx >= 0) {
            send_conn_states[idx].conn_id = conns[i].conn_id;
            send_conn_states[idx].connected = (conns[i].state == 1);
            // 获取 RSSI
            msle_service->svr_get_rssi(conns[i].conn_id, &send_conn_states[idx].rssi);
        }
    }
    goldie_mutex_unlock(&send_states_mutex);
}

static int get_conn_index_by_id(uint16_t conn_id) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (send_conn_states[i].connected && send_conn_states[i].conn_id == conn_id) {
            return i;
        }
    }
    return -1;
}

static void close_send_file(send_conn_state_t *state) {
    if (state->cur_filename[0] != '\0') {
        f_close(&state->file);
        state->cur_filename[0] = '\0';
        state->sending = 0;
    }
}


static void voice_codec_init()
{
	voice_enc = encoder_init();
	voice_dec = decoder_init();
}

static void voice_codec_destroy()
{
	encoder_destroy(voice_enc);
	decoder_destroy(voice_dec);
	voice_enc=NULL;
	voice_dec= NULL;
}

/*============================================================================
 * 编码器/解码器（预留空实现，直接拷贝）
 *============================================================================*/

static int encode(uint16_t *wav_data, uint16_t in_size, uint8_t *out_buffer, uint16_t max_outsize) {
 int samples_cost;
 int encode_len = encode_buffer(voice_enc, wav_data, in_size/2,out_buffer,&samples_cost); 
 return encode_len;
}

static int decode(uint8_t *encode_data, uint16_t in_size, uint8_t *out_buffer, uint16_t max_outsize) {
   int samples_cost;
   int samples = decode_buffer(voice_dec, encode_data,in_size,out_buffer,&samples_cost);
   if(samples<0){
	printf("decode error \r\n");
   }
    return samples*2;
}

static transfer_conn_context_t send_ctx={0};
/*============================================================================
 * 对外接口（供应用调用）
 *============================================================================*/

/**
 * @brief 应用调用写入数据（带 start/stop 标记）
 * @param data 数据指针
 * @param len 数据长度
 * @param is_start 是否为开始包
 * @param is_stop 是否为结束包
 * @return 0成功，-1失败
 */
int sle_wtp_write_data(const uint8_t *data, uint32_t len, uint8_t is_start, uint8_t is_stop) {
      if (!msle_service->svr_is_enabled()) {
        printf("[SLE WTP] SLE not enabled, write rejected\n");
        return -1;
    }
	  
	 if(voice_enc ==NULL)
	 {
		printf("codec has not init \r\n");
		return 0;
	 }

    static FIL send_file;
    static char send_filename[FILE_NAME_MAX] = {0};

    // 如果是开始包，创建新文件
    if (is_start) {
        printf("[SLE WTP DEBUG] Start packet, creating new file\n");
        // 关闭之前的文件（如果有）
        if (send_ctx.filename[0] != '\0') {
            f_close(&send_ctx.file);
            printf("[SLE WTP DEBUG] Closed previous file: %s\n", send_ctx.filename);
            send_ctx.filename[0] = '\0';
        }

        // 生成新文件名
        goldie_mutex_lock(&file_index_mutex);
        snprintf(send_ctx.filename, sizeof(send_ctx.filename), "%s%d.sle", SEND_DIR, send_file_index);
        send_file_index = (send_file_index + 1) % (MAX_FILE_INDEX + 1);
        goldie_mutex_unlock(&file_index_mutex);
        FRESULT res = f_open(&send_ctx.file, send_ctx.filename, FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            printf("[SLE WTP] Failed to create send file: %s, err=%d\n", send_ctx.filename, res);
            send_ctx.filename[0] = '\0';
            return -1;
        }
	send_ctx.buffer_len = 0;
	send_ctx.last_index = 0;
    }

    // 如果文件未打开，无法写入
    if (send_ctx.filename[0] == '\0') {
        printf("[SLE WTP DEBUG] No file open for writing\n");
        return -1;
    }

    int encoded_len = encode((uint16_t*)data, len, encoded_buf, sizeof(encoded_buf));    
    // 写入文件
    UINT bw;
    sle_wtp_data_head_format data_head;
    data_head.data_head = WTP_DATA_HEAD;
    data_head.magic_num0 = WTP_MAGIC_NUM0;
    data_head.magic_num1 = WTP_MAGIC_NUM1;
    data_head.data_len = encoded_len;
    if(is_stop){
        if(is_stop&&is_start){
            data_head.data_index = WTP_DATA_INDEX_START_END;
        }else{
            data_head.data_index = WTP_DATA_INDEX_END;
        }
    }else{
    	data_head.data_index = send_ctx.last_index;
	send_ctx.last_index++;
    }

#if 0
    FRESULT res = f_write(&send_ctx.file, &data_head, sizeof(sle_wtp_data_head_format), &bw);
        if (res != FR_OK || bw != sizeof(sle_wtp_data_head_format)) {
        printf("[SLE WTP0] File write error %d \n",bw);
        return -1;
    }
	
     res = f_write(&send_ctx.file, encoded_buf, encoded_len, &bw);
    if (res != FR_OK || bw != encoded_len) {
        printf("[SLE WTP1] File write error %d \n",bw);
        return -1;
    }
#else
append_data_to_buffer(&send_ctx, &data_head, sizeof(sle_wtp_data_head_format));
append_data_to_buffer(&send_ctx, encoded_buf, encoded_len);
#endif	
	
    // 如果是结束包，关闭文件并加入发送队列
    if (is_stop) {
        printf("[SLE WTP DEBUG] Stop packet, closing file and queueing for send\n");
	flush_buffer_to_file(&send_ctx);
	send_ctx.last_index = 0;
        f_close(&send_ctx.file);

        // 加入发送队列
        send_file_queue_item_t item;
        strncpy(item.filename, send_ctx.filename, FILE_NAME_MAX - 1);
        item.filename[FILE_NAME_MAX - 1] = '\0';

        goldie_mutex_lock(&send_file_queue_mutex);
        if (send_file_queue_count < SEND_FILE_QUEUE_SIZE) {
            send_file_queue[send_file_queue_tail] = item;
            send_file_queue_tail = (send_file_queue_tail + 1) % SEND_FILE_QUEUE_SIZE;
            send_file_queue_count++;
        } else {
            printf("[SLE WTP] Send file queue full, drop file %s\n", send_ctx.filename);
        }
        goldie_mutex_unlock(&send_file_queue_mutex);

        send_ctx.filename[0] = '\0';
    }

    return 0;
}

/**
 * @brief 应用调用读取数据
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @param conn_id 输出参数，返回数据来源的连接ID
 * @return 实际读取的数据长度，0表示无数据
 */
int sle_wtp_read_data(uint8_t *buffer, uint32_t size, uint16_t *conn_id) {
    if (!msle_service->svr_is_enabled()) {
        return 0;
    }
    if(voice_dec == NULL)
    {
	printf("codec has not init \r\n");
	return 0;
     }
    static FIL recv_file;
    static char recv_filename[FILE_NAME_MAX] = {0};
    static uint16_t recv_conn_id = 0;
    static uint32_t file_pos = 0;

    // 如果没有打开的文件，从接收队列中取一个
    if (recv_filename[0] == '\0') {
        goldie_mutex_lock(&recv_file_queue_mutex);
        if (recv_file_queue_count > 0) {
            recv_file_queue_item_t item = recv_file_queue[recv_file_queue_head];
            recv_file_queue_head = (recv_file_queue_head + 1) % RECV_FILE_QUEUE_SIZE;
            recv_file_queue_count--;

            strncpy(recv_filename, item.filename, FILE_NAME_MAX - 1);
            recv_filename[FILE_NAME_MAX - 1] = '\0';
            recv_conn_id = item.conn_id;
            printf("[SLE WTP DEBUG] read_data: pop file %s for reading\n", recv_filename);
        }
        goldie_mutex_unlock(&recv_file_queue_mutex);

        if (recv_filename[0] != '\0') {
            FRESULT res = f_open(&recv_file, recv_filename, FA_READ);
            if (res != FR_OK) {
                printf("[SLE WTP] Failed to open recv file: %s\n", recv_filename);
                recv_filename[0] = '\0';
                return 0;
            }
            file_pos = 0;
        } else {
            return 0; // 无文件可读
        }
    }

    // 读取数据
    UINT br;
    sle_wtp_data_head_format wtp_dhead;
    FRESULT res = f_read(&recv_file, &wtp_dhead, sizeof(wtp_dhead), &br);
    if(br !=sizeof(wtp_dhead)){
		printf("[wtp0] read error data len %d wtp_dhead.data_len %d \r\n",br,sizeof(wtp_dhead));
    }	
    if (res != FR_OK) {
        printf("[SLE WTP] File read error\n");
        f_close(&recv_file);
        recv_filename[0] = '\0';
        return 0;
    }

    if((wtp_dhead.data_head != WTP_DATA_HEAD) ||
		(wtp_dhead.magic_num0 != WTP_MAGIC_NUM0) ||
		(wtp_dhead.magic_num1 != WTP_MAGIC_NUM1)){
		printf("read data file is error data_head  \r\n");
		dump_wtp_data_head(&wtp_dhead);
    }
   
    res = f_read(&recv_file, read_file_buffer, wtp_dhead.data_len, &br);
    if(br !=wtp_dhead.data_len){
		printf("[wtp1] read error data len %d wtp_dhead.data_len %d \r\n",br,wtp_dhead.data_len);
    }
    // 解码（直接拷贝）
    int decoded_len = decode(read_file_buffer, br, buffer, size);
    // 检查是否读完
    if (f_eof(&recv_file)) {
        f_close(&recv_file);
        recv_filename[0] = '\0';
    }
    *conn_id = recv_conn_id;
    return decoded_len;
}
