#include <stdint.h>
#include "driver_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif

//#define DEBUG_TOUCH_POINT

static I2cDrv *i2c_drv = 0;
static int init_flag = 0;
static int last_touch_count = 0;

#ifdef DEBUG_TOUCH_REG
static int read_counter = 0;  // 读取计数器
#endif

#define HW_TOUCH_HORIZONTAL

// 屏幕分辨率配置（根据实际屏幕修改）
#ifdef HW_TOUCH_HORIZONTAL
#define TOUCH_WIDTH 320
#define TOUCH_HEIGHT 240
#else
#define TOUCH_WIDTH 240
#define TOUCH_HEIGHT 320
#endif

// CST816D I2C地址（7位地址）
#define CST816D_I2C_ADDR         0x15

// CST816D 寄存器地址（参考数据手册）
#define CST816D_REG_GESTURE      0x01  // 手势ID
#define CST816D_REG_FINGERNUM    0x02  // 触摸点数
#define CST816D_REG_XPOS_H       0x03  // X坐标高8位
#define CST816D_REG_XPOS_L       0x04  // X坐标低8位
#define CST816D_REG_YPOS_H       0x05  // Y坐标高8位
#define CST816D_REG_YPOS_L       0x06  // Y坐标低8位

// CST816D 控制寄存器
#define CST816D_REG_CHIP_ID      0xA7  // 芯片ID
#define CST816D_REG_PROJ_ID      0xA8  // 项目ID
#define CST816D_REG_FW_VERSION   0xA9  // 固件版本
#define CST816D_REG_SLEEP_MODE   0xE5  // 睡眠模式控制
#define CST816D_REG_LED_MODE     0xFA  // LED模式控制
#define CST816D_REG_IRQ_CTL      0xFE  // 中断控制
#define CST816D_REG_SWRESET      0xFF  // 软件复位

// CST816D 手势ID定义
#define CST816D_GESTURE_NONE     0x00  // 无手势
#define CST816D_GESTURE_UP       0x01  // 向上滑动
#define CST816D_GESTURE_DOWN     0x02  // 向下滑动
#define CST816D_GESTURE_LEFT     0x03  // 向左滑动
#define CST816D_GESTURE_RIGHT    0x04  // 向右滑动
#define CST816D_GESTURE_CLICK    0x05  // 单击
#define CST816D_GESTURE_DOUBLE   0x0B  // 双击
#define CST816D_GESTURE_LONG     0x0C  // 长按

// 需要dump的寄存器地址列表
static const uint8_t dump_registers[] = {
    // 状态寄存器
    0x00,  // 设备模式/状态
    0x01,  // 手势ID
    0x02,  // 触摸点数
    
    // 坐标寄存器
    0x03,  // X坐标高8位
    0x04,  // X坐标低8位
    0x05,  // Y坐标高8位
    0x06,  // Y坐标低8位
    
    // 配置寄存器
    0xA0,  // 控制寄存器1
    0xA1,  // 控制寄存器2
    0xA2,  // 控制寄存器3
    0xA3,  // 控制寄存器4
    0xA4,  // 控制寄存器5
    0xA5,  // 控制寄存器6
    
    // ID寄存器
    0xA7,  // 芯片ID
    0xA8,  // 项目ID
    0xA9,  // 固件版本
    
    // 电源管理
    0xE5,  // 睡眠模式控制
    
    // LED控制
    0xFA,  // LED模式控制
    
    // 中断控制
    0xFE,  // 中断控制
    
    // 复位控制
    0xFF,  // 软件复位
};

#define DUMP_REG_COUNT (sizeof(dump_registers) / sizeof(dump_registers[0]))

// 触摸数据结构
typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t touch_count;
    uint8_t gesture_id;
} touch_point_t;

/**
 * @brief 读取CST816D寄存器值
 * @param reg_addr 寄存器地址
 * @return 读取到的寄存器值，-1表示读取失败
 */
