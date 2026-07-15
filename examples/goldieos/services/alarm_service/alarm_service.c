#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "alarm_service.h"
#include "service_manager.h"
#include "audio_service.h"
#include "sdcard_service.h"
#include "ntp_service.h"
#include "goldie_osal.h"
#include "ff.h"

// 最大闹钟数量
#define MAX_ALARMS 10
static int  active_alarm_index  =-1;
// 全局变量
static AlarmService alarm_service;
static AlarmInfo alarm_list[MAX_ALARMS];
static int alarm_count = 0;
static bool alarm_ringing = false;
static bool alarm_triggered_today = false;
static char last_triggered_time[6] = "00:00"; // HH:MM格式
static AudioService* audio_service = NULL;
static SdCardService* sdcard_service = NULL;

// 内部辅助函数声明
static int load_alarms_from_sd(void);
static int save_alarms_to_sd(void);
static int compare_alarm_info(const AlarmInfo* a, const AlarmInfo* b);
static int find_alarm_index(const AlarmInfo* alarm);
static int is_time_match(const AlarmInfo* alarm, char hour, char min);
static int is_weekday_match(const AlarmInfo* alarm,int weekday);

// 服务初始化函数
static int alarm_service_init(void) {
    printf("[AlarmService] Initializing alarm service...\n");
    
    // 从SD卡加载闹钟
    int ret = load_alarms_from_sd();
    if (ret != 0) {
        printf("[AlarmService] Failed to load alarms from SD card\n");
    }
    
    printf("[AlarmService] Initialized with %d alarms\n", alarm_count);
    // 获取音频服务
    if (audio_service == NULL) {
        audio_service = (AudioService*)get_service(AUDIO_SERVICE_INDEX);
    }
    return 0;
}

// 添加闹钟函数
static int alarm_service_add_alarm(AlarmInfo* alarm) {
    if (alarm == NULL) {
        printf("[AlarmService] add_alarm: invalid parameter\n");
        return -1;
    }
    
    if (alarm_count >= MAX_ALARMS) {
        printf("[AlarmService] add_alarm: maximum alarm limit reached (%d)\n", MAX_ALARMS);
        return -2;
    }

#if 0    
    // 检查是否已存在相同的闹钟
    for (int i = 0; i < alarm_count; i++) {
        if (compare_alarm_info(&alarm_list[i], alarm) == 0) {
            printf("[AlarmService] add_alarm: alarm already exists\n");
            return -3;
        }
    }
#endif

    // 添加闹钟
    memcpy(&alarm_list[alarm_count], alarm, sizeof(AlarmInfo));
    int added_index = alarm_count;
    alarm_count++;
    
    printf("[AlarmService] add_alarm: added alarm %02d:%02d, enabled=%d, total=%d, index=%d\n",
           alarm->m_hour, alarm->m_min, alarm->enabled, alarm_count, added_index);
    
    // 保存到SD卡
    save_alarms_to_sd();
    
    return added_index;
}

// 删除闹钟函数（按索引删除）
static int alarm_service_del_alarm_by_index(int index) {
    if (index < 0 || index >= alarm_count) {
        printf("[AlarmService] del_alarm: invalid index %d (alarm_count=%d)\n", index, alarm_count);
        return -1;
    }
    
    // 删除闹钟（将最后一个元素移到当前位置）
    if (index < alarm_count - 1) {
        memcpy(&alarm_list[index], &alarm_list[alarm_count - 1], sizeof(AlarmInfo));
    }
    alarm_count--;
    
    printf("[AlarmService] del_alarm: deleted alarm at index %d, remaining=%d\n",
           index, alarm_count);
    
    // 保存到SD卡
    save_alarms_to_sd();
    
    return 0;
}

// 删除闹钟函数（兼容旧接口）
static int alarm_service_del_alarm(AlarmInfo* alarm) {
    if (alarm == NULL) {
        printf("[AlarmService] del_alarm: invalid parameter\n");
        return -1;
    }
    
    int index = find_alarm_index(alarm);
    if (index < 0) {
        printf("[AlarmService] del_alarm: alarm not found\n");
        return -2;
    }
    
    return alarm_service_del_alarm_by_index(index);
}

