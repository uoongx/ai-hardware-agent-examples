//mfcc_lib.h
#ifndef __MFCC_LIB_H__
#define __MFCC_LIB_H__


#define MAX_MEMPOOL_LEN 0x1000

#define SAMPLE_RATE 8000
#define FRAME_LENGTH_MS 30

#define FFT_SIZE_8K30MS        (FRAME_LENGTH_MS*(SAMPLE_RATE/1000))
#define REAL_FFT_SIZE   FFT_SIZE_8K30MS
#define FRAME_SIZE   FFT_SIZE_8K30MS

// Q15 格式参数
#define Q15_SCALE       (1 << 15)  // 2^15
#define Q15_MAX         32767
#define Q15_MIN        (-32768)

// 类型定义
typedef int16_t pcm16_t;         // PCM16 原始类型
typedef int16_t q15_t;           // Q15 定点数类型


#define NUM_MEL_FILTERS 20
#define NUM_MFCC_COEFFS 13

// Mel滤波器参数结构
typedef struct {
    int start_idx;
    int center_idx;
    int end_idx;
    float* weights;
} MelFilter;

// MFCC计算器
typedef struct {
    MelFilter filters[NUM_MEL_FILTERS];
    float dct_matrix[NUM_MFCC_COEFFS][NUM_MEL_FILTERS];
    float mel_freq_points[NUM_MEL_FILTERS + 2];
} MfccCalculator;

void init_mfcc_calculator(void);
void uinit_mfcc(void);
int priv_mfcc_8k30ms(short* audio_data,float *mfcc);
void normalize_mfcc(float *mfcc_out, int num_frames,int mfcc_dim);
#endif