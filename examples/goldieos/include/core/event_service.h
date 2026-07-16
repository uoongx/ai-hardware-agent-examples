#ifndef _INCLUDE_EVENT_SERVICE_
#define _INCLUDE_EVENT_SERVICE_
#include <stdint.h>
#include <string.h>
#include "goldie_osal.h"

#define EVENT_SERVICE_STACK_SIZE               0x1000
#define EVENT_SERVICE_PRIO 24

/*keyevent---------------------------------------*/
#define MAX_CALLBACKS 20
#define EVENT_QUEUE_SIZE 100

enum EVENT_KEYCODE{
	EVENT_KEY_CODE_NON = 0,
	EVENT_KEY_CODE_WAKEUP,//唤醒后连接服务器
	EVENT_KEY_CODE_CLOUD_SLEEP,
	EVENT_KEY_CODE_CLOUD_RESTART,
	EVENT_KEY_CODE_CLOUD_STOP,	
	EVENT_KEY_CODE_START_RECORD,//连接服务器后开始录音发送音频
	EVENT_KEY_CODE_WAKEUP_AIATLK,
	EVENT_KEY_CODE_TTS,//接收到信号开始发送文字
	EVENT_KEY_CODE_NEW_MSG,
	EVENT_KEY_CODE_START_PLAY,
	EVENT_KEY_CODE_FINISH_PLAY,
	EVENT_KEY_CODE_PLAY,
	EVENT_KEY_CODE_VOLUME_DOWN,
	EVENT_KEY_CODE_VOLUME_UP,
	EVENT_KEY_CODE_PLAY_NEXT,
	EVENT_KEY_CODE_STOP,
	EVENT_KEY_CODE_SDCARD_INSERT,   //insert  remove
	EVENT_KEY_CODE_SDCARD_REMOVE,	
	EVENT_KEY_CODE_HEADSET_INSERT,	
	EVENT_KEY_CODE_HEADSET_REMOVE,
	EVENT_KEY_CODE_SLE_WTP_EVENT,
	EVENT_KEY_CODE_MAX,
	EVENT_KEY_CODE_FINISH_RECORD=EVENT_KEY_CODE_VOLUME_DOWN,
};

enum EVENT_BROADCAST{
	EVENT_BROADCAST_NON = 0,
	EVENT_BROADCAST_ADDMSG,
	EVENT_BROADCAST_INPUT_METHOD,
	EVENT_BROADCAST_POWER,
	EVENT_BROADCAST_TIMERUPDATE,
	EVENT_BROADCAST_SHUTDOWN,
	EVENT_BROADCAST_MAX,
};

typedef void (*KeyEventHandler)(int);
typedef struct {
    int keycode;
    KeyEventHandler handler;
} CallbackEntry;

typedef struct {
    int buffer[EVENT_QUEUE_SIZE];
    int head;
    int tail;
    goldie_mutex mutex;
} EventQueue;

typedef struct keyevtInst{
    CallbackEntry callbacks[MAX_CALLBACKS];
    int cb_count;
    EventQueue queue;
    void *task_handle;
} keyevtInst;


/*boardcast--------------------------------------*/
#define MAX_RECEIVERS 10

// ������Ϣ�ṹ��
typedef struct {
    uint16_t id;        // �㲥ID
    void* msg;          // ��Ϣָ��,���ڴ����ַ����Ͷ���������
    uint16_t msg_len;   // ��Ϣ����
    unsigned int ext[4];   //������չ�����������ͣ���ǿ��ת�����������ݽṹ�壬�ܳ��Ȳ�����16����.
} BroadcastMessage;

// ��������߽ṹ��
typedef struct {
    uint16_t id;                            // ���ĵĹ㲥ID
    void (*callback)(BroadcastMessage*);    // �ص�����ָ��
} BroadcastReceiver;

// �㲥ϵͳ�ṹ��
typedef struct {
    BroadcastReceiver receivers[MAX_RECEIVERS]; // ����������
    uint8_t receiver_count;                     // ��ǰ����������
} BroadcastSystem;

typedef void (*boardcast_callback)(BroadcastMessage*);

typedef struct EventService {
	void (*send_keyevent)(int);
    void (*register_keyevt_cb)(int,KeyEventHandler); //ע��һ��keycode �Ͷ�Ӧ�Ļص�����
	int8_t (*register_broadcast_recv)(uint16_t, boardcast_callback);
	void (*send_broadcast)(BroadcastMessage*);

} EventService;
#endif

