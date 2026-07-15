#ifndef INCLUDE_GOLDIE_OSAL_H
#define INCLUDE_GOLDIE_OSAL_H
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

#ifdef PLATFORM_TYPE_WS63
typedef unsigned int size_t;
#include "app_init.h"
#include "sfcop.h"
#include "adc.h"
#define GOLDIE_INIT_CALL_(funx)  app_run(funx)
#else
#ifdef PLATFORM_TYPE_WIN
#include "goldie_os_init.h"
#endif
#endif

#define true 1
#define false 0
#define unused(x)  (x=x)

typedef enum goldie_gpio_direction {
     GOLDIE_GPIO_DIRECTION_INPUT,
     GOLDIE_GPIO_DIRECTION_OUTPUT
} goldie_gpio_direction;


typedef int (*goldie_thread_handler)(void *data);

typedef struct {
    void *sem;
} goldie_sem;

typedef struct {
    void *mutex;
} goldie_mutex;

typedef struct {
    long tv_sec;
    long tv_usec;
} goldie_timeval;

	
void goldie_msleep(int ms);
void *goldie_thread_create(goldie_thread_handler handler, void *data, const char *name, unsigned int stack_size);
int  goldie_thread_set_priority(void *goldie_thread, unsigned int priority);
void goldie_thread_destroy(void* goldie_thread);
void goldie_thread_lock(void);
void goldie_thread_unlock(void);

void goldie_mutex_init(goldie_mutex *mutex);
void goldie_mutex_lock(goldie_mutex *mutex);
void goldie_mutex_unlock(goldie_mutex *mutex);
void goldie_mutex_destroy(goldie_mutex *mutex);

void goldie_sem_init(goldie_sem *sem);
void goldie_sem_wait(goldie_sem *sem);
void goldie_sem_post(goldie_sem *sem);
void goldie_sem_destroy(goldie_sem *sem);

void goldie_gettimeofday(goldie_timeval *tv);

void * goldie_malloc(unsigned long size);

void goldie_free(void* addr);

#ifdef PLATFORM_TYPE_WS63
int goldie_gpio_setfunc(unsigned char gpio_id,unsigned char func);
int goldie_gpio_setdir(unsigned char gpio_id,unsigned char in_out);
int goldie_gpio_setval(unsigned char gpio_id,unsigned char value);
int goldie_gpio_getval(unsigned char gpio_id);

uint32_t goldie_adc_init(void);
uint32_t goldie_deinit(void);
uint32_t goldie_adc_read(uint8_t channel);
#endif

#endif