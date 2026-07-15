#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 闹钟信息结构体
typedef struct AlarmInfo {
    bool enabled;
    char m_hour;
    char m_min;
    bool weekdays[7];
    char ring_index; 
} AlarmInfo;

// 闹钟服务接口
typedef struct AlarmService {
    int (*init)(void); // 服务初始化，从sd卡中加载闹钟程序。该接口由系统主进程来调用。
    int (*add_alarm)(AlarmInfo*); // 添加一个闹钟。该接口由闹钟应用来调用。返回添加的闹钟索引
    int (*del_alarm)(int index); // 删除一个闹钟。该接口由闹钟应用来调用。
    bool (*task_loop)(int,int,int); // 检查闹钟，并播放闹钟铃声，该接口由主进程来调用。
    int (*cancel_alarm)(void); // 取消正在触发的闹钟铃声。该接口由主进程来调用。
    int (*get_alarm_list)(AlarmInfo* list, int max_count); // 获取闹钟列表。该接口由闹钟应用来调用。
    int (*update_alarm)(int index, AlarmInfo* alarm); // 更新闹钟。该接口由闹钟应用来调用。
    int (*get_actived_alarm)();
} AlarmService;

#ifdef __cplusplus
}
#endif

#endif // ALARM_SERVICE_H