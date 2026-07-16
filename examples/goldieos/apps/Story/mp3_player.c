#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "goldie_osal.h"
#include "service_manager.h"
#include "ringbuffer.h"
#include "ff.h"
#include "helix_mp3.h"
#include "mp3_player.h"

int16_t resampled_pcm[RESAMPLED_BUFFER_SIZE];

static audio_player_t g_player;

int saved_index;//记录故事索引值
// 使用helix_mp3库进行MP3解码
static helix_mp3_t g_helix_mp3;
static int g_helix_mp3_initialized = 0;
// #define DEBUG_MSG_ON

static void print_with_time(char* log) {
#ifdef DEBUG_MSG_ON
    goldie_timeval temp_tv;
    goldie_gettimeofday(&temp_tv);
    printf("%s - %d(ms) \r\n", log, temp_tv.tv_sec * 1000 + temp_tv.tv_usec / 1000);
#endif
}

// 自定义I/O接口用于FatFs文件系统
static int helix_mp3_fatfs_seek(void *user_data, int offset) {
    FIL *file = (FIL *)user_data;
    if (f_lseek(file, offset) != FR_OK) {
        return -1;
    }
    return 0;
}

static size_t helix_mp3_fatfs_read(void *user_data, void *buffer, size_t size) {
    FIL *file = (FIL *)user_data;
    UINT bytes_read;
    if (f_read(file, buffer, size, &bytes_read) != FR_OK) {
        return 0;
    }
    return bytes_read;
}

static helix_mp3_io_t fatfs_io = {
    .seek = helix_mp3_fatfs_seek,
    .read = helix_mp3_fatfs_read
};

// 使用helix_mp3库进行MP3解码
static int helix_mp3_decode_frame(int16_t *pcm_buffer, int *pcm_samples) {
    if (!g_helix_mp3_initialized) {
        printf("g_helix_mp3_initialized not ready \r\n");
        return -1;
    }

    // 使用helix_mp3库读取PCM帧
    size_t frames_read = helix_mp3_read_pcm_frames_s16(&g_helix_mp3, pcm_buffer, PCM_BUFFER_SIZE);
    if (frames_read == 0) {
        printf("no pcm data \r\n");
        // 文件结束或解码错误
        *pcm_samples = 0;
        return -1;
    }

    /*
    #ifdef OUTPUT_MONO_AUDIO
    // 立体声转单声道：取左右声道的平均值
    for (size_t i = 0; i < frames_read; i++) {
        int16_t left = pcm_buffer[i * 2];
        int16_t right = pcm_buffer[i * 2 + 1];
        pcm_buffer[i] = (left/2 + right/2);
    }
    #endif
    */
    *pcm_samples = frames_read; // 单声道样本数
    return 0;
}

// 简单的MP3帧头解析（用于备用解码器）
typedef struct {
    uint32_t syncword : 11;
    uint32_t version : 2;
    uint32_t layer : 2;
    uint32_t protection : 1;
    uint32_t bitrate_index : 4;
    uint32_t frequency : 2;
    uint32_t padding : 1;
    uint32_t private_bit : 1;
    uint32_t mode : 2;
    uint32_t mode_extension : 2;
    uint32_t copyright : 1;
    uint32_t original : 1;
    uint32_t emphasis : 2;
} mp3_header_t;

