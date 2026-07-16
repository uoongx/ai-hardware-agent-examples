#include "main_ui.h"
extern "C" {
#include "goldie_osal.h"
#include "service_manager.h"
#include "app_manager.h"
}
#include "app_icon.h"
#include "goldie_thread.h"
#include "rgb16_bowtie_56_53.h"
#include "rgb16_closeeye_l_88_85.h"
#include "rgb16_closeeye_r_88_85.h"
#include "rgb16_closeeye_r1_88_85.h"
#include "rgb16_closeeye_r2_88_85.h"
#include "rgb16_closeeye_r3_88_85.h"
#include "rgb16_eye_88_85.h"
#include "rgb16_half_l_88_85.h"
#include "rgb16_half_r_88_85.h"
#include "rgb16_laugh_l_88_85.h"
#include "rgb16_laugh_r_88_85.h"
#include "rgb16_closeeye_l_new_88_85.h"
#include "rgb16_closeeye_r_new_88_85.h"
#include "rgb16_closeeye_r1_new_88_85.h"
#include "rgb16_closeeye_r2_new_88_85.h"
#include "rgb16_closeeye_r3_new_88_85.h"
#include "rgb16_eye_new_88_85.h"
#include "rgb16_half_l_new_88_85.h"
#include "rgb16_half_r_new_88_85.h"
#include "rgb16_laugh_l_new_88_85.h"
#include "rgb16_laugh_r_new_88_85.h"

#include "rgb16_angry_male_l_88_85.h"
#include "rgb16_angry_male_r1_88_85.h"
#include "rgb16_angry_male_r2_88_85.h"
#include "rgb16_doubt_male_l_88_85.h"
#include "rgb16_doubt_male_r1_88_85.h"
#include "rgb16_doubt_male_r2_88_85.h"
#include "rgb16_sad_male_l_88_85.h"
#include "rgb16_sad_male_r_88_85.h"

#include "rgb16_angry_female_l_88_85.h"
#include "rgb16_angry_female_r1_88_85.h"
#include "rgb16_angry_female_r2_88_85.h"
#include "rgb16_doubt_female_l_88_85.h"
#include "rgb16_doubt_female_r1_88_85.h"
#include "rgb16_doubt_female_r2_88_85.h"
#include "rgb16_sad_female_l_88_85.h"
#include "rgb16_sad_female_r_88_85.h"
// 移除视频相关图片头文件（新UI用眼睛/嘴巴控件，不再用整图动画）

static WifiService* wifi_service = NULL;
static EventService* mevtservice = NULL;

static goldie_mutex msgque_mutex;
#define MAX_CHAT_MSG_LEN 1500

static void *task_handle = NULL;
static int running_flag = 0;
static char init_flag = 0;
static char thread_running_status = 0;
static Goldie_Thread* play_thread;

enum {
    PLAY_TYPE_SILENCE = 0,
    PLAY_TYPE_SPEAK,
    PLAY_TYPE_SLEEP,
};

// 情绪枚举
enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_ANGRY,
    EMOTION_SAD,
    EMOTION_DOUBT,
};

int play_type = PLAY_TYPE_SILENCE;

typedef struct _Chat_Msg {
    int uid;
    char msg_buf[MAX_CHAT_MSG_LEN];
} Chat_Msg;

#define MAX_MSG_NUM 4
typedef struct {
    Chat_Msg messages[MAX_MSG_NUM];
    int head;
    int tail;
    int count;
} MsgQueue;

static MsgQueue s_msg_queue = {0};

// SDK integration via convai_bridge
#include "convai_bridge.h"
#include "convai/convai_api.h"

static int avatar_id = 0;
static int current_emotion = EMOTION_NEUTRAL;
static convai_engine_t sdk_engine = NULL;
static convai_status_e sdk_status = CONVAI_STATUS_IDLE;
static int sdk_started = 0;

static void sdk_status_callback(convai_status_e status) {
    sdk_status = status;
    printf("[AItalk] SDK status: %d\n", (int)status);
}

