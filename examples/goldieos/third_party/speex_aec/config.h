/* config.h */
#ifndef CONFIG_H
#define CONFIG_H
//#define DUMP_ECHO_CANCEL_DATA
//#define DUMP_FILE_USE_FATFS
/* 强制使用定点运算（无硬件FPU的单片机必须） */
#define FIXED_POINT
/* 使用内置 Kiss FFT */
#define USE_KISS_FFT

/* 告知 SpeexDSP 我们有 config.h */
#define HAVE_CONFIG_H

/* 导出 API 符号（静态库不需要，但保留宏） */
#define EXPORT

/* 可选：自定义 OS 支持（如果需要替换 malloc/free） */
// #define OS_SUPPORT_CUSTOM

#endif