// 解析MP3帧头（用于备用解码器）
static int parse_mp3_header(const uint8_t *header, mp3_header_t *mp3h) {
    // 检查同步字：0xFF 0xE0-0xFF
    if (header[0] != 0xFF || (header[1] & 0xE0) != 0xE0) {
        return -1; // 无效的同步字
    }

    // 检查版本和层
    uint8_t version_bits = (header[1] >> 3) & 0x3;
    uint8_t layer_bits = (header[1] >> 1) & 0x3;

    // 验证版本和层
    if (version_bits == 1 || layer_bits == 0) {
        return -1; // 保留值，无效
    }

    mp3h->syncword = ((header[0] << 3) | (header[1] >> 5)) & 0x7FF;
    mp3h->version = version_bits;
    mp3h->layer = layer_bits;
    mp3h->protection = header[1] & 0x1;
    mp3h->bitrate_index = (header[2] >> 4) & 0xF;
    mp3h->frequency = (header[2] >> 2) & 0x3;
    mp3h->padding = (header[2] >> 1) & 0x1;
    mp3h->private_bit = header[2] & 0x1;
    mp3h->mode = (header[3] >> 6) & 0x3;
    mp3h->mode_extension = (header[3] >> 4) & 0x3;
    mp3h->copyright = (header[3] >> 3) & 0x1;
    mp3h->original = (header[3] >> 2) & 0x1;
    mp3h->emphasis = header[3] & 0x3;

    // 验证比特率索引
    if (mp3h->bitrate_index == 0 || mp3h->bitrate_index == 15) {
        return -1; // 无效的比特率
    }

    return 0;
}

static void resample_to_8k(const int16_t *input, int input_samples,
                           int16_t *output, int *output_samples, int original_rate) {
    if (original_rate == 8000) {
        memcpy(output, input, input_samples * sizeof(int16_t));
        *output_samples = input_samples;
        return;
    }

    float ratio = (float)original_rate / 8000.0f;
    *output_samples = (int)(input_samples / ratio);

    if (*output_samples > RESAMPLED_BUFFER_SIZE) {
        *output_samples = RESAMPLED_BUFFER_SIZE;
    }

    // 使用4点sinc插值（比线性插值质量更好）
    for (int i = 0; i < *output_samples; i++) {
        float pos = i * ratio;
        int idx = (int)pos;
        float frac = pos - idx;

        // 获取4个相邻样本
        int idx0 = idx - 1;
        int idx1 = idx;
        int idx2 = idx + 1;
        int idx3 = idx + 2;

        // 边界处理
        idx0 = (idx0 < 0) ? 0 : idx0;
        idx1 = (idx1 < 0) ? 0 : idx1;
        idx2 = (idx2 >= input_samples) ? input_samples - 1 : idx2;
        idx3 = (idx3 >= input_samples) ? input_samples - 1 : idx3;

        // 4点sinc插值系数
        float x = frac;
        float a0 = -0.5f * x + x * x - 0.5f * x * x * x;
        float a1 = 1.0f - 2.5f * x * x + 1.5f * x * x * x;
        float a2 = 0.5f * x + 2.0f * x * x - 1.5f * x * x * x;
        float a3 = -0.5f * x * x + 0.5f * x * x * x;

        float sample = a0 * input[idx0] + a1 * input[idx1] + a2 * input[idx2] + a3 * input[idx3];

        // 限制范围
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;

        output[i] = (int16_t)sample;
    }
}

