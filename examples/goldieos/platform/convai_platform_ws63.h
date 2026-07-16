#ifndef CONVAI_PLATFORM_WS63_H
#define CONVAI_PLATFORM_WS63_H

#include "convai/convai_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

int convai_platform_ws63_init(void);

uint64_t ws63_get_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_PLATFORM_WS63_H */