static int cst816d_read_reg(uint8_t reg_addr)
{
    char read_data = 0;
    int ret = i2c_drv->i2c_reg_read(CST816D_I2C_ADDR, reg_addr, &read_data);
    if (ret != 0) {
        return -1;
    }
    return (int)read_data;
}

static int cst816d_read_multi_regs(uint8_t reg_addr,char* buffer,int reg_size)
{
    int ret = i2c_drv->i2c_reg_read_multi(CST816D_I2C_ADDR, reg_addr, buffer,reg_size);
    if (ret != 0) {
	printf("cst816d_read_multi_regs failed \r\n");
        return -1;
    }
    return 0;
}


/**
 * @brief 写入CST816D寄存器值
 * @param reg_addr 寄存器地址
 * @param data 要写入的数据
 * @return 0成功，-1失败
 */
static int cst816d_write_reg(uint8_t reg_addr, uint8_t data)
{
    return i2c_drv->i2c_reg_write(CST816D_I2C_ADDR, reg_addr, data);
}

/**
 * @brief 获取触摸点数
 * @return 触摸点数，-1表示读取失败
 */
static int cst816d_get_touch_count(void)
{
    int finger_num = cst816d_read_reg(CST816D_REG_FINGERNUM);
    if (finger_num < 0) {
        return -1;
    }
    return finger_num & 0x0F;  // 低4位表示触摸点数
}

/**
 * @brief 获取手势ID
 * @return 手势ID，-1表示读取失败
 */
static int cst816d_get_gesture_id(void)
{
    return cst816d_read_reg(CST816D_REG_GESTURE);
}

/**
 * @brief 读取触摸点坐标
 * @param x 返回的X坐标
 * @param y 返回的Y坐标
 * @return 0成功，-1失败
 */
static int cst816d_read_touch_point(uint16_t *x, uint16_t *y)
{
    int xh, xl, yh, yl;
    char buffer[6] = {0};
    cst816d_read_multi_regs(CST816D_REG_XPOS_H,buffer,6);
    xh = buffer[0];
    xl = buffer[1];
    yh= buffer[2];
    yl = buffer[3];
    // CST816D的坐标是12位数据，高4位在_H寄存器的高4位
    *x = ((xh & 0x0F) << 8) | xl;
    *y = ((yh & 0x0F) << 8) | yl;
    
    return 0;
}

/**
 * @brief 读取多个连续寄存器
 * @param reg_addr 起始寄存器地址
 * @param buffer 数据缓冲区
 * @param length 要读取的长度
 * @return 0成功，-1失败
 */
static int cst816d_read_regs(uint8_t reg_addr, uint8_t *buffer, uint8_t length)
{
    if (!buffer || length == 0) {
        return -1;
    }
    
    // 使用单字节读取方式读取多个寄存器
    for (int i = 0; i < length; i++) {
        int value = cst816d_read_reg(reg_addr + i);
        if (value < 0) {
            return -1;
        }
        buffer[i] = (uint8_t)value;
    }
    
    return 0;
}

/**
 * @brief 读取触摸坐标数据（接口函数）
 * @param touch_data 返回的触摸数据
 * @return 0成功，-1失败
 */
int cst816d_read_xy(touch_point_t *touch_data)
{
    if (!touch_data) {
        return -1;
    }
    
    // 获取触摸点数
    int touch_count = cst816d_get_touch_count();
    if (touch_count < 0) {
        return -1;
    }
    
    // 获取手势ID
    int gesture_id = cst816d_get_gesture_id();
    if (gesture_id < 0) {
        gesture_id = CST816D_GESTURE_NONE;
    }
    
    touch_data->touch_count = (uint8_t)touch_count;
    touch_data->gesture_id = (uint8_t)gesture_id;
    
    // 如果没有触摸，清零坐标
    if (touch_count == 0) {
        touch_data->x = 0;
        touch_data->y = 0;
        return 0;
    }
    
    // 读取触摸坐标
    if (cst816d_read_touch_point(&touch_data->x, &touch_data->y) != 0) {
        return -1;
    }
    return 0;
}