// 解码线程 - 使用helix_mp3库的分帧方法
static int decode_thread(void *arg) {
    audio_player_t *player = (audio_player_t *)arg;
    AudioService *audio_service = (AudioService *)wait_service(AUDIO_SERVICE_INDEX);

    // 初始化缓冲区状态
    player->thread_status = 1;
    while (1) {
        if (player->state != PLAYER_STATE_PLAYING) {
            player->decode_status = 0;
        }
        goldie_sem_wait(&player->decode_sem);
        if (player->thread_exit_flag) {
            break;
        }

        if (player->state != PLAYER_STATE_PLAYING) {
            player->decode_status = 0;
            continue;
        }

        player->decode_status = 1;
        // 初始化helix_mp3解码器
        if (!g_helix_mp3_initialized) {
            fatfs_io.user_data = &player->mp3_file;
            if (helix_mp3_init(&g_helix_mp3, &fatfs_io) == 0) {
                g_helix_mp3_initialized = 1;
            } else {
                printf("Failed to initialize Helix MP3 decoder\n");
                continue;
            }
        }

        // 持续解码播放，直到文件结束或停止
        while (player->state == PLAYER_STATE_PLAYING && !player->file_eof) {
            // 确保每次只处理一个解码周期
            goldie_mutex_lock(&player->player_mutex);

            int original_rate = helix_mp3_get_sample_rate(&g_helix_mp3);
            if (original_rate == 0) {
                original_rate = 44100; // 默认采样率
            }
            // 使用helix_mp3库解码帧
            int pcm_samples;
            int decode_result = helix_mp3_decode_frame(player->pcm_buffer, &pcm_samples);
            if (decode_result == 0 && pcm_samples > 0) {
                // 重采样到8kHz
                int resampled_samples;
                resample_to_8k(player->pcm_buffer, pcm_samples, resampled_pcm, &resampled_samples, original_rate);
                int write_result = audio_service->audio_write(resampled_pcm, resampled_samples * sizeof(int16_t));
                if (write_result < 0) {
                    printf("Audio write failed: %d\n", write_result);
                }
            } else {
                // 解码结束或错误
                player->file_eof = 1;
                goldie_mutex_unlock(&player->player_mutex);
                break;
            }

            // 解锁互斥锁，允许下一个解码周期
            goldie_mutex_unlock(&player->player_mutex);
            // 短暂延时，避免过度占用CPU
            goldie_msleep(10);
        }
        // 检查文件是否结束，如果是循环模式则重新开始
        if (player->file_eof) {
            if (player->loop_mode) {
                // 清理当前解码器
                if (g_helix_mp3_initialized) {
                    helix_mp3_deinit(&g_helix_mp3);
                    g_helix_mp3_initialized = 0;
                }

                // 重新定位文件
                f_lseek(&player->mp3_file, 0);
                player->file_eof = 0;
                goldie_sem_post(&player->decode_sem);
            } else {
                g_player.decode_status = 0;
                audio_player_stop();
            }
        }
    }

    // 清理解码器
    if (g_helix_mp3_initialized) {
        helix_mp3_deinit(&g_helix_mp3);
        g_helix_mp3_initialized = 0;
    }
    player->thread_status = 0;
    printf("decode thread exited \r\n");
    return 0;
}

// 遍历SD卡music目录，搜索MP3文件
static int scan_music_directory(audio_player_t *player) {
    FRESULT res;
    DIR dir;
    FILINFO fno;
    char longname[_MAX_LFN * 3 + 1]; 
    char *path = "/music";


    // 初始化播放列表
    player->playlist_size = 0;
    player->playlist = NULL;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        return -1;
    }

    // 第一次遍历，统计MP3文件数量
    int mp3_count = 0;
    for (;;) {
	memset(longname,0,_MAX_LFN * 3 + 1);
	fno.lfname = longname;        
	fno.lfsize = sizeof(longname); 	
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;

        // 检查是否为MP3文件
        char *ext = strrchr(fno.fname, '.');
        if (ext && strcasecmp(ext, ".mp3") == 0) {
            mp3_count++;
        }
    }

    if (mp3_count == 0) {
        f_closedir(&dir);
        return 0;
    }

    // 分配播放列表内存
    player->playlist = (char **)goldie_malloc(mp3_count * sizeof(char *));
    if (!player->playlist) {
        f_closedir(&dir);
        return -1;
    }
    memset(player->playlist, 0, mp3_count * sizeof(char *));
    // 重新遍历，保存文件名
    f_rewinddir(&dir);
    int index = 0;
    for (;;) {
	memset(longname,0,_MAX_LFN * 3 + 1);
	fno.lfname = longname;        
	fno.lfsize = sizeof(longname); 
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;

        char *ext = strrchr(fno.fname, '.');
        if (ext && strcasecmp(ext, ".mp3") == 0) {
	  const char* filename = fno.lfname[0] ? fno.lfname : fno.fname;
	   printf("fno.lfname[0] is 0x%x  strlen %d \r\n",fno.lfname[0],strlen(filename));
            // 分配内存并保存文件名
            player->playlist[index] = (char *)goldie_malloc(strlen(filename) + strlen(path) + 2);

			
            if (player->playlist[index]) {
                sprintf(player->playlist[index], "%s/%s", path, filename);
                index++;
            }
    	}
    }

    player->playlist_size = index;
    f_closedir(&dir);

    return player->playlist_size;
}

