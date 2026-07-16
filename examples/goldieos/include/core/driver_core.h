#ifndef INCLUDE_DRIVER_CORE_
#define INCLUDE_DRIVER_CORE_
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DRV_NAME_MAX_LEN     16
#define MAX_REG_DRV          16
#define MAX_OPEN_FDS         (2*MAX_REG_DRV)
#define INVALID_FD           -1

#define O_RDONLY             0x01    // 只读
#define O_WRONLY             0x02    // 只写
#define O_RDWR               0x03    // 读写

#define DRV_MATCH_RETRY_CNT  20      // 最大重试次数
#define DRV_MATCH_DELAY_MS   50      // 每次重试延时（ms）

// IOCTL命令定义
#define I2C_IOCTL_READ_REG   		0x01
#define I2C_IOCTL_WRITE_REG  		0x02
#define I2C_IOCTL_READ_MULTI_REG    0x03
#define I2C_IOCTL_WRITE_MULTI_REG   0x04

#define AW9523B_IOCTL_SET_DIR   0x01    // 设置GPIO方向
#define AW9523B_IOCTL_SET_VAL   0x02    // 设置GPIO输出值
#define AW9523B_IOCTL_GET_VAL   0x03    // 获取GPIO输入值


#define BAT_IOCTL_CAL_POWER  0x1
#define BAT_IOCTL_GET_CHGSTATUS  0x2

#define PCF8563_IOCTL_GET_TIME  0x01  // 获取时间
#define PCF8563_IOCTL_SET_TIME  0x02  // 设置时间

#define BAT_DRV_NAME			"battery"
#define I2C_DRV_NAME			"i2c"
#define AW9523B_DRV_NAME             "aw9523b"
#define GPIO_KEY_DRV_NAME		"gpio_key"
#define TOUCH_DRV_NAME		"touch"
#define RTC_DRV_NAME 			"rtc"

typedef struct {
    int io_index;                     // GPIO索引(0-15)
    int value;                        // 方向/电平值（0=输入/低电平，1=输出/高电平）
} aw9523b_ioctl_arg_t;

typedef struct {
    uint8_t dev_addr;	// I2C设备地址
    uint8_t reg_addr;	// 寄存器地址
    uint8_t *reg_data;	// 寄存器数据
    uint8_t len;
} i2c_ioctl_arg_t;


typedef struct file_t  file_t;

/**
 * @brief 驱动操作集结构体（定义驱动的核心接口）
 */
typedef struct {
    const char *name;                  // 驱动名称
    int (*open)(file_t *filp);         // 驱动打开接口
    int (*close)(file_t *filp);        // 驱动关闭接口
    int (*read)(file_t *filp, uint8_t *buf, uint16_t len);  // 数据读取接口
    int (*write)(file_t *filp, const uint8_t *buf, uint16_t len); // 数据写入接口
    int (*ioctl)(file_t *filp, uint8_t cmd, void *arg);     // 控制命令接口
} drv_ops_t;

/**
 * @brief 文件描述符上下文结构体（每个FD对应一个实例）
 */
typedef struct file_t {
    drv_ops_t *ops;            // 绑定的驱动操作集
    void *priv_data;           // 驱动私有数据
    int fd;                    // 对应的文件描述符
    int flags;                 // 打开标志（O_RDONLY/O_WRONLY/O_RDWR）
    uint8_t is_used;           // FD是否被使用（0-未使用，1-使用中）
} file_t;

// -------------------------- 函数原型声明 --------------------------
/**
 * @brief 注册驱动（将驱动操作集加入注册表）
 * @param ops 驱动操作集指针
 * @return 0-成功，-1-失败
 */
int goldie_driver_register(drv_ops_t *ops);

/**
 * @brief 打开驱动（带10ms轮询，5次重试）
 * @param drv_name 驱动名称
 * @param flags 打开标志（O_RDONLY/O_WRONLY/O_RDWR）
 * @return 成功返回FD，失败返回INVALID_FD
 */
int goldie_open(const char *drv_name, int flags);

/**
 * @brief 读取驱动数据
 * @param fd 文件描述符
 * @param buf 接收数据缓冲区
 * @param len 读取长度
 * @return 成功返回读取字节数，失败返回-1
 */
int goldie_read(int fd, uint8_t *buf, uint16_t len);

/**
 * @brief 写入驱动数据
 * @param fd 文件描述符
 * @param buf 发送数据缓冲区
 * @param len 写入长度
 * @return 成功返回写入字节数，失败返回-1
 */
int goldie_write(int fd, const uint8_t *buf, uint16_t len);

/**
 * @brief 驱动控制命令
 * @param fd 文件描述符
 * @param cmd 控制命令码
 * @param arg 命令参数
 * @return 0-成功，-1-失败
 */
int goldie_ioctl(int fd, uint8_t cmd, void *arg);

/**
 * @brief 关闭驱动（释放FD）
 * @param fd 文件描述符
 * @return 0-成功，-1-失败
 */
int goldie_close(int fd);

#endif // INCLUDE_DRIVER_CORE_
