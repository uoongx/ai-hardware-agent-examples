#include "media_driver.h"

/* 全局介质管理器实例，并初始化为零 */
MediaManager g_media_manager = {{0}, {0}, {0}, 0};

/*-----------------------------------------------------------------------*/
/* 内部辅助函数：根据路径字符串解析驱动器索引                            */
/*-----------------------------------------------------------------------*/
static BYTE get_drive_index_from_path(const char* path) {
    if (path && path[0] >= '0' && path[0] <= '9') {
        return path[0] - '0';
    }
    return 0xFF; /* 无效索引 */
}

/*-----------------------------------------------------------------------*/
/* 公开函数实现                                                          */
/*-----------------------------------------------------------------------*/

BYTE media_attach_driver_ex(const MediaDriverOps* ops, char* path, BYTE lun) {
    BYTE idx;

    if (g_media_manager.count >= _VOLUMES) {
        return 1; /* 已达最大支持数量 */
    }

    idx = g_media_manager.count; /* 新驱动器将获得当前计数作为索引 */

    g_media_manager.initialized[idx] = 0;
    g_media_manager.ops[idx]        = ops;
    g_media_manager.lun[idx]        = lun;
    g_media_manager.count++;

    /* 生成路径字符串，例如 "0:/" */
    if (path) {
        path[0] = idx + '0';
        path[1] = ':';
        path[2] = '/';
        path[3] = '\0';
    }

    return 0;
}

BYTE media_attach_driver(const MediaDriverOps* ops, char* path) {
    return media_attach_driver_ex(ops, path, 0);
}

BYTE media_detach_driver_ex(char* path, BYTE lun) {
    BYTE idx = get_drive_index_from_path(path);
    BYTE i;

    /* 忽略 lun 参数（简化实现，通常仅用于匹配，这里仅检查 idx 有效性） */
    (void)lun;

    if (idx >= _VOLUMES || g_media_manager.ops[idx] == 0) {
        return 1; /* 无效索引或未注册 */
    }

    /* 清除该驱动器的信息 */
    g_media_manager.initialized[idx] = 0;
    g_media_manager.ops[idx]        = 0;
    g_media_manager.lun[idx]        = 0;

    /* 减少总计数（注意：这里简单地将 count 减 1，但不紧凑数组，这可能导致 count 与实际已注册数不一致。
       若需要精确计数，可扫描所有槽位重新计算。此处为简化，保持与 ST 原版一致：仅将 count 减 1，
       但 ST 原版实际上并未紧凑数组，可能导致 count 小于实际占用的最大索引。我们这里也采用同样简单逻辑。 */
    if (g_media_manager.count > 0) {
        g_media_manager.count--;
    }

    return 0;
}

BYTE media_detach_driver(char* path) {
    return media_detach_driver_ex(path, 0);
}

BYTE media_get_attached_count(void) {
    return g_media_manager.count;
}