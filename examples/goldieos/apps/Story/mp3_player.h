#ifndef INCLUDE_AUDIO_PLAYER_H
#define INCLUDE_AUDIO_PLAYER_H

#include <stdint.h>
#define USE_HELIX_DECODER
#define AUDIO_PLAYER_TASK_STACK_SIZE 0x1000
#define AUDIO_PLAYER_TASK_PRIO 20

#define PCM_BUFFER_SIZE 1024     // 恢复原来的PCM缓冲区大小
#define RESAMPLED_BUFFER_SIZE 1024  // 输出缓冲区大小
// MP3播放器状态
typedef enum {
    PLAYER_STATE_IDLE = 0,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_STOPPED
} player_state_t;

typedef struct {
    player_state_t state;
    char **playlist;
    int playlist_size;
    int current_track;
    int loop_mode;
    
    // 线程句柄
    void *decode_task_handle;    
    // 控制信号
    goldie_sem decode_sem;
    goldie_mutex player_mutex;
    
    // 文件系统
    FATFS fs;
    FIL mp3_file;
    int16_t pcm_buffer[PCM_BUFFER_SIZE];          // PCM解码缓冲区
    
    int decode_status;
    int thread_exit_flag;
    int thread_status;
    // 解码状态
    int file_eof;
    int decode_error;
} audio_player_t;
// 初始化音频播放器
int audio_player_init(void);

// 开始播放
int audio_player_play(void);

// 停止播放
int audio_player_stop(void);

// 暂停播放
int audio_player_pause(void);

// 恢复播放
int audio_player_resume(void);

// 播放下一个曲目
int audio_player_play_next(void);

// 播放上一个曲目
int audio_player_play_prev(void);

// 播放指定曲目
int audio_player_play_track(int track_index);

// 设置循环模式
int audio_player_set_loop(int loop);

// 获取当前状态
player_state_t audio_player_get_state(void);

// 获取当前曲目
int audio_player_get_current_track(void);

// 获取播放列表大小
int audio_player_get_playlist_size(void);

// 获取曲目名称
const char *audio_player_get_track_name(int track_index);

// 清理资源
void audio_player_cleanup(void);

#endif // INCLUDE_AUDIO_PLAYER_H
