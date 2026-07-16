#ifndef INCLUDE_AUALGO_SERVICE_H
#define INCLUDE_AUALGO_SERVICE_H

/* Task configuration */
#define AUALGO_TASK_STACK_SIZE 0xa00
#define AUALGO_TASK_PRIO  24

/* Audio buffer configuration */
#define AUALGO_AUDIO_BUFFER_SIZE 240
#define AUALGO_MFCC_ROWS 70
#define AUALGO_MFCC_COLS 13
#define AUALGO_READ_SIZE 160

/* Audio processing constants */
#define AUALGO_SILENCE_THRESHOLD 5.0f
#define AUALGO_WAKEUP_THRESHOLD 60
#define AUALGO_DETECT_MASK_TIMES 5
#define AUALGO_WAKEUP_TIMEOUT_SECONDS 5
#define AUALGO_SLEEP_MS_WHEN_PAUSED 50
#ifdef USE_EXT_ASR_CHIP_AC2817
#define AUALGO_SLEEP_MS_WHEN_RESUMED 100
#else
#define AUALGO_SLEEP_MS_WHEN_RESUMED 500
#endif
#define AUALGO_MFCC_FEATURE_SIZE 13
#define AUALGO_AUDIO_FRAME_SIZE 240
#define AUALGO_SHIFT_SIZE 80
#define AUALGO_READ_FRAME_SIZE 160

/* Energy calculation constants */
#define AUALGO_ENERGY_INDEX 0  /* Energy is at index 0 in MFCC features */

typedef struct AualgoService {
    void (*run)(void);
    void (*puase)(void);
} AualgoService;

#endif