static int play_animation(void);
static void stop_animation(void);
static void update_status(void);

static void add_msg(BroadcastMessage* msg) {
    int msg_len = msg->msg_len;
    int userid = msg->ext[0];
    if (msg_len > (MAX_CHAT_MSG_LEN - 1)) {
        msg_len = MAX_CHAT_MSG_LEN - 1;
    }

    goldie_mutex_lock(&msgque_mutex);
    if (s_msg_queue.count < MAX_MSG_NUM) {
        s_msg_queue.messages[s_msg_queue.tail].uid = userid;
        memset(s_msg_queue.messages[s_msg_queue.tail].msg_buf, 0, MAX_CHAT_MSG_LEN);
        memcpy(s_msg_queue.messages[s_msg_queue.tail].msg_buf, msg->msg, msg_len);
        s_msg_queue.tail = (s_msg_queue.tail + 1) % MAX_MSG_NUM;
        s_msg_queue.count++;
    }
    goldie_mutex_unlock(&msgque_mutex);
}
// 模拟眼睛状态变化
static void update_avatar_ui(int play_type) {
    if (!init_flag) return;//如果正在退出APP，不刷新UI
    static int count = 0;
    static int dir = 0;
    int temp_index = 0;
    if(avatar_id)//男性
    {
        LabelView_tie->setImageBuffer((uint16_t*)&rgb16_bowtie_56_53);
        if (play_type == PLAY_TYPE_SLEEP) { 
	   temp_index = count % 8;
            const unsigned char* sleep_imgs[] = {
                rgb16_closeeye_r1_88_85,  
                rgb16_closeeye_r1_88_85, 
                rgb16_closeeye_r2_88_85,  
                rgb16_closeeye_r3_88_85,
                rgb16_closeeye_r3_88_85, 
                rgb16_closeeye_r3_88_85, 
                rgb16_closeeye_r2_88_85,  
                rgb16_closeeye_r1_88_85  
            };
            LabelView_L->setImageBuffer((uint16_t*)rgb16_closeeye_l_88_85);
            LabelView_R->setImageBuffer((uint16_t*)sleep_imgs[temp_index]);
            count = (count+1)  % 8;
        }else if (play_type == PLAY_TYPE_SILENCE) {
            if(++count==15) count=0;
            if(count==2) {
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_half_l_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_half_r_88_85);}
            else if(count==3) {
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_closeeye_l_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_closeeye_r_88_85);}
            else {
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_eye_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_eye_88_85);}   
        } else if (play_type == PLAY_TYPE_SPEAK) {
            if(current_emotion==EMOTION_NEUTRAL)//平静
            {
                //正常表情：睁眼
                LabelView_L->setImageBuffer((uint16_t*)rgb16_eye_88_85);
                LabelView_R->setImageBuffer((uint16_t*)rgb16_eye_88_85);

            }else if(current_emotion==EMOTION_HAPPY){//开心
                //开心表情循环
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_laugh_l_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_laugh_r_88_85);    
                if(dir==1){
                    if(++count>=5) count=5,dir=0;
                }else if(dir==0){
                    if(--count<=0) count=0,dir=1;
                }
                LabelView_L->setPosition(72, 66+count*2);
                LabelView_R->setPosition(8, 66+count*2);
            }else if(current_emotion==EMOTION_ANGRY){//生气
                //愤怒表情循环
                if(++count>=8)  count=0;
                if(count>3)
                {
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_angry_male_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_angry_male_r2_88_85);
                }else{
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_angry_male_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_angry_male_r1_88_85);
                }
            }else if(current_emotion==EMOTION_SAD){//伤心
                //伤心表情循环，//左右眼伤心表情，中间穿插眨眼
                if(++count>=20) count=0;
                if(count==14 || count==15|| count==18|| count==19)
                {
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_closeeye_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_closeeye_r_88_85);
                }else{
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_sad_male_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_sad_male_r_88_85);
                }

            }else if(current_emotion==EMOTION_DOUBT){//疑惑
                // //疑惑表情循环
                if(++count>=8)  count=0;
                if(count>3)
                {
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_doubt_male_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_doubt_male_r2_88_85);
                }else{
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_doubt_male_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_doubt_male_r1_88_85);
                }
            }
        }
    }
    else{//女性
        LabelView_tie->setImageBuffer((uint16_t*)&rgb16_bow_56_53);
        if (play_type == PLAY_TYPE_SLEEP) { 
	temp_index = count % 8;		
        const unsigned char* sleep_imgs[] = {
            rgb16_closeeye_r1_new_88_85,  
            rgb16_closeeye_r1_new_88_85, 
            rgb16_closeeye_r2_new_88_85,  
            rgb16_closeeye_r3_new_88_85,
            rgb16_closeeye_r3_new_88_85, 
            rgb16_closeeye_r3_new_88_85, 
            rgb16_closeeye_r2_new_88_85,  
            rgb16_closeeye_r1_new_88_85  
        };
        LabelView_L->setImageBuffer((uint16_t*)rgb16_closeeye_l_new_88_85);
        LabelView_R->setImageBuffer((uint16_t*)sleep_imgs[temp_index]);
        count = (count+1)  % 8;
        }else if (play_type == PLAY_TYPE_SILENCE) {
            
            if(++count==15) count=0;
            if(count==2) {
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_half_l_new_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_half_r_new_88_85);}
            else if(count==3) {
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_closeeye_l_new_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_closeeye_r_new_88_85);}
            else {
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_eye_new_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_eye_new_88_85);}   
        } else if (play_type == PLAY_TYPE_SPEAK) {
            
            if(current_emotion==EMOTION_NEUTRAL)//平静
            {
                //正常表情：睁眼
                LabelView_L->setImageBuffer((uint16_t*)rgb16_eye_new_88_85);
                LabelView_R->setImageBuffer((uint16_t*)rgb16_eye_new_88_85);

            }else if(current_emotion==EMOTION_HAPPY){//开心
                //开心表情循环
                LabelView_L->setImageBuffer((uint16_t*)&rgb16_laugh_l_new_88_85);
                LabelView_R->setImageBuffer((uint16_t*)&rgb16_laugh_r_new_88_85);    
                if(dir==1){
                    if(++count>=5) count=5,dir=0;
                }else if(dir==0){
                    if(--count<=0) count=0,dir=1;
                }
                LabelView_L->setPosition(72, 66+count*2);
                LabelView_R->setPosition(8, 66+count*2);
            }else if(current_emotion==EMOTION_ANGRY){//生气
                //愤怒表情循环
                if(++count>=8)  count=0;
                if(count>3)
                {
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_angry_female_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_angry_female_r2_88_85);
                }else{
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_angry_female_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_angry_female_r1_88_85);
                }
            }else if(current_emotion==EMOTION_SAD){//伤心
                //伤心表情循环，//左右眼伤心表情，中间穿插眨眼
                if(++count>=20) count=0;
                if(count==14 || count==15|| count==18|| count==19)
                {
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_closeeye_l_new_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_closeeye_r_new_88_85);
                }else{
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_sad_female_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_sad_female_r_88_85);
                }

            }else if(current_emotion==EMOTION_DOUBT){//疑惑
                // //疑惑表情循环
                if(++count>=8)  count=0;
                if(count>3)
                {
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_doubt_female_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_doubt_female_r2_88_85);
                }else{
                    LabelView_L->setImageBuffer((uint16_t*)rgb16_doubt_female_l_88_85);
                    LabelView_R->setImageBuffer((uint16_t*)rgb16_doubt_female_r1_88_85);
                }
            }
        }
    }
}

