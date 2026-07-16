#ifndef _RING_BUFFER_
#define _RING_BUFFER_
#include "goldie_osal.h"
typedef struct {
    uint8_t *buffer;//[RING_BUFFER_SIZE];
    int head;
    int tail;
    unsigned int count;
    unsigned int buffer_len;
   goldie_mutex ringbuf_mutex;
} RingBuffer;

// 初始化环形缓冲区
void ring_buffer_init(RingBuffer *cb);

// 检查缓冲区是否为空
int ring_buffer_is_empty(RingBuffer *cb) ;

// 检查缓冲区是否已满
int ring_buffer_is_full(RingBuffer *cb);

// 单个写入
int ring_buffer_write_byte_noblock(RingBuffer *cb, uint8_t data) ;

// 批量写入
int ring_buffer_bulk_write_block(RingBuffer *cb, const uint8_t *data, unsigned int size);

// 单个读取
int ring_buffer_read_byte_noblock(RingBuffer *cb, uint8_t *data) ;

// 批量读取
int ring_buffer_bulk_read_block(RingBuffer *cb, uint8_t *data, unsigned int size);
int ring_buffer_bulk_read_noblock(RingBuffer *cb, uint8_t *data, unsigned int size);
int ring_buffer_bulk_read_double_block(RingBuffer *cb, uint8_t *data, unsigned int size);
int ring_buffer_bulk_write_half_block(RingBuffer *cb, const uint8_t *data, unsigned int size);
int ring_buffer_bulk_write_half_noblock(RingBuffer *cb, const uint8_t *data, unsigned int size);
int ring_buffer_bulk_write_noblock(RingBuffer *cb, const uint8_t *data, unsigned int size);
// 打印缓冲区内容（仅用于调试）
void ring_buffer_print(RingBuffer *cb);
void ring_buffer_reset(RingBuffer *cb);
#endif
