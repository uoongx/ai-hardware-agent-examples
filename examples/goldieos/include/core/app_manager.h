#ifndef INCLUDE_APP_MANAGER_
#define INCLUDE_APP_MANAGER_

#include <string.h>
#include <stdint.h>
#include "input_service.h"

#define MAX_APP_STACK  4

typedef enum APP_TATUS_{
	APP_STATUS_NONE,
	APP_STATUS_RUNNING,
	APP_STATUS_SUSPENDED,
	APP_STATUS_EXITED,
};
// 定义APP结构体
typedef struct App_t{
    char app_name[32];          // 应用名称
    void (*h_app_run)(void);      // 运行函数
    void (*h_app_exit)(void);     // 退出函数
    void (*h_app_suspend)(void);  // 挂起函数
    void (*h_app_resume)(void);   // 恢复函数
    void (*h_touch_event)(int,int,int);
    void (*h_keyboard_event)(int,int);
    uint16_t* icon;             // 图标指针
    int app_status;
} App_t;

// 最大支持的APP数量
#define MAX_APPS  12

// APP管理器结构
typedef struct {
    App_t* apps[MAX_APPS];       // APP数组
    int app_count;          // 当前APP数量
} AppManager_t;



int goldie_run_app(App_t* app);
int goldie_exit_app(App_t* app);
int goldie_suspend_app(App_t* app);
int goldie_resume_app(App_t* app);

void report_touch(int pressure,int x,int y);
void report_keyvalue(int pressure,int value);

void start_launcher_app(void);

int install_app(App_t* app);
App_t* get_app_by_name(const char* name);
App_t** get_app_list(int* count);
int get_app_count(void);
App_t* wait_app(const char* app_name);
#endif
