#ifndef VIRTUAL_DISPLAY_H
#define VIRTUAL_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef VIRTUAL_DISPLAY_EXPORTS
#define VIRTUAL_DISPLAY_API __declspec(dllexport)
#else
#define VIRTUAL_DISPLAY_API __declspec(dllimport)
#endif

#include <stdint.h>
#include <windows.h>
#include <windowsx.h>

// 初始化虚拟显示屏
VIRTUAL_DISPLAY_API int virtual_display_init(int width, int height, const char* title);

// 关闭虚拟显示屏
VIRTUAL_DISPLAY_API void virtual_display_close(void);

// 锁定绘制区域
VIRTUAL_DISPLAY_API int virtual_display_lock_region(int start_x, int start_y, int width, int height);

// 填充RGB16像素数据
VIRTUAL_DISPLAY_API int virtual_display_fill_pixels(uint16_t* buffer, uint32_t count);

// 更新显示区域
VIRTUAL_DISPLAY_API int virtual_display_update_region(void);

// 处理窗口消息（需要在主循环中调用）
VIRTUAL_DISPLAY_API int virtual_display_process_messages(void);

// 检查显示窗口是否仍然存在
VIRTUAL_DISPLAY_API int virtual_display_is_alive(void);

#ifdef __cplusplus
}
#endif

#endif // VIRTUAL_DISPLAY_H
