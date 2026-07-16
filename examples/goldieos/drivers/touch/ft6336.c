#include <stdint.h>
#include "driver_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif
static I2cDrv * i2c_drv;

static int init_flag  = 0;
static int last_touch_count = 0;
#define HW_TOUCH_HORIZONTAL
#define HW_TOUCH_WIDTH 240
#define HW_TOUCH_HEIGHT 320

// FT6336 设备地址（7位地址）
#define FT6336_I2C_ADDR         0x38

// FT6336 寄存器地址
#define FT6336_REG_DEVICE_MODE  0x00
#define FT6336_REG_GEST_ID      0x01
#define FT6336_REG_TD_STATUS    0x02
#define FT6336_REG_P1_XH        0x03
#define FT6336_REG_P1_XL        0x04
#define FT6336_REG_P1_YH        0x05
#define FT6336_REG_P1_YL        0x06
#define FT6336_REG_P1_WEIGHT    0x07
#define FT6336_REG_P1_MISC      0x08
#define FT6336_REG_P2_XH        0x09
#define FT6336_REG_P2_XL        0x0A
#define FT6336_REG_P2_YH        0x0B
#define FT6336_REG_P2_YL        0x0C

// 屏幕分辨率（需要根据实际屏幕设置）
#define FT6336_SCREEN_WIDTH     800
#define FT6336_SCREEN_HEIGHT    480

// 触摸点数据结构
typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t touch_count;
    uint8_t gesture_id;
} touch_point_t;

/**
 * @brief 读取FT6336寄存器值
 * @param reg_addr 寄存器地址
 * @return 读取到的寄存器值，-1表示读取失败
 */
static int ft6336_read_reg(uint8_t reg_addr)
{
    char read_data = 0;
    int ret = i2c_drv->i2c_reg_read(FT6336_I2C_ADDR, reg_addr, &read_data);
    if (ret != 0) {
        return -1;
    }
    return (int)read_data;
}

/**
 * @brief 写入FT6336寄存器值
 * @param reg_addr 寄存器地址
 * @param data 要写入的数据
 * @return 0成功，-1失败
 */
static int ft6336_write_reg(uint8_t reg_addr, uint8_t data)
{
    return i2c_drv->i2c_reg_write(FT6336_I2C_ADDR, reg_addr, data);
}

/**
 * @brief 获取触摸点数量
 * @return 触摸点数量，-1表示读取失败
 */
static int ft6336_get_touch_count(void)
{
    int td_status = ft6336_read_reg(FT6336_REG_TD_STATUS);
    if (td_status < 0) {
        return -1;
    }
    return td_status & 0x0F;  // 低4位表示触摸点数量
}

/**
 * @brief 获取手势ID
 * @return 手势ID，-1表示读取失败
 */
static int ft6336_get_gesture_id(void)
{
    return ft6336_read_reg(FT6336_REG_GEST_ID);
}

/**
 * @brief 读取单个触摸点的坐标
 * @param point_num 触摸点编号（1或2）
 * @param x 返回的X坐标
 * @param y 返回的Y坐标
 * @return 0成功，-1失败
 */
static int ft6336_read_touch_point(uint8_t point_num, uint16_t *x, uint16_t *y)
{
    uint8_t xh_reg, xl_reg, yh_reg, yl_reg;
    
    // 根据触摸点编号选择寄存器
    if (point_num == 1) {
        xh_reg = FT6336_REG_P1_XH;
        xl_reg = FT6336_REG_P1_XL;
        yh_reg = FT6336_REG_P1_YH;
        yl_reg = FT6336_REG_P1_YL;
    } else if (point_num == 2) {
        xh_reg = FT6336_REG_P2_XH;
        xl_reg = FT6336_REG_P2_XL;
        yh_reg = FT6336_REG_P2_YH;
        yl_reg = FT6336_REG_P2_YL;
    } else {
        return -1;
    }
    
    // 读取X坐标
    int xh = ft6336_read_reg(xh_reg);
    int xl = ft6336_read_reg(xl_reg);
    if (xh < 0 || xl < 0) {
        return -1;
    }
    
    // 读取Y坐标
    int yh = ft6336_read_reg(yh_reg);
    int yl = ft6336_read_reg(yl_reg);
    if (yh < 0 || yl < 0) {
        return -1;
    }
    
    // 组合坐标值（高8位和低8位）
    *x = ((xh & 0x0F) << 8) | xl;  // X坐标是12位，高4位在XH的低4位
    *y = ((yh & 0x0F) << 8) | yl;  // Y坐标是12位，高4位在YH的低4位
    
    return 0;
}

/**
 * @brief 读取触摸坐标（主接口）
 * @param touch_data 返回的触摸点数据
 * @return 0成功，-1失败
 * 
 * 注意：此函数只返回第一个触摸点的坐标
 * 对于多点触摸应用，可以扩展此函数
 */
