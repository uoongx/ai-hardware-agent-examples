#ifndef MEDIA_DRIVER_H
#define MEDIA_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "diskio.h"   /* FatFs 底层接口定义 */
#include "ff.h"       /* FatFs 核心定义，含 _VOLUMES 等配置 */

/* 定义基本整数类型（如果环境未提供，可自行定义） */
#ifndef BYTE
typedef unsigned char BYTE;
#endif
#ifndef DWORD
typedef unsigned long DWORD;
#endif

/*-----------------------------------------------------------------------*/
/* 底层介质驱动操作集（每个物理驱动器需提供这些函数）                    */
/* 对应原 Diskio_drvTypeDef                                             */
/*-----------------------------------------------------------------------*/
typedef struct {
    DSTATUS (*init)(BYTE lun);                /* 初始化，lun 为子单元号 */
    DSTATUS (*status)(BYTE lun);               /* 获取状态 */
    DRESULT (*read)(BYTE lun, BYTE* buff, DWORD sector, UINT count);   /* 读扇区 */
#if _USE_WRITE == 1
    DRESULT (*write)(BYTE lun, const BYTE* buff, DWORD sector, UINT count); /* 写扇区 */
#endif
#if _USE_IOCTL == 1
    DRESULT (*ioctl)(BYTE lun, BYTE cmd, void* buff);                  /* 控制命令 */
#endif
} MediaDriverOps;

/*-----------------------------------------------------------------------*/
/* 全局介质管理器，管理所有已注册的驱动器                                */
/* 对应原 Disk_drvTypeDef                                               */
/*-----------------------------------------------------------------------*/
typedef struct {
    BYTE initialized[_VOLUMES];          /* 每个驱动器的初始化标志 */
    const MediaDriverOps* ops[_VOLUMES];  /* 每个驱动器的操作函数集 */
    BYTE lun[_VOLUMES];                   /* 每个驱动器的子单元号（如 USB 多 LUN） */
    BYTE count;                            /* 当前已注册的驱动器数量 */
} MediaManager;

/* 全局管理器变量（在 media_driver.c 中定义） */
extern MediaManager g_media_manager;

/*-----------------------------------------------------------------------*/
/* 公开的辅助函数：用于动态注册/注销驱动器                              */
/* 对应原 FATFS_LinkDriverEx / FATFS_UnLinkDriverEx 等                  */
/*-----------------------------------------------------------------------*/

/**
 * @brief  将介质驱动注册到系统中，并自动分配逻辑驱动器号
 * @param  ops   指向 MediaDriverOps 的指针，包含底层函数
 * @param  path  输出参数：用于接收生成的逻辑驱动器路径（如 "0:/"）
 * @param  lun   子单元号（通常为 0，多 LUN 设备时使用）
 * @return 成功返回 0，失败返回 1
 */
BYTE media_attach_driver_ex(const MediaDriverOps* ops, char* path, BYTE lun);

/**
 * @brief  简化版注册（lun 固定为 0）
 */
BYTE media_attach_driver(const MediaDriverOps* ops, char* path);

/**
 * @brief  从系统中注销指定驱动器
 * @param  path  逻辑驱动器路径（如 "0:/"）
 * @param  lun   子单元号（仅用于匹配，通常传递 0）
 * @return 成功返回 0，失败返回 1
 */
BYTE media_detach_driver_ex(char* path, BYTE lun);

/**
 * @brief  简化版注销（lun 固定为 0）
 */
BYTE media_detach_driver(char* path);

/**
 * @brief  获取当前已注册的驱动器数量
 */
BYTE media_get_attached_count(void);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_DRIVER_H */