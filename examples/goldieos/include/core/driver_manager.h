#ifndef INCLUDE_DRIVER_MANAGER_
#define INCLUDE_DRIVER_MANAGER_

#ifndef PLATFORM_TYPE_WIN_CREATOR
#include "diskio.h"
#include "media_driver.h"
#endif
#ifdef PLATFORM_TYPE_WS63
#include "sle_drv.h"
#endif
#include <time.h>

#define I2C_BUS_BAUDRATE  400000  //400000

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef void (*interrupt_call_back)(void);

enum DRIVER_INDEX{
	I2C_DRV_INDEX =0,
	DISP_DRV_INDEX,
	FATFS_DISK_DRV_INDEX,
	I2S_DRV_INDEX,
	WLAN_DRV_INDEX,
	TOUCH_DRV_INDEX,
	KEYBOARD_DRV_INDEX,
	SPI1_DRV_INDEX,
	GPIO_EXT_DRV_INDEX,
	HW_TOUCH_DRV_INDEX,
	HW_KEYBOARD_DRV_INDEX,
	BATTERY_DRV_INDEX,
	SLE_DRV_INDEX,
	RTC_DRV_INDEX,
	PA_DRV_INDEX,
	FLASH_DRV_INDEX,
	RESERVE_DRV_INDEX_3,
	RESERVE_DRV_INDEX_2,
	RESERVE_DRV_INDEX_1,
	RESERVE_DRV_INDEX_0,	
	MAX_DRV_INDEX,
};

typedef struct GpioDrvExt{
	int (*drv_init)(void);
	int (*set_gpio_func)(int,int);
	int (*set_gpio_value)(int,int);
	int (*get_gpio_value)(int);
}GpioDrvExt;

typedef struct PA_Driver{
	int (*drv_init)(void);
	int (*set_mute)(int);
}PA_Driver;

typedef struct Rtc_Driver{
	int (*drv_init)(void);
	int (*set_time)(const struct tm *);
	int (*read_time)(struct tm *);

}Rtc_Driver;

typedef struct Flash_Drv {
    int (*flash_read)(unsigned int,char *,unsigned int);
    int (*flash_write)(unsigned int,char *,unsigned int);
    int (*flash_erase)(unsigned int,unsigned int);
} Flash_Drv;

typedef struct Touch_Data{
	int pressure;
	uint16_t x_pos;
	uint16_t y_pos;
}Touch_Data;

#define MAX_AP_NUM 30
#define WIFI_MAX_KEY_LEN                65
#define WIFI_MAX_SSID_LEN               33

typedef struct ApInfo_t
{
    char ssid[WIFI_MAX_SSID_LEN];
    char passwd[WIFI_MAX_KEY_LEN];
    int rssi;
}ApInfo_t;


typedef struct Wlan_Drv{
	int (*sta_enable)(void);
	int (*sta_disable)(void);
	int (*sta_scan)(void);
	int (*sta_is_scan_done)(void);
	int (*sta_get_ap_list)(ApInfo_t*);
	int (*sta_connect)(ApInfo_t*,int); //int sta_connect(ApInfo_t* dst_ap,int timeout_ms)
	int (*sta_isConneted)(void);
	int (*sta_isEnabled)(void);
	int (*softap_config)(char*,char*);//void softap_config(char* ap_name,char* passwd)
	int (*softap_enable)(void);
	int (*softap_disable)(void);
	int (*softap_isEnabled)(void);
}Wlan_Drv;


typedef struct Keyboard_Data{
	int pressure;
	uint16_t keyvalue;
}Keyboard_Data;

typedef struct Touch_Drv{
	void (*touch_init)(void);
	void (*obtain_coordinates)(Touch_Data*);
}Touch_Drv;


typedef struct Keyboard_Drv{
	void (*keyboard_init)(void);
	void (*obtain_keyvalue)(Keyboard_Data*);
}Keyboard_Drv;


typedef struct DispDrv {
    void  (*lock_region)(unsigned int,unsigned int,unsigned int,unsigned int);
    void  (*update_region)(void);
	void (*fill_pixels)(uint16_t*, uint32_t);
	void (*set_backlight)(int);

	int (*get_framebuffer)(char**);
	void (*init_framebuffer)(char*,int,int);
} DispDrv;

typedef struct I2cDrv {
    int (*i2c_init)(int);
    int (*i2c_reg_read)(char,char,char *);
    int (*i2c_reg_write)(char,char,char);
    int (*i2c_reg_read_multi)(char,char,char*,int);
    int (*i2c_reg_write_multi)(char,char,char*,int);

} I2cDrv;

typedef struct BatDrv {
    int (*init)(void);
    void (*calc_soc)(void);
    int (*read_power)(void);
    int (*is_charging)(void);
} BatDrv;


typedef struct SpiDrv {
    int (*spi_init)(void);	
    int (*spi_read)(unsigned char *,int);
    int (*spi_write)(unsigned char*,int);
} SpiDrv;

typedef struct I2S_Drv {
    int (*device_init)(void);
	void (*device_enable)(int);
	void (*regist_interrupt)(interrupt_call_back);
    int (*read_write_data)(void*,void*,unsigned int);
} I2S_Drv;

typedef struct Wifi_Drv{
    int (*device_init)(void);
	void (*device_enable)(int);
	void (*isConnected)(void);
	void (*wifi_account_cfg)(char*,char*);
}Wifi_Drv;

void register_drv(int drv_index, void* drv_ops);
void* get_drv(int drv_index);
void* wait_drv(int drv_index);
void* wait_drv_timeout_ms(int drv_index,int m_timeout);
#endif
