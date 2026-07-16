/*-----------------------------------------------------------------------*/
/* 该文件实现了 FatFs 所需的底层磁盘 I/O 函数，并调用 media_driver 层   */
/* 所有函数原型必须与 FatFs 的期望完全一致                              */
/*-----------------------------------------------------------------------*/

#include "media_driver.h"

/* 外部全局管理器已在 media_driver.c 中定义，此处直接引用 */
extern MediaManager g_media_manager;

/*-----------------------------------------------------------------------*/
/* 获取驱动器状态                                                        */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(BYTE pdrv) {
    DSTATUS stat = STA_NOINIT;

    if (pdrv < _VOLUMES && g_media_manager.ops[pdrv] != 0) {
        if (g_media_manager.ops[pdrv]->status) {
            stat = g_media_manager.ops[pdrv]->status(g_media_manager.lun[pdrv]);
        }
    }
    return stat;
}

/*-----------------------------------------------------------------------*/
/* 初始化驱动器                                                          */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(BYTE pdrv) {
    DSTATUS stat = STA_NOINIT;

    if (pdrv >= _VOLUMES || g_media_manager.ops[pdrv] == 0) {
        return stat;
    }

    /* 如果尚未初始化，则调用底层初始化函数 */
    if (g_media_manager.initialized[pdrv] == 0) {
        if (g_media_manager.ops[pdrv]->init) {
            stat = g_media_manager.ops[pdrv]->init(g_media_manager.lun[pdrv]);
        }
        if (stat == RES_OK) {
            g_media_manager.initialized[pdrv] = 1;
        }
    } else {
        /* 已经初始化过，返回 OK（这里可根据需要调用 status 确认） */
        stat = RES_OK;
    }
    return stat;
}

/*-----------------------------------------------------------------------*/
/* 读扇区                                                                */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    DRESULT res = RES_PARERR;

    if (pdrv < _VOLUMES && g_media_manager.ops[pdrv] != 0) {
        if (g_media_manager.ops[pdrv]->read) {
            res = g_media_manager.ops[pdrv]->read(g_media_manager.lun[pdrv],
                                                   buff, sector, count);
        }
    }
    return res;
}

/*-----------------------------------------------------------------------*/
/* 写扇区 (如果启用)                                                     */
/*-----------------------------------------------------------------------*/
#if _USE_WRITE == 1
DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    DRESULT res = RES_PARERR;

    if (pdrv < _VOLUMES && g_media_manager.ops[pdrv] != 0) {
        if (g_media_manager.ops[pdrv]->write) {
            res = g_media_manager.ops[pdrv]->write(g_media_manager.lun[pdrv],
                                                    buff, sector, count);
        }
    }
    return res;
}
#endif

/*-----------------------------------------------------------------------*/
/* 其他控制命令 (如果启用)                                               */
/*-----------------------------------------------------------------------*/
#if _USE_IOCTL == 1
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    DRESULT res = RES_PARERR;

    if (pdrv < _VOLUMES && g_media_manager.ops[pdrv] != 0) {
        if (g_media_manager.ops[pdrv]->ioctl) {
            res = g_media_manager.ops[pdrv]->ioctl(g_media_manager.lun[pdrv],
                                                    cmd, buff);
        }
    }
    return res;
}
#endif