int ft6336_read_xy(touch_point_t *touch_data)
{
    if (!touch_data) {
        return -1;
    }
    
    // 获取触摸点数量
    int touch_count = ft6336_get_touch_count();
    if (touch_count < 0) {
        return -1;
    }
    
    // 获取手势ID
    int gesture_id = ft6336_get_gesture_id();
    if (gesture_id < 0) {
        gesture_id = 0;
    }
    
    touch_data->touch_count = (uint8_t)touch_count;
    touch_data->gesture_id = (uint8_t)gesture_id;
    
    // 如果没有触摸，清空坐标
    if (touch_count == 0) {
        touch_data->x = 0;
        touch_data->y = 0;
        return 0;
    }
    
    // 读取第一个触摸点的坐标
    if (ft6336_read_touch_point(1, &touch_data->x, &touch_data->y) != 0) {
        return -1;
    }
    
    // 坐标范围检查（可选）
    if (touch_data->x > FT6336_SCREEN_WIDTH) {
        touch_data->x = FT6336_SCREEN_WIDTH;
    }
    if (touch_data->y > FT6336_SCREEN_HEIGHT) {
        touch_data->y = FT6336_SCREEN_HEIGHT;
    }
    
    return 0;
}

/**
 * @brief 初始化FT6336触摸屏
 * @return 0成功，-1失败
 */
int ft6336_init(void)
{
     i2c_drv = (I2cDrv*)wait_drv(I2C_DRV_INDEX);
    int device_mode = ft6336_read_reg(FT6336_REG_DEVICE_MODE);
    if (device_mode < 0) {
        return -1;
    }
    init_flag = 1;
    return 0;
}

static Touch_Data last_tdata ={0};

static void ft6636_obtain_coordinates(Touch_Data* tdata)
{
    touch_point_t touch_data;
    if(!init_flag)
    {
    	tdata->pressure = -1;
	return;
    }
    if (ft6336_read_xy(&touch_data) != 0) {
        return -1;
    }

	if(touch_data.touch_count == 0 ){
		if(last_touch_count == touch_data.touch_count){
			tdata->pressure = -1;
		}else{
			tdata->pressure = 0;
			tdata->x_pos = last_tdata.x_pos ;
			tdata->y_pos = last_tdata.y_pos;
		}
	}else{
		tdata->pressure = 1;
#ifdef HW_TOUCH_HORIZONTAL
		tdata->x_pos= touch_data.y;
		tdata->y_pos= HW_TOUCH_WIDTH -touch_data.x ;
#else
		tdata->x_pos= touch_data.x;
		tdata->y_pos= touch_data.y ;
#endif		
		last_tdata.x_pos = tdata->x_pos;
		last_tdata.y_pos = tdata->y_pos;
	}
	last_touch_count = touch_data.touch_count;
}


static Touch_Drv ft6636_touch = {
	.touch_init = ft6336_init,
	.obtain_coordinates = ft6636_obtain_coordinates,
}; 

#ifdef DRV_CORE

static int ft6336_open_cnt = 0;

static int ft6336_open(file_t *filp)
{
    if (ft6336_open_cnt > 0) {
        printf("FT6336 ReOpen(%d)\r\n", ft6336_open_cnt);
        return 0;
    }

	ft6336_init();

    ft6336_open_cnt++;
    printf("FT6336 Open Success\r\n");
    return 0;
}

static int ft6336_read(file_t *filp, uint8_t *buf, uint16_t len) {
    Touch_Data* out_data = (Touch_Data*)buf;
    if (buf == NULL || len < sizeof(Touch_Data)) {
        printf("ft6336 Read Error: invalid buffer (len=%d < 3)\r\n", len);
        return -1;
    }

    ft6636_obtain_coordinates(out_data);

    //printf("FT6336 Read: pressure=0x%x, keyvalue=0x%x, buf=[0x%02X,0x%02X,0x%02X]\r\n",
           //kdata.pressure, kdata.keyvalue,
           //buf[0], buf[1], buf[2]);

    return 5;
}

static int ft6336_close(file_t *filp)
{
    if (ft6336_open_cnt == 0) {
		printf("FT6336 Close Success\r\n");
        return 0;
    }

    ft6336_open_cnt--;
    printf("FT6336 Close(%d)\r\n", ft6336_open_cnt);
    return 0;
}

static drv_ops_t ft6336_drv_ops = {
    .name = TOUCH_DRV_NAME,
    .open = ft6336_open,
    .read = ft6336_read,
	.close = ft6336_close,
};

#endif

void ft6636_entry(void)
{
	register_drv(HW_TOUCH_DRV_INDEX,&ft6636_touch);
	printf("register hw touch driver %d \r\n",HW_TOUCH_DRV_INDEX);
	#ifdef DRV_CORE
	int ret = goldie_driver_register(&ft6336_drv_ops);
    if (ret == 0) {
        printf("FT6336 Driver Register Success: '%s'\r\n", TOUCH_DRV_NAME);
    } else {
        printf("FT6336 Driver Register Error: ret=%d\r\n", ret);
    }
	#endif
}

//GOLDIE_INIT_CALL_(ft6636_entry);