// 打开并播放指定曲目
static int play_track(audio_player_t *player, int track_index) {
    if (track_index < 0 || track_index >= player->playlist_size) {
        return -1;
    }

    // 关闭当前文件（如果有）
    if (player->mp3_file.fs) {
        f_close(&player->mp3_file);
    }

    // 打开新文件
    FRESULT res = f_open(&player->mp3_file, player->playlist[track_index], FA_READ);
    if (res != FR_OK) {
        return -1;
    }

    player->current_track = track_index;
    player->file_eof = 0;
    player->decode_error = 0;

    return 0;
}

static int initialized = 0;

// 公共API函数

// 初始化音频播放器
int audio_player_init(void) {
    int time_out = 10;
    
    // 检查是否已经初始化
    if (initialized) {
        return 0;
    }
    

    memset(&g_player, 0, sizeof(audio_player_t));
    SdCardService *sdcard_service = (SdCardService *)wait_service(SDCARD_SERVICE_INDEX);

    while (!sdcard_service->IsSdCardExists() && (time_out > 0)) {
        goldie_msleep(100);
        time_out--;
    }

    if (!sdcard_service->IsSdCardExists()) {
        AudioService *audio_service = (AudioService *)get_service(AUDIO_SERVICE_INDEX);
        if (audio_service) {
            audio_service->play_start();
            audio_service->play_ring(AUDIO_RING_ID_POWERON,0);
        }
        printf("sd card not exists\n");
        return -1;
    }
    // 初始化信号量和互斥锁
    goldie_sem_init(&g_player.decode_sem);
    goldie_mutex_init(&g_player.player_mutex);
    // 扫描音乐目录
    if (scan_music_directory(&g_player) <= 0) {
        printf("no music\n");
        return -1;
    }
    g_player.current_track=saved_index;//恢复索引值

    // 创建解码线程（确保只创建一次）
    goldie_thread_lock();
    if (g_player.decode_task_handle == NULL) {
        g_player.decode_task_handle = goldie_thread_create(decode_thread, &g_player,
                                                          "mp3_decode", AUDIO_PLAYER_TASK_STACK_SIZE);
        if (g_player.decode_task_handle != NULL) {
            goldie_thread_set_priority(g_player.decode_task_handle, AUDIO_PLAYER_TASK_PRIO);
        } else {
            goldie_thread_unlock();
            printf("decode_task_handle error\n");
            return -1;
        }
    }
    goldie_thread_unlock();

    g_player.state = PLAYER_STATE_STOPPED;
    g_player.loop_mode = 0;
    initialized = 1;
    
    return 0;
}

// 开始播放
int audio_player_play(void) {
    if (g_player.state == PLAYER_STATE_PLAYING) {
        return 0; // 已经在播放
    }
    
    if (g_player.playlist_size == 0) {
        return -1;
    }
    
    // 如果没有当前曲目，从第一个开始
    if (g_player.current_track < 0) {
        g_player.current_track = 0;
    }
    
    if (play_track(&g_player, g_player.current_track) != 0) {
        return -1;
    }
    printf("audio_service start \r\n");
    // 启动音频服务
    AudioService *audio_service = (AudioService *)get_service(AUDIO_SERVICE_INDEX);
    if (audio_service) {
         audio_service->play_start();	
    }
    
    g_player.state = PLAYER_STATE_PLAYING;

    // 启动解码线程
    goldie_sem_post(&g_player.decode_sem);    
    printf("play decode_sem is %p \r\n",&g_player.decode_sem);
    return 0;
}