static int play_task(void *param) {
    param = param;
    while (init_flag) {
        while (running_flag) {
            update_status();
            
            // Update state from SDK status
            if (!sdk_started || sdk_status == CONVAI_STATUS_IDLE) {
                play_type = PLAY_TYPE_SLEEP;
            } else {
                if (sdk_status == CONVAI_STATUS_ANSWERING) {
                    play_type = PLAY_TYPE_SPEAK;
                } else {
                    play_type = PLAY_TYPE_SILENCE;
                }
            }

            // 更新头像UI（眼睛/嘴巴）
            update_avatar_ui(play_type);
            Window_1770202198007->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
            goldie_msleep(200);
        }
        goldie_msleep(200);
    }
    thread_running_status = 0;
    return 0;
}

static int play_animation(void) {
    running_flag = 1;
    return 0;
}

static void stop_animation(void) {
    running_flag = 0;
}

static int priv_atoi(char* str) {
    int str_len = strlen(str);
    if (str_len < 3) {
        return 0;
    }
    if ((str[0] == 'i') && (str[1] == 'd') && (str[2] == '_')) {
        return atoi(str + 3);
    }
    return 0;
}

static void update_status() {
    if (sdk_engine) {
        avatar_id = 0;  // default avatar (no cloud config yet)
        sdk_status = convai_bridge_get_status();
        current_emotion = EMOTION_NEUTRAL;
    }
}