// 检查并触发闹钟函数
static bool alarm_service_loop(int current_hour,int current_min,int current_weekday) {
    printf("[AlarmService] Checking %d alarms at %d-%d...\n", alarm_count, current_hour,current_min);
    if(current_hour>=24){
		current_hour = current_hour -24;
     }
    // 检查所有闹钟
    for (int i = 0; i < alarm_count; i++) {
        AlarmInfo* alarm = &alarm_list[i];
        
        printf("[AlarmService] Alarm %d: %02d:%02d, enabled=%d\n",
               i, alarm->m_hour, alarm->m_min, alarm->enabled);
        
        // 检查闹钟是否开启
        if (!alarm->enabled) {
            printf("[AlarmService] Alarm %d is disabled, skipping\n", i);
            continue;
        }
        
        // 检查时间是否匹配
        if (!is_time_match(alarm, current_hour, current_min)) {
            printf("[AlarmService] Alarm %d time mismatch\n", i);
            continue;
        }
        
        // 检查星期是否匹配
        //修正索引，current_weekday=0为周日
        current_weekday=current_weekday-1;
        if(current_weekday<0) current_weekday=6;
        if (!is_weekday_match(alarm,current_weekday)) {
            printf("[AlarmService] Alarm %d weekday mismatch\n", i);
            continue;
        }
             
        // 触发闹钟
        printf("[AlarmService] Triggering alarm %d at %d-%d weekday %d \n", i, alarm->m_hour,alarm->m_min,current_weekday);
        
        if (audio_service != NULL) {
	   audio_service->play_start();
            audio_service->play_ring(AUDIO_RING_ID_ALARM,1);
        } else {
            printf("[AlarmService] Audio service not available, cannot play alarm sound\n");
        }
        alarm_ringing = true;
        active_alarm_index = i;
        // 只触发一个闹钟
        break;
    }
    
    return alarm_ringing;
}

// 取消闹钟函数
static int alarm_service_cancel_alarm(void) {
    printf("[AlarmService] Cancelling alarm...\n");
    
    if (alarm_ringing) {
         audio_service->stop_ring();
	    alarm_ringing = false;
	    active_alarm_index = -1;	
        printf("[AlarmService] Alarm cancelled\n");
    } else {
        printf("[AlarmService] No alarm is ringing\n");
    }
    
    return 0;
}

static int get_actived_alarm(void){
	return active_alarm_index;
}

// 从SD卡加载闹钟
static int load_alarms_from_sd(void) {
    printf("[AlarmService] Loading alarms from SD card...\n");
    
    FRESULT res;
    FIL fp;
    
    // 打开文件进行读取
    res = f_open(&fp, _T("0:/alarms.dat"), FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) {
        if (res == FR_NO_FILE) {
            printf("[AlarmService] No alarms.dat file found, starting with empty alarm list\n");
        } else {
            printf("[AlarmService] Failed to open alarms.dat for reading, error=%d\n", res);
        }
        return -1;
    }
    
    // 读取闹钟数量
    UINT br;
    res = f_read(&fp, &alarm_count, sizeof(alarm_count), &br);
    if (res != FR_OK || br != sizeof(alarm_count)) {
        printf("[AlarmService] Failed to read alarm count, error=%d\n", res);
        f_close(&fp);
        return -2;
    }
    
    // 限制闹钟数量不超过最大值
    if (alarm_count > MAX_ALARMS) {
        alarm_count = MAX_ALARMS;
    }
    
    // 读取闹钟数据
    for (int i = 0; i < alarm_count; i++) {
        res = f_read(&fp, &alarm_list[i], sizeof(AlarmInfo), &br);
        if (res != FR_OK || br != sizeof(AlarmInfo)) {
            printf("[AlarmService] Failed to read alarm %d, error=%d\n", i, res);
            f_close(&fp);
            return -3;
        }
        
        printf("[AlarmService] Loaded alarm %d: %02d:%02d, enabled=%d\n",
               i, alarm_list[i].m_hour, alarm_list[i].m_min, alarm_list[i].enabled);
    }
    
    f_close(&fp);
    printf("[AlarmService] Loaded %d alarms from SD card\n", alarm_count);
    return 0;
}

// 保存闹钟到SD卡
static int save_alarms_to_sd(void) {
    printf("[AlarmService] Saving alarms to SD card...\n");
    
    FRESULT res;
    FIL fp;
    
    // 打开文件进行写入（覆盖模式）
    res = f_open(&fp, _T("0:/alarms.dat"), FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("[AlarmService] Failed to open alarms.dat for writing, error=%d\n", res);
        return -1;
    }
    
    // 写入闹钟数量
    UINT bw;
    res = f_write(&fp, &alarm_count, sizeof(alarm_count), &bw);
    if (res != FR_OK || bw != sizeof(alarm_count)) {
        printf("[AlarmService] Failed to write alarm count, error=%d\n", res);
        f_close(&fp);
        return -2;
    }
    
    // 写入闹钟数据
    for (int i = 0; i < alarm_count; i++) {
        res = f_write(&fp, &alarm_list[i], sizeof(AlarmInfo), &bw);
        if (res != FR_OK || bw != sizeof(AlarmInfo)) {
            printf("[AlarmService] Failed to write alarm %d, error=%d\n", i, res);
            f_close(&fp);
            return -3;
        }
    }
    
    f_close(&fp);
    printf("[AlarmService] Saved %d alarms to SD card\n", alarm_count);
    return 0;
}