// 停止播放
int audio_player_stop(void) {
   int timeout_count =0;
    if (g_player.state == PLAYER_STATE_STOPPED) {
        return 0;
    }
    
    g_player.state = PLAYER_STATE_STOPPED;

    printf("wait decoder stoped \r\n");
    while(g_player.decode_status){
	goldie_msleep(30);
	timeout_count++;
	if(timeout_count>20)
	{
		printf("wait decode_status timeout! \r\n");
		break;
	}
   }   
   printf("decoder has stoped \r\n");
    // 停止音频服务
    AudioService *audio_service = (AudioService *)get_service(AUDIO_SERVICE_INDEX);
    if (audio_service) {
        audio_service->play_stop();
    }
    // 关闭文件
    if (g_player.mp3_file.fs) {
        f_close(&g_player.mp3_file);
        memset(&g_player.mp3_file, 0, sizeof(FIL));
    }
    
    // 清理helix_mp3解码器
    if (g_helix_mp3_initialized) {
        helix_mp3_deinit(&g_helix_mp3);
        g_helix_mp3_initialized = 0;
    }

    return 0;
}

// 暂停播放
int audio_player_pause(void) {
    if (g_player.state != PLAYER_STATE_PLAYING) {
        return 0;
    }
    
    g_player.state = PLAYER_STATE_PAUSED;
    
    return 0;
}

// 恢复播放
int audio_player_resume(void) {
    if (g_player.state != PLAYER_STATE_PAUSED) {
        return 0;
    }
    
    g_player.state = PLAYER_STATE_PLAYING;
    goldie_sem_post(&g_player.decode_sem);
    
    return 0;
}

// 播放下一个曲目
int audio_player_play_next(void) {
    if (g_player.playlist_size == 0) {
        return -1;
    }
    
    int next_track = (g_player.current_track + 1) % g_player.playlist_size;
    return audio_player_play_track(next_track);
}

// 播放上一个曲目
int audio_player_play_prev(void) {
    if (g_player.playlist_size == 0) {
        return -1;
    }
    
    int prev_track = (g_player.current_track - 1 + g_player.playlist_size) % g_player.playlist_size;
    return audio_player_play_track(prev_track);
}

// 播放指定曲目
int audio_player_play_track(int track_index) {
    if (track_index < 0 || track_index >= g_player.playlist_size) {
        return -1;
    }
    
    // 如果正在播放，先停止
    if (g_player.state == PLAYER_STATE_PLAYING) {
        audio_player_stop();
        goldie_msleep(30); // 等待停止完成
    }
    
    g_player.current_track = track_index;
    return audio_player_play();
}

// 设置循环模式
int audio_player_set_loop(int loop) {
    g_player.loop_mode = loop ? 1 : 0;
    return 0;
}

// 获取当前状态
player_state_t audio_player_get_state(void) {
    return g_player.state;
}

// 获取当前曲目
int audio_player_get_current_track(void) {
    return g_player.current_track;
}

// 获取播放列表大小
int audio_player_get_playlist_size(void) {
    return g_player.playlist_size;
}

// 获取曲目名称
const char *audio_player_get_track_name(int track_index) {
    if (track_index < 0 || track_index >= g_player.playlist_size) {
        return NULL;
    }
    
    // 返回文件名（不含路径）
    const char *full_path = g_player.playlist[track_index];
    const char *filename = strrchr(full_path, '/');
    return filename ? filename + 1 : full_path;
}
// 清理资源
void audio_player_cleanup(void) {
    audio_player_stop();
    printf("audio_player_cleanup \r\n");
    g_player.thread_exit_flag = 1;
    goldie_sem_post(&g_player.decode_sem);
#if 0	
    while(g_player.thread_status);{
        goldie_msleep(30);
    }  
    printf("thread has stoped \r\n");
#endif	
    // 释放播放列表内存
    if (g_player.playlist) {
        for (int i = 0; i < g_player.playlist_size; i++) {
            if (g_player.playlist[i]) {
                goldie_free(g_player.playlist[i]);
            }
        }
        goldie_free(g_player.playlist);
        g_player.playlist = NULL;
    }
    
    g_player.playlist_size = 0;

    saved_index=g_player.current_track;//记录索引值
    
    g_player.current_track = -1;    
  
   goldie_thread_destroy(g_player.decode_task_handle);   
   initialized = 0;   
}
