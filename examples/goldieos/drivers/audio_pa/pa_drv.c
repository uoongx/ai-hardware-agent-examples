#include <stdint.h>
#include "goldie_osal.h"
#include "driver_manager.h"
#ifdef SUPPORT_EXT_GPIO_PA
#define PA_CTL_EXT_PIN  9
static GpioDrvExt * gpio_ext_drv = NULL;
#endif

#ifdef SUPPORT_EXT_GPIO_PA
static void pa_init(){
         printf("pa_init \r\n");
	gpio_ext_drv = wait_drv(GPIO_EXT_DRV_INDEX);
	gpio_ext_drv->drv_init();
	gpio_ext_drv->set_gpio_func(PA_CTL_EXT_PIN,GOLDIE_GPIO_DIRECTION_OUTPUT);
	gpio_ext_drv->set_gpio_value(PA_CTL_EXT_PIN,0);
}
static void set_pa_mute(int mute){
	if(gpio_ext_drv){
		int gpio_status = mute?0:1;		
		printf("set_pa_mute %d gpio_status %d \r\n",mute,gpio_status);		
		gpio_ext_drv->set_gpio_func(PA_CTL_EXT_PIN,GOLDIE_GPIO_DIRECTION_OUTPUT);
		gpio_ext_drv->set_gpio_value(PA_CTL_EXT_PIN,gpio_status);
	}else{		
		printf("gpio_ext_drv is null \r\n");
	}
}
#else
static void pa_init(){
	return 0;
}

static void set_pa_mute(int mute){
	return 0;
}
#endif

// 驱动接口定义
static PA_Driver pa_drv = {
    .drv_init = pa_init,
    .set_mute = set_pa_mute,
};

void pa_drv_entry(void)
{
    register_drv(PA_DRV_INDEX, &pa_drv);
    printf("Register pa driver at index %d\n", PA_DRV_INDEX);
}

GOLDIE_INIT_CALL_(pa_drv_entry);

