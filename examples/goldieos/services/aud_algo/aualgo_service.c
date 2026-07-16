#include <stdio.h>
#include <string.h>
#include <math.h>
#include "service_manager.h"
#include "core/goldie_mfcc/mfcc_lib.h"
#include "core/goldie_algo_wk.h"
#include "goldie_osal.h"
#include "aualgo_service.h"
#include "convai_bridge.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif

#ifdef USE_EXT_ASR_CHIP_AC2817
#define AC2817_XIAOHE_GPIO 11
#define AC2817_XIAOXING_GPIO 8
#else
#include "nihaoxiaohe_model.h"
#endif

static EventService *evt_service = NULL;
static AualgoService aualgo_service;

#ifdef USE_EXT_ASR_CHIP_AC2817
#ifdef DRV_CORE
static int gpio_ext_fd = -1;
static aw9523b_ioctl_arg_t ioctl_arg;
#else
static GpioDrvExt * gpio_ext_drv;
#endif
#else
static short aud_buffer[AUALGO_AUDIO_BUFFER_SIZE] = {0};
static float mfcc_buffer[AUALGO_MFCC_ROWS * AUALGO_MFCC_COLS] = {0.0f};
static float calc_mfcc_buffer[AUALGO_MFCC_ROWS * AUALGO_MFCC_COLS] ={0.0f};

#define TENSOR_ARENA_SIZE_16BIT (10 * 1024) 
static int tensor_arena_size = TENSOR_ARENA_SIZE_16BIT*2;
static uint16_t tensor_arena[TENSOR_ARENA_SIZE_16BIT];

static float new_mfcc[13];
static short aud_buffer_temp[240];
static float mean[13] = {0};
#endif

static int running_status = 0;

static int last_status = 1;

static AudioService* audioservice = NULL;
/* CloudService removed; use convai_bridge_is_speaking() */

static void *task_handle = NULL;

uint8_t detect_mask = 0;

static int Goldie_AudNetProcess(float *buffer)
{
    buffer = buffer;
    return 0;
}

static goldie_timeval last_commit_time;

uint8_t wakeup_timeout = 1;

static void print_time_stamp(char* debugmsg)
{
    goldie_timeval tem_time;
    goldie_gettimeofday(&tem_time);
    printf("\r\n [%s] tem_time sec %lu  us %lu \r\n", debugmsg, tem_time.tv_sec, tem_time.tv_usec);
}

static void kick_wakeup_timer(void)
{
    goldie_gettimeofday(&last_commit_time);
    wakeup_timeout = 0;
}

static int check_wakeup_timeout(void)
{
    goldie_timeval tem_time;
    goldie_gettimeofday(&tem_time);
    if (tem_time.tv_sec > (last_commit_time.tv_sec + AUALGO_WAKEUP_TIMEOUT_SECONDS))
    {
        wakeup_timeout = 1;
    }
    return 0;
}

