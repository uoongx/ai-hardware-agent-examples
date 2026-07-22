#ifndef CONVAI_PLATFORM_WS63_H
#define CONVAI_PLATFORM_WS63_H

#include "convai/convai_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

int convai_platform_ws63_init(void);

uint64_t ws63_get_time_ms(void);

/**
 * Get device id (WiFi MAC or fallback).
 * Returns string length on success, -1 on failure.
 */
int convai_platform_ws63_device_id(char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_PLATFORM_WS63_H */