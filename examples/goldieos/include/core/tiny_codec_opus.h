#ifndef  _TINY_CODEC_OPUS_H_
#define _TINY_CODEC_OPUS_H_

#include <stdint.h>
#include <string.h>

#define SAMPLES_IN_FRAME  80     // 10ms @ 8kHz
#define ENCODE_MAGIC16    0x4F50   // "OP" 魔数
#define MAX_FRAME_BYTES   1276     // Opus 最大帧长

#ifdef CUSTOM_MODES
#include <opus_custom.h>
#define DECODER_BUFFER_SIZE 0x2390
#define ENCODER_BUFFER_SIZE 0x12a0

// 定义编码器结构体
typedef struct {
    OpusCustomEncoder *enc;
    OpusCustomMode *mode;
    // 如果使用静态缓冲区，可以不需要额外指针，但为了统一接口，仍保留 enc
} encoder_t;

typedef struct {
    OpusCustomDecoder *dec;
    OpusCustomMode *mode;
} decoder_t;
#else
#include <opus.h>
typedef struct {
    OpusEncoder *enc;
} encoder_t;

typedef struct {
    OpusDecoder *dec;
} decoder_t;
#endif
 encoder_t* encoder_init();
 decoder_t* decoder_init();
 int encode_buffer(encoder_t *enc, const int16_t *input, int input_samples,
                  uint8_t *output, int16_t *cost);
int decode_buffer(decoder_t *dec, const uint8_t *input, int input_size,
                  int16_t *output, int16_t *cost);
void encoder_destroy(encoder_t *enc);
 void decoder_destroy(decoder_t *dec);
#endif