#ifdef USE_EXT_ASR_CHIP_AC2817
static void ac2817_processing_loop(void)
{
	int last_io_status =0;
	while (1) {
		goldie_msleep(AUALGO_SLEEP_MS_WHEN_RESUMED);
		if (!running_status)
		{
			goldie_msleep(AUALGO_SLEEP_MS_WHEN_PAUSED);
			last_status = running_status;
			continue;
		}
		else
		{
			if (last_status == 0)
			{
				goldie_msleep(AUALGO_SLEEP_MS_WHEN_RESUMED);
				last_status = 1;
				last_status = running_status;
				continue;
			}
		}

		if (1) /* convai_bridge */
	        {
	            if (convai_bridge_is_speaking())
	            {
	                continue;
	            }
	        }

       		 check_wakeup_timeout();
		 if (wakeup_timeout == 1) {
		#ifdef DRV_CORE
		ioctl_arg.io_index = AC2817_XIAOHE_GPIO;
		goldie_ioctl(gpio_ext_fd, AW9523B_IOCTL_GET_VAL, &ioctl_arg);
		int result = ioctl_arg.value;
		#else
		int result =gpio_ext_drv->get_gpio_value(AC2817_XIAOHE_GPIO);
		#endif
		if ((result == 0)&&( last_io_status == 1)) {			
			printf("AC2817 wakeup value: %d last_io_status: %d \r\n", result,last_io_status);
			if (evt_service == NULL) {
				evt_service = get_service(EVENT_SERVICE_INDEX);
			}
			if (evt_service != NULL) {
				evt_service->send_keyevent(EVENT_KEY_CODE_WAKEUP_AIATLK);
				evt_service->send_keyevent(EVENT_KEY_CODE_WAKEUP);
				kick_wakeup_timer();
			}
		}
		last_io_status = result;
	   }
	}
	#ifdef DRV_CORE
	goldie_close(gpio_ext_fd);
	#endif
}
#else  
static void audio_processing_loop(void)
{
    memset(mfcc_buffer, 0, sizeof(mfcc_buffer));
    memset(calc_mfcc_buffer, 0, sizeof(calc_mfcc_buffer));
    audioservice->algostream_start();  
    while (1) {
        if (!running_status)
        {
            goldie_msleep(AUALGO_SLEEP_MS_WHEN_PAUSED);
            memset(aud_buffer, 0, AUALGO_AUDIO_BUFFER_SIZE * 2);
            last_status = running_status;
            continue;
        }
        else
        {
            if (last_status == 0)
            {
                goldie_msleep(AUALGO_SLEEP_MS_WHEN_RESUMED);
                last_status = 1;
                last_status = running_status;
                continue;
            }
        }
        last_status = running_status;
        memmove(aud_buffer, aud_buffer + AUALGO_SHIFT_SIZE, AUALGO_SHIFT_SIZE * sizeof(short));
	audioservice->algostream_read((char*)(aud_buffer + AUALGO_SHIFT_SIZE), AUALGO_READ_FRAME_SIZE * sizeof(short));
        check_wakeup_timeout();
        memcpy(aud_buffer_temp, aud_buffer, AUALGO_AUDIO_FRAME_SIZE * sizeof(short));
        priv_mfcc_8k30ms(aud_buffer_temp, new_mfcc);
        
        memmove(mfcc_buffer, mfcc_buffer + AUALGO_MFCC_COLS, (AUALGO_MFCC_ROWS - 1) * AUALGO_MFCC_COLS * sizeof(float));
        memcpy(mfcc_buffer + (AUALGO_MFCC_ROWS - 1) * AUALGO_MFCC_COLS, new_mfcc, AUALGO_MFCC_COLS * sizeof(float));

        /* Calculate energy standard deviation and detect silence */
        float energy_sum = 0.0f;
        float energy_sq_sum = 0.0f;
        float energy_mean, energy_std;
        int is_silence = 0;

        /* Extract energy values (index 0 of each frame) and calculate sum */
        for (int i = 0; i < AUALGO_MFCC_ROWS; i++) {
            float energy = mfcc_buffer[i * AUALGO_MFCC_COLS + AUALGO_ENERGY_INDEX];  /* Energy is at index 0 */
            energy_sum += energy;
            energy_sq_sum += energy * energy;
        }

        /* Calculate mean and standard deviation */
        energy_mean = energy_sum / AUALGO_MFCC_ROWS;
        energy_std = sqrtf((energy_sq_sum / AUALGO_MFCC_ROWS) - (energy_mean * energy_mean));
        
        /* Determine if it's silence (standard deviation < threshold means speech) */
        if (energy_std < AUALGO_SILENCE_THRESHOLD) {
            is_silence = 0;
        } else {
            is_silence = 1;
        }

        if (is_silence)
        {
            continue;
        }

        if (1) /* convai_bridge */
        {
            if (convai_bridge_is_speaking())
            {
                continue;
            }
        }

        if (detect_mask < AUALGO_DETECT_MASK_TIMES) {
            detect_mask++;
            continue;
        } else {
            detect_mask = 0;
        }

        /* Calculate mean of MFCC coefficients */
        for (int coeff = 0; coeff < AUALGO_MFCC_COLS; coeff++) {
            for (int frame = 0; frame < AUALGO_MFCC_ROWS; frame++) {
                mean[coeff] += mfcc_buffer[frame * AUALGO_MFCC_COLS + coeff];
            }
            mean[coeff] /= (float)AUALGO_MFCC_ROWS;
        }
        
        /* Normalize MFCC features by subtracting mean */
        for (int frame = 0; frame < AUALGO_MFCC_ROWS; frame++) {
            for (int coeff = 0; coeff < AUALGO_MFCC_COLS; coeff++) {
                calc_mfcc_buffer[frame * AUALGO_MFCC_COLS + coeff] = 
                    mfcc_buffer[frame * AUALGO_MFCC_COLS + coeff] - mean[coeff];
            }
        }
        
        normalize_mfcc(calc_mfcc_buffer, AUALGO_MFCC_ROWS, AUALGO_MFCC_COLS);
        if (wakeup_timeout == 1) {
            int result = wk_algo_process(calc_mfcc_buffer);
            if (result >= AUALGO_WAKEUP_THRESHOLD) {
                printf("Algorithm wakeup value: %d \r\n", result);
                if (evt_service == NULL) {
                    evt_service = get_service(EVENT_SERVICE_INDEX);
                }
                if (evt_service != NULL) {
#ifdef PLATFORM_TYPE_WIN					
		  evt_service->send_keyevent(EVENT_KEY_CODE_WAKEUP_AIATLK);
#endif
		  evt_service->send_keyevent(EVENT_KEY_CODE_WAKEUP);
                    kick_wakeup_timer();
                }
            }
        }
    }
    audioservice->algostream_stop();
}
#endif