// 比较两个闹钟信息是否相同
static int compare_alarm_info(const AlarmInfo* a, const AlarmInfo* b) {
    if (a->m_hour != b->m_hour || a->m_min != b->m_min) {
        return 1;
    }
    
    for (int i = 0; i < 7; i++) {
        if (a->weekdays[i] != b->weekdays[i]) {
            return 1;
        }
    }
    
    return 0;
}

// 查找闹钟索引
static int find_alarm_index(const AlarmInfo* alarm) {
    for (int i = 0; i < alarm_count; i++) {
        if (compare_alarm_info(&alarm_list[i], alarm) == 0) {
            return i;
        }
    }
    return -1;
}

// 检查时间是否匹配
static int is_time_match(const AlarmInfo* alarm, char hour, char min) {
    return (alarm->m_hour == hour && alarm->m_min == min);
}

// 检查星期是否匹配
static int is_weekday_match(const AlarmInfo* alarm,int current_weekday) {
    if (current_weekday < 0 || current_weekday >= 7) {
        return 0;
    }
    
    // 检查是否至少选中了一个星期
    bool any_weekday_selected = false;
    for (int i = 0; i < 7; i++) {
        if (alarm->weekdays[i]) {
            any_weekday_selected = true;
            break;
        }
    }
    
    // 如果没有选中任何星期，则每天都会响
    if (!any_weekday_selected) {
        printf("[AlarmService] No weekday selected, alarm will ring every day\n");
        return 1;
    }
    
    // 否则检查当前星期是否被选中
    bool matched = alarm->weekdays[current_weekday];
    printf("[AlarmService] Weekday %d selected: %s, match: %s\n", 
           current_weekday, alarm->weekdays[current_weekday] ? "true" : "false",
           matched ? "true" : "false");
    return matched ? 1 : 0;
}

// 获取闹钟列表函数
static int alarm_service_get_alarm_list(AlarmInfo* list, int max_count) {
    if (list == NULL) {
        printf("[AlarmService] get_alarm_list: invalid parameter\n");
        return -1;
    }
    
    int count_to_copy = (alarm_count < max_count) ? alarm_count : max_count;
    
    for (int i = 0; i < count_to_copy; i++) {
        memcpy(&list[i], &alarm_list[i], sizeof(AlarmInfo));
    }
    
    printf("[AlarmService] get_alarm_list: copied %d alarms (total=%d)\n", count_to_copy, alarm_count);
    return count_to_copy;
}

// 更新闹钟函数
static int alarm_service_update_alarm(int index, AlarmInfo* alarm) {
    if (alarm == NULL) {
        printf("[AlarmService] update_alarm: invalid parameter\n");
        return -1;
    }
    
    if (index < 0 || index >= alarm_count) {
        printf("[AlarmService] update_alarm: invalid index %d (alarm_count=%d)\n", index, alarm_count);
        return -2;
    }
    
    // 更新闹钟
    memcpy(&alarm_list[index], alarm, sizeof(AlarmInfo));
    
    printf("[AlarmService] update_alarm: updated alarm at index %d: %02d:%02d, enabled=%d\n",
           index, alarm->m_hour, alarm->m_min, alarm->enabled);
    
    // 保存到SD卡
    save_alarms_to_sd();
    
    return 0;
}

// 服务注册函数
static void alarm_service_entry(void) {
    printf("[AlarmService] Registering alarm service...\n");
    
    // 初始化服务接口
    alarm_service.init = alarm_service_init;
    alarm_service.add_alarm = alarm_service_add_alarm;
    alarm_service.del_alarm = alarm_service_del_alarm_by_index; // 使用按索引删除的函数
    alarm_service.task_loop = alarm_service_loop;
    alarm_service.cancel_alarm = alarm_service_cancel_alarm;
    alarm_service.get_alarm_list = alarm_service_get_alarm_list;
    alarm_service.update_alarm = alarm_service_update_alarm;
   alarm_service.get_actived_alarm = get_actived_alarm;
    
    // 注册服务
    register_service(ALARM_SERVICE_INDEX, (void*)&alarm_service);
    
    printf("[AlarmService] Service registered successfully\n");
}

// 初始化调用
GOLDIE_INIT_CALL_(alarm_service_entry);
