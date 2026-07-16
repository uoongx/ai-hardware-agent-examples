#include "main_ui.h"
#include "goldie_thread.h"
#include "animation_player.h"

extern "C" {
#include "goldie_osal.h"
#include "service_manager.h"
#include "app_manager.h"
}

static int thread_running = 0;
static void *task_handle = NULL;
static int running_flag = 0;
static AudioService* audio_service = NULL;
static char init_flag = 0;
static Goldie_Thread* play_thread;

static int label1_setImg(uint16_t* imgbuffer) {
    if (!label1) {
        return 0;
    }
    label1->setImageBuffer((uint16_t*)imgbuffer);
    return 0;
}

static Animation_Container boot_animation = {
    7,  // count
    {   // pic_list 数组
        gImage_frame0,
        gImage_frame1,
        gImage_frame2,
        gImage_frame3,
        gImage_frame4,
        gImage_frame5,
        gImage_frame6
    }
};

static int play_task(void *param) {
    param = param;
    int timeout = 0;
    thread_running = 1;
    while (init_flag) {
        while (running_flag) {
            for (int i = 0; i < boot_animation.count; i++) {
                label1_setImg((uint16_t*)(boot_animation.pic_list[i]));
                goldie_msleep(80);
            }
            timeout--;
        }
        goldie_msleep(500);
    }
    thread_running = 0;
    window_exit();
    return 0; // 成功返回
}

static int play_animation(void) {
    running_flag = 1;
    audio_service = (AudioService*)get_service(AUDIO_SERVICE_INDEX);
    if (!audio_service) {
        return 0;
    }
    audio_service->play_start();
    audio_service->play_ring(AUDIO_RING_ID_POWERON,0);
    return 0;
}

static void stop_animation(void) {
    running_flag = 0;

    audio_service = (AudioService*)get_service(AUDIO_SERVICE_INDEX);
    if (!audio_service) {
        return;
    }
    audio_service->play_stop();
    window->setVisible(false);
}

static void goldie_app_run(void) {
    main_ui_init();
    init_flag = 1;
    goldie_thread_lock();
    play_thread = new Goldie_Thread(play_task, NULL, 0x1000);
    goldie_thread_unlock();
    play_animation();
}

static void goldie_app_suspend(void) {
    window_suspend();
    stop_animation();
}

static void goldie_app_resume(void) {
    window_resume();
    play_animation();
}

static void goldie_app_exit(void) {
    stop_animation();
}

static void goldie_touch_event(int pressure, int x, int y);
static void goldie_keyboard_event(int pressure, int key);

static App_t animationplayerapp = {
    "boot_animation", // app_name
    goldie_app_run, // h_app_run
    goldie_app_exit, // h_app_exit
    goldie_app_suspend, // h_app_suspend
    goldie_app_resume, // h_app_resume
    goldie_touch_event, // h_touch_event
    goldie_keyboard_event, // h_keyboard_event
    NULL,
};

static void goldie_touch_event(int pressure, int x, int y) {
    printf("boot_animation touch evt :%d %d %d \r\n", pressure, x, y);
}

static void goldie_keyboard_event(int pressure, int key) {
    printf("boot_animation keyboard evt: %d %d \r\n", pressure, key);
    if ((key == SYSTEM_KEY_VALUE_BACK) && (pressure == 1)) {
        goldie_exit_app(&animationplayerapp);
    }
}

static void animation_player_entry(void) {
    install_app(&animationplayerapp);
}

GOLDIE_INIT_CALL_(animation_player_entry);
