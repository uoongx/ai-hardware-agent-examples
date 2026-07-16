#ifdef SUPPORT_GPIO_KEYBOARD
#include "goldie_osal.h"
#include "service_manager.h"
#include "driver_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif

#define HOME_KEY_IO           14
#define WAKEUP_KEY_IO           3
static int last_key_status = 0;
static void gpio_key_init(void);
static void gpio_key_obtain_value(Keyboard_Data* kdata);
static Keyboard_Drv gpio_key = {
    .keyboard_init = gpio_key_init,
    .obtain_keyvalue = gpio_key_obtain_value,
};


static void gpio_key_init(void)
{
    #ifndef GC9D01_SPI_LCD
	goldie_gpio_setfunc(HOME_KEY_IO,0);
    goldie_gpio_setdir(HOME_KEY_IO,GOLDIE_GPIO_DIRECTION_INPUT);
    #endif
	goldie_gpio_setfunc(WAKEUP_KEY_IO,0);
    goldie_gpio_setdir(WAKEUP_KEY_IO,GOLDIE_GPIO_DIRECTION_INPUT);
    
    return;
}

static void gpio_key_obtain_value(Keyboard_Data* kdata)
{	
    #ifndef GC9D01_SPI_LCD
    int home_key_status = goldie_gpio_getval(HOME_KEY_IO);
    #endif
    int wakeup_key_status = goldie_gpio_getval(WAKEUP_KEY_IO);
    
    static int last_home_key_status = 0;
    static int last_wakeup_key_status = 0;
    #ifndef GC9D01_SPI_LCD
    // 检查HOME键状态变化
    if (last_home_key_status != home_key_status) {
        printf("HOME key_status is %d \r\n", home_key_status);
        kdata->pressure = home_key_status;
        kdata->keyvalue = 300; // SYSTEM_KEY_VALUE_BACK
        last_home_key_status = home_key_status;
        return;
    }
    #endif
    
    // 检查WAKEUP键状态变化
    if (last_wakeup_key_status != wakeup_key_status) {
        printf("WAKEUP key_status is %d \r\n", wakeup_key_status);
        kdata->pressure = wakeup_key_status;
        kdata->keyvalue = 301; // SYSTEM_KEY_VALUE_WAKEUP
        last_wakeup_key_status = wakeup_key_status;
        return;
    }
    
    
    // 没有按键状态变化
    kdata->pressure = -1;
    kdata->keyvalue = 0;
}

#ifdef DRV_CORE

static int gpio_key_open_cnt = 0;

static int gpio_key_open(file_t *filp)
{
    if (gpio_key_open_cnt > 0) {
        printf("GPIO KEY ReOpen(%d)\r\n", gpio_key_open_cnt);
        return 0;
    }

	gpio_key_init();

    gpio_key_open_cnt++;
    printf("GPIO KEY Open Success\r\n");
    return 0;
}

static int gpio_key_read(file_t *filp, uint8_t *buf, uint16_t len) {
    Keyboard_Data * out_data = (Keyboard_Data *)buf;
    if (buf == NULL || len < sizeof(Keyboard_Data)) {
        printf("EXT GPIO Key Read Error: invalid buffer (len=%d < 3)\r\n", len);
        return -1;
    }

    gpio_key_obtain_value(out_data);

    //printf("GPIO Key Read: pressure=0x%x, keyvalue=0x%x, buf=[0x%02X,0x%02X,0x%02X]\r\n",
           //kdata.pressure, kdata.keyvalue,
           //buf[0], buf[1], buf[2]);

    return sizeof(Keyboard_Data);
}

static int gpio_key_close(file_t *filp)
{
    if (gpio_key_open_cnt == 0) {
		printf("GPIO KEY Close Success\r\n");
        return 0;
    }

    gpio_key_open_cnt--;
    printf("GPIO KEY Close(%d)\r\n", gpio_key_open_cnt);
    return 0;
}

static drv_ops_t gpio_key_drv_ops = {
    .name = GPIO_KEY_DRV_NAME,
    .open = gpio_key_open,
	.read = gpio_key_read,
    .close = gpio_key_close,
};

#endif
void gpio_keyboard_init()
{
	register_drv(HW_KEYBOARD_DRV_INDEX, (void*)(&gpio_key));
	#ifdef DRV_CORE
	int ret = goldie_driver_register(&gpio_key_drv_ops);
    if (ret == 0) {
        printf("GPIO KEY Driver Register Success: '%s'\r\n", GPIO_KEY_DRV_NAME);
    } else {
        printf("GPIO KEY Driver Register Error: ret=%d\r\n", ret);
    }
	#endif
}
#endif
