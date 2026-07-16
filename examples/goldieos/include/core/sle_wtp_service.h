#ifndef SLE_WTP_INCLUDE_H_
#define SLE_WTP_INCLUDE_H_
#include "sle_sdp_service.h"

#define MAX_CONNECTIONS   8

#define MAX_ENCODE_DATA_PACKET_SIZE 140

#define WTP_DATA_INDEX_END 0xffff

#define WTP_DATA_INDEX_START_END 0xfff0

#define WTP_DATA_HEAD   0xff0e

#define WTP_MAGIC_NUM0 0x1314

#define WTP_MAGIC_NUM1 0xf3f4

#define RECV_QUEUE_SIZE             20      // 接收数据队列大小
#define RECV_BUFFER_SIZE             140     // 每个接收缓冲大小

#define SEND_FILE_QUEUE_SIZE         20      // 待发送文件队列大小
#define RECV_FILE_QUEUE_SIZE          20      // 已接收文件队列大小

#define FILE_NAME_MAX                 20      // 文件名最大长度
#define FILE_PATH_MAX                  20      // 文件路径最大长度

#define SEND_DIR                      "0:/send/"
#define RECV_DIR                      "0:/recv/"

#define MAX_FILE_INDEX                 999     // 文件最大序号
#define MIN_FILE_INDEX                 0

#define MAX_DATA_PACKET_SIZE           120     // 单次发送最大字节数

#define RSSI_WEAK_THRESHOLD            -70     // 信号弱阈值（dBm），小于此值视为弱信号
#define WEAK_SIGNAL_NOTIFY_CODE        0xFF    // 弱信号特殊通知码

typedef struct wtp_notify_data{
	uint16_t conn_id;
	uint8_t notify_id;
	uint8_t flag; 
}wtp_notify_data;

// 接收上下文结构体
typedef struct {
    uint16_t conn_id;                // 连接ID
    FIL file;                         // 文件对象
    char filename[FILE_NAME_MAX];     // 当前文件名
    uint8_t buffer[512];              // 数据缓冲区
    uint16_t buffer_len;              // 缓冲区有效数据长度
    uint32_t last_index;              // 上一个包的索引
    uint8_t in_use;                   // 是否在使用
} transfer_conn_context_t;

// 接收队列元素：存储从回调中放入的数据
typedef struct {
    uint8_t data[RECV_BUFFER_SIZE];
    uint32_t len;                          // 实际数据长度
   uint16_t conn_id;
} recv_queue_item_t;

// 发送文件队列元素：存储待发送的文件名
typedef struct {
    char filename[FILE_NAME_MAX];
} send_file_queue_item_t;

// 接收文件队列元素：存储已接收的文件名及对应的连接ID
typedef struct {
    char filename[FILE_NAME_MAX];
    uint16_t conn_id;
} recv_file_queue_item_t;

// 文件与连接ID映射（用于接收时关联）
typedef struct {
    char filename[FILE_NAME_MAX];
    uint16_t conn_id;
    uint8_t in_use;                         // 0-空闲, 1-使用中
} file_conn_map_t;

// 发送状态（每个连接一个）
typedef struct {
    uint16_t conn_id;
    int8_t   rssi;
    uint8_t  connected;                      // 连接状态
    FIL      file;                            // 当前发送文件
    char     cur_filename[FILE_NAME_MAX];
    uint32_t file_pos;                        // 文件读取位置
    uint8_t  sending;                         // 是否正在发送该连接
} send_conn_state_t;


typedef struct{
	int (*write_data)(const uint8_t *, uint32_t, bool, bool);
	int (*read_data)(uint8_t *, uint32_t, uint16_t *);
	int (*read_notify)(wtp_notify_data *);
	void  (*voice_codec_init)(void);
	void  (*voice_codec_destroy)(void);
	int  (*clear_notify)();
}sleWtpService;
#endif
