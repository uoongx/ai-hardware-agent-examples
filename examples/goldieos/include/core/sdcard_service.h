#ifndef SDCARD_SERVICE_H
#define SDCARD_SERVICE_H
typedef struct SdCardService {
    int (*IsSdCardExists)(void);
	int (*mount_disk)(void);
	int (*unmount_disk)(void);
} SdCardService;

#endif