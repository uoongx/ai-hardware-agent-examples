/**
 * @file goldie_osal.h
 * @brief GoldieOS OS abstraction layer compatibility header.
 *
 * Provides unified interface for both WS63 (embedded) and Win10 (desktop)
 * platforms. Platform-specific implementations are selected via
 * PLATFORM_TYPE_WS63 / PLATFORM_TYPE_WIN preprocessor macros.
 */

#ifndef GOLDIE_OSAL_H
#define GOLDIE_OSAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 *  Platform detection (must define one in CMakeLists.txt)
 * =================================================================== */
#if !defined(PLATFORM_TYPE_WS63) && !defined(PLATFORM_TYPE_WIN)
  #error "Must define either PLATFORM_TYPE_WS63 or PLATFORM_TYPE_WIN"
#endif

/* ===================================================================
 *  Common type definitions (platform-independent)
 * =================================================================== */
typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;

typedef int (*goldie_thread_handler)(void *data);

typedef struct { void *sem;   } goldie_sem;
typedef struct { void *mutex; } goldie_mutex;

typedef struct {
    long tv_sec;
    long tv_usec;
} goldie_timeval;

typedef enum goldie_gpio_direction {
    GOLDIE_GPIO_DIRECTION_INPUT,
    GOLDIE_GPIO_DIRECTION_OUTPUT
} goldie_gpio_direction;

/* ===================================================================
 *  Legacy macros
 * =================================================================== */
#define true   1
#define false  0
#define unused(x)  (x = x)

/* ===================================================================
 *  OSAL function declarations
 * =================================================================== */

/* Thread */
void *goldie_thread_create(goldie_thread_handler h, void *data,
                           const char *name, unsigned int stack_size);
int   goldie_thread_set_priority(void *thread, unsigned int priority);
void  goldie_thread_destroy(void *thread);
void  goldie_thread_lock(void);
void  goldie_thread_unlock(void);

/* Mutex */
void goldie_mutex_init(goldie_mutex *m);
void goldie_mutex_lock(goldie_mutex *m);
void goldie_mutex_unlock(goldie_mutex *m);
void goldie_mutex_destroy(goldie_mutex *m);

/* Semaphore */
void goldie_sem_init(goldie_sem *s);
void goldie_sem_wait(goldie_sem *s);
void goldie_sem_post(goldie_sem *s);
void goldie_sem_destroy(goldie_sem *s);

/* Time */
void goldie_gettimeofday(goldie_timeval *tv);

/* Sleep */
void goldie_msleep(int ms);

/* Memory */
void *goldie_malloc(unsigned long size);
void  goldie_free(void *addr);

/* ===================================================================
 *  Platform-specific initialization dispatch
 * =================================================================== */
#if defined(PLATFORM_TYPE_WS63)

  /* WS63: LiteOS app_run() */
  #include "app_init.h"
  #include "sfcop.h"
  #include "adc.h"

  #define GOLDIE_INIT_CALL_(funx)  app_run(funx)

  /* WS63-specific GPIO */
  int goldie_gpio_setfunc(unsigned char id, unsigned char func);
  int goldie_gpio_setdir(unsigned char id, unsigned char in_out);
  int goldie_gpio_setval(unsigned char id, unsigned char value);
  int goldie_gpio_getval(unsigned char id);

  /* WS63-specific ADC */
  uint32_t goldie_adc_init(void);
  uint32_t goldie_deinit(void);
  uint32_t goldie_adc_read(uint8_t channel);

#elif defined(PLATFORM_TYPE_WIN)

  /* ===================================================================
   *  Driver I/O (abstracted for portability)
   * =================================================================== */
  void *goldie_open(const char *name, int flags);
  int   goldie_ioctl(int fd, int cmd, void *arg);
  int   goldie_read(int fd, void *buf, int len);
  int   goldie_write(int fd, const void *buf, int len);
  int   goldie_close(int fd);

  /* Win10: constructor-based auto-registration (in libwinvm.a) */
  void register_init_function(void (*func)(void), const char *name);
  void call_all_init_functions(void);

  #if defined(__GNUC__) || defined(__clang__)
    #define GOLDIE_INIT_CALL_(func) \
        static void _goldie_init_##func(void) __attribute__((constructor)); \
        static void _goldie_init_##func(void) { \
            register_init_function(func, #func); \
        }
    __attribute__((weak)) void call_all_init_functions(void) {}
  #elif defined(_MSC_VER)
    #pragma section(".CRT$XCU", read)
    #define GOLDIE_INIT_CALL_(func) \
        static void _goldie_init_##func(void); \
        __declspec(allocate(".CRT$XCU")) \
        void (*_goldie_init_ptr_##func)(void) = _goldie_init_##func; \
        static void _goldie_init_##func(void) { \
            register_init_function(func, #func); \
        }
  #else
    #error "GOLDIE_INIT_CALL_: unsupported compiler"
  #endif

#else
  #error "Unknown platform type"
#endif

#ifdef __cplusplus
}
#endif

#endif /* GOLDIE_OSAL_H */