/**
 * @brief 进入睡眠模式
 * @return 0成功，-1失败
 */
static int cst816d_enter_sleep(void)
{
    return cst816d_write_reg(CST816D_REG_SLEEP_MODE, 0x03);
}

/**
 * @brief 退出睡眠模式
 * @return 0成功，-1失败
 */
static int cst816d_exit_sleep(void)
{
    return cst816d_write_reg(CST816D_REG_SLEEP_MODE, 0x01);
}

/**
 * @brief 设置中断模式
 * @param mode 中断模式
 * @return 0成功，-1失败
 */
static int cst816d_set_irq_mode(uint8_t mode)
{
    return cst816d_write_reg(CST816D_REG_IRQ_CTL, mode);
}

/**
 * @brief 读取芯片ID
 * @return 芯片ID，-1表示读取失败
 */
static int cst816d_get_chip_id(void)
{
    return cst816d_read_reg(CST816D_REG_CHIP_ID);
}


#ifdef DEBUG_TOUCH_REG
/**
 * @brief 打印CST816D所有寄存器值
 * @note 每20次读取坐标后自动调用
 */
static void cst816d_dump_registers(void)
{
    printf("\n========== CST816D Register Dump ==========\n");
    printf("Total registers: %d\n", DUMP_REG_COUNT);
    printf("I2C Address: 0x%02X\n", CST816D_I2C_ADDR);
    
    int success_count = 0;
    int fail_count = 0;
    
    for (int i = 0; i < DUMP_REG_COUNT; i++) {
        uint8_t reg_addr = dump_registers[i];
        int value = cst816d_read_reg(reg_addr);
        
        if (value >= 0) {
            // 根据寄存器地址显示不同信息
            switch (reg_addr) {
                case 0x00:
                    printf("0x%02X: 0x%02X (Device Mode/Status", reg_addr, value);
                    if (value & 0x80) printf(", Power On");
                    if (value & 0x40) printf(", Gesture Mode");
                    if (value & 0x20) printf(", System Running");
                    printf(")\n");
                    break;
                    
                case 0x01:
                    printf("0x%02X: 0x%02X (Gesture ID: ", reg_addr, value);
                    switch (value) {
                        case CST816D_GESTURE_NONE: printf("None"); break;
                        case CST816D_GESTURE_UP: printf("Up"); break;
                        case CST816D_GESTURE_DOWN: printf("Down"); break;
                        case CST816D_GESTURE_LEFT: printf("Left"); break;
                        case CST816D_GESTURE_RIGHT: printf("Right"); break;
                        case CST816D_GESTURE_CLICK: printf("Click"); break;
                        case CST816D_GESTURE_DOUBLE: printf("Double Click"); break;
                        case CST816D_GESTURE_LONG: printf("Long Press"); break;
                        default: printf("Unknown");
                    }
                    printf(")\n");
                    break;
                    
                case 0x02:
                    printf("0x%02X: 0x%02X (Touch Count: %d)\n", 
                           reg_addr, value, value & 0x0F);
                    break;
                    
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                    printf("0x%02X: 0x%02X\n", reg_addr, value);
                    break;
                    
                case 0xA7:
                    printf("0x%02X: 0x%02X (Chip ID", reg_addr, value);
                    if (value == 0xB4 || value == 0xB5) {
                        printf(", Valid");
                    }
                    printf(")\n");
                    break;
                    
                case 0xA8:
                    printf("0x%02X: 0x%02X (Project ID)\n", reg_addr, value);
                    break;
                    
                case 0xA9:
                    printf("0x%02X: 0x%02X (Firmware Version)\n", reg_addr, value);
                    break;
                    
                case 0xE5:
                    printf("0x%02X: 0x%02X (Sleep Mode", reg_addr, value);
                    if (value == 0x00) printf(", Normal");
                    else if (value == 0x03) printf(", Sleep");
                    printf(")\n");
                    break;
                    
                case 0xFA:
                    printf("0x%02X: 0x%02X (LED Mode", reg_addr, value);
                    if (value == 0x00) printf(", Off");
                    else if (value == 0x01) printf(", On");
                    printf(")\n");
                    break;
                    
                case 0xFE:
                    printf("0x%02X: 0x%02X (IRQ Control", reg_addr, value);
                    if ((value & 0x01) == 0) printf(", Falling Edge");
                    else printf(", Low Level");
                    printf(")\n");
                    break;
                    
                case 0xFF:
                    printf("0x%02X: 0x%02X (Software Reset)\n", reg_addr, value);
                    break;
                    
                default:
                    printf("0x%02X: 0x%02X\n", reg_addr, value);
                    break;
            }
            success_count++;
        } else {
            printf("0x%02X: Read Failed\n", reg_addr);
            fail_count++;
        }
        
        // 每读取4个寄存器后添加一点延迟，避免I2C总线过载
        if ((i + 1) % 4 == 0) {
            // 可以添加微秒级延迟
            // delay_us(10);
        }
    }
    
    printf("Success: %d, Failed: %d\n", success_count, fail_count);
    printf("===========================================\n\n");
}
#endif

/**
 * @brief 初始化CST816D触摸芯片
 * @return 0成功，-1失败
 */
int cst816d_init(void)
{
    // 获取I2C驱动
    i2c_drv = (I2cDrv*)wait_drv(I2C_DRV_INDEX);
    if (!i2c_drv) {
        printf("Failed to get I2C driver\n");
        return -1;
    }
     
    // 读取芯片ID验证通信
    int chip_id = cst816d_get_chip_id();
    if (chip_id < 0) {
        printf("Failed to read CST816D chip ID\n");
        return -1;
    }
    
    // CST816D芯片ID通常是0xB4或0xB5
    printf("CST816D chip ID: 0x%02X\n", chip_id);
    if (chip_id != 0xB6) {
        printf("Warning: Unexpected CST816D chip ID\n");
    }
    
    // 配置中断模式（根据实际硬件连接配置）
    // 0x00: 下降沿触发，0x01: 低电平触发
    if (cst816d_set_irq_mode(0x00) != 0) {
        printf("Failed to set CST816D interrupt mode\n");
    }
    
    // 退出睡眠模式
    if (cst816d_exit_sleep() != 0) {
        printf("Failed to wake up CST816D\n");
    }
#ifdef DEBUG_TOUCH_REG
    // 初始化读取计数器
    read_counter = 0;
    // 初始化完成后dump一次寄存器
    cst816d_dump_registers();
#endif    
    init_flag = 1;
    printf("CST816D initialized successfully\n");
    return 0;
}

static Touch_Data last_tdata = {0};

/**
 * @brief 获取触摸坐标信息（主回调函数）
 * @param tdata 触摸数据结构
 */