static void cloud_status_callback(int status) {
    (void)status;
}

static void goldie_app_run(void) {
    main_ui_init();

    // 服务初始化
    mevtservice = (EventService*)wait_service(EVENT_SERVICE_INDEX);
    if (mevtservice) {
        mevtservice->register_broadcast_recv(EVENT_BROADCAST_ADDMSG, add_msg);
    }

    /* Get SDK engine via convai_bridge */
    sdk_engine = convai_bridge_get_engine();
    convai_bridge_on_status(sdk_status_callback);
    printf("[AItalk] SDK engine: %p\n", (void*)sdk_engine);

    /* Start AI conversation session */
    convai_bridge_start();
    sdk_started = 1;

    update_status();

    // 启动任务线程
    init_flag = 1;
    goldie_thread_lock();
    play_thread = new Goldie_Thread(play_task, NULL, 0x1000);
    thread_running_status = 1;
    goldie_thread_unlock();
	
    play_animation();
	
}

static void goldie_app_suspend(void) {
    stop_animation();
    window_suspend();
}

static void goldie_app_resume(void) {
    play_animation();
    window_resume();
}

static void goldie_app_exit(void) {
    Window_1770202198007->setVisible(false);	
    stop_animation();
    init_flag = 0;
    while(thread_running_status)
    {
	goldie_msleep(50);
	printf("wait for thread exit \r\n");
    }

    // 释放资源
    window_exit();
    printf("[AItalk] stopping SDK engine\n");
    convai_bridge_stop();
    sdk_started = 0;

    // 清理线程
    if (play_thread) {
        delete play_thread;
        play_thread = NULL;
    }
}

static void goldie_touch_event(int pressure, int x, int y) {
    Window_1770202198007->handleEvent(pressure, x, y);
}


static void goldie_keyboard_event(int pressure, int key);

static App_t AITalk = {
    "AI小荷",               // app_name char*
    goldie_app_run,         // h_app_run
    goldie_app_exit,        // h_app_exit
    goldie_app_suspend,     // h_app_suspend
    goldie_app_resume,      // h_app_resume
    goldie_touch_event,     // h_touch_event
    goldie_keyboard_event,  // h_keyboard_event
    (uint16_t*)rgb16_taking_56_56,
};



static void goldie_keyboard_event(int pressure, int key) {
    printf("key %d play_type %d \r\n", key, play_type);
    if ((key == SYSTEM_KEY_VALUE_BACK) && (pressure == 1)) {
        goldie_exit_app(&AITalk);
    }else if((key == SYSTEM_KEY_VALUE_WAKEUP) && (pressure == 1)) {
        if (mevtservice) {
            mevtservice->send_keyevent(EVENT_KEY_CODE_WAKEUP);
        }
    }
}


static void test_entry(void) {
    install_app(&AITalk);
}

GOLDIE_INIT_CALL_(test_entry);