static void service_run(void)
{
    running_status = 1;
}

static void service_puase(void)
{
    running_status = 0;
}

static int aualgo_task(void *param)
{
    param = param;
#ifdef USE_EXT_ASR_CHIP_AC2817
#ifdef DRV_CORE
		gpio_ext_fd = goldie_open(AW9523B_DRV_NAME, O_RDWR);
		ioctl_arg.io_index = AC2817_XIAOHE_GPIO;
		ioctl_arg.value = GOLDIE_GPIO_DIRECTION_INPUT;
		goldie_ioctl(gpio_ext_fd, AW9523B_IOCTL_SET_DIR, &ioctl_arg);

		ioctl_arg.io_index = AC2817_XIAOXING_GPIO;
		ioctl_arg.value = GOLDIE_GPIO_DIRECTION_INPUT;
		goldie_ioctl(gpio_ext_fd, AW9523B_IOCTL_SET_DIR, &ioctl_arg);
#else
	gpio_ext_drv = wait_drv(GPIO_EXT_DRV_INDEX);
	gpio_ext_drv->drv_init();
	gpio_ext_drv->set_gpio_func(AC2817_XIAOHE_GPIO,GOLDIE_GPIO_DIRECTION_INPUT);
	gpio_ext_drv->set_gpio_func(AC2817_XIAOXING_GPIO,GOLDIE_GPIO_DIRECTION_INPUT);
#endif
#else    
    audioservice = (AudioService *)wait_service(AUDIO_SERVICE_INDEX);
#endif
    /* convai_bridge covers this */
    aualgo_service.run = service_run;
    aualgo_service.puase = service_puase;
    register_service(AUALGO_SERVICE_INDEX, &aualgo_service);	
#ifdef USE_EXT_ASR_CHIP_AC2817       
ac2817_processing_loop();
#else
    wk_algo_init(wk_model_nihaoxiaohe_tflite,(uint8_t*)tensor_arena,tensor_arena_size);
    init_mfcc_calculator();
    audio_processing_loop();    
    uinit_mfcc();
#endif
    return 0;
}

static void aualgo_service_entry(void)
{
    printf("aualgo_service_entry !\r\n");
    goldie_thread_lock();
    task_handle = goldie_thread_create(aualgo_task, 0, "aualgo_Task", AUALGO_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        goldie_thread_set_priority(task_handle, AUALGO_TASK_PRIO);
    }
    goldie_thread_unlock();
}

GOLDIE_INIT_CALL_(aualgo_service_entry);