static void cst816d_obtain_coordinates(Touch_Data* tdata)
{
    touch_point_t touch_data;
    
    if (!init_flag || !tdata) {
        tdata->pressure = -1;
        return;
    }
    
    // 读取触摸数据
    if (cst816d_read_xy(&touch_data) != 0) {
        tdata->pressure = -1;
        return;
    }

#ifdef DEBUG_TOUCH_REG
    // 更新读取计数器
    read_counter++;
    
    // 每20次读取dump一次寄存器
    if (read_counter >= 200) {
        cst816d_dump_registers();
        read_counter = 0;  // 重置计数器
    }
#endif    
    // 处理触摸状态变化
    if (touch_data.touch_count == 0) {
        // 没有触摸
        if (last_touch_count == touch_data.touch_count) {
            // 连续无触摸，上报无效
            tdata->pressure = -1;
        } else {
            // 从有触摸变为无触摸，上报释放事件
            tdata->pressure = 0;
            tdata->x_pos = last_tdata.x_pos;
            tdata->y_pos = last_tdata.y_pos;
        }
    } else {
        // 有触摸
        tdata->pressure = 1;
        
#ifdef HW_TOUCH_HORIZONTAL
        // 横向屏幕，需要坐标转换
        tdata->x_pos = touch_data.y;
        tdata->y_pos = touch_data.x;
#else
        // 纵向屏幕，直接使用
        tdata->x_pos = touch_data.x;
        tdata->y_pos = touch_data.y;
#endif
	tdata->x_pos = tdata->x_pos + tdata->x_pos*23/100; //X_POS*320/240 = X_POS*1.333 = X_POS+XPOS*1/3
        // 保存最后一次有效坐标
        last_tdata.x_pos = tdata->x_pos;

        last_tdata.y_pos = tdata->y_pos;
        
        // 可选：打印坐标信息用于调试
        /* printf("Touch: x=%d, y=%d, count=%d, gesture=0x%02X\n", 
                tdata->x_pos, tdata->y_pos, touch_data.touch_count, touch_data.gesture_id);*/
    }
    
    // 更新触摸点计数
    last_touch_count = touch_data.touch_count;
}

// 驱动接口定义
static Touch_Drv cst816d_touch = {
    .touch_init = cst816d_init,
    .obtain_coordinates = cst816d_obtain_coordinates,
};

#ifdef DRV_CORE

static int cst816d_open_cnt = 0;

static int cst816d_open(file_t *filp)
{
    if (cst816d_open_cnt > 0) {
        printf("CST816D ReOpen(%d)\r\n", cst816d_open_cnt);
        return 0;
    }

	cst816d_init();

    cst816d_open_cnt++;
    printf("CST816D Open Success\r\n");
    return 0;
}

static int cst816d_read(file_t *filp, uint8_t *buf, uint16_t len) {
    Touch_Data* out_data = (Touch_Data*)buf;
    if (buf == NULL || len < sizeof(Touch_Data)) {
        printf("CST816D Read Error: invalid buffer (len=%d < 3)\r\n", len);
        return -1;
    }

    cst816d_obtain_coordinates(out_data);
    //printf("CST816D Read: pressure=0x%x, x=0x%x, y=0x%x, buf=[0x%02X,0x%02X,0x%02X,0x%02X,0x%02X]\r\n",
           //tdata.pressure, tdata.x_pos,tdata.y_pos,
           //buf[0], buf[1], buf[2], buf[3], buf[4]);

    return 5;
}

static int cst816d_close(file_t *filp)
{
    if (cst816d_open_cnt == 0) {
		printf("CST816D Close Success\r\n");
        return 0;
    }

    cst816d_open_cnt--;
    printf("CST816D Close(%d)\r\n", cst816d_open_cnt);
    return 0;
}

static drv_ops_t cst816d_drv_ops = {
    .name = TOUCH_DRV_NAME,
    .open = cst816d_open,
	.read = cst816d_read,
    .close = cst816d_close,
};

#endif

/**
 * @brief CST816D驱动入口函数
 */
void cst816d_entry(void)
{
    register_drv(HW_TOUCH_DRV_INDEX, &cst816d_touch);
    printf("Register CST816D touch driver at index %d\n", HW_TOUCH_DRV_INDEX);
	#ifdef DRV_CORE
	int ret = goldie_driver_register(&cst816d_drv_ops);
    if (ret == 0) {
        printf("CST816D Driver Register Success: '%s'\r\n", TOUCH_DRV_NAME);
    } else {
        printf("CST816D Driver Register Error: ret=%d\r\n", ret);
    }
	#endif
}
