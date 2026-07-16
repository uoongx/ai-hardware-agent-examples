#include "goldie_osal.h"
#include "service_manager.h"
#include "driver_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif
#include "aw9523b.h"

static int init_flag = 0;
static I2cDrv* i2c_drv;
static goldie_mutex gpio_ext_mutex;
// 0x18 0x38 0x5b
// 内部状态缓存
static uint8_t output_state[2] = {0xFF, 0xFF}; // 默认输出高电平
static uint8_t config_state[2] = {0xFF, 0xFF}; // 默认配置为输入

// 初始化AW9523B
static int aw9523b_drv_init(void)
{
    int ret;
    char chip_id;
    goldie_mutex_lock(&gpio_ext_mutex);
    if (init_flag) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return 0; // 已经初始化过
    }
    // 等待I2C驱动就绪
    i2c_drv = (I2cDrv*)wait_drv(I2C_DRV_INDEX);
    if (!i2c_drv) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return -1;
    }

    // 初始化I2C
    ret = i2c_drv->i2c_init(I2C_BUS_BAUDRATE);
    if (ret != 0) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return ret;
    }
    // 读取芯片ID验证
    ret = i2c_drv->i2c_reg_read(AW9523B_ADDR, AW9523B_REG_ID, &chip_id);
    if (ret != 0 || chip_id != AW9523B_CHIP_ID) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return -1;
    }

    // 配置全局控制寄存器: 所有端口为GPIO模式
    ret = i2c_drv->i2c_reg_write(AW9523B_ADDR, AW9523B_REG_CTL, 0x00);
    if (ret != 0) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return ret;
    }

    // 初始化所有GPIO为输入模式，输出高电平
    ret = i2c_drv->i2c_reg_write(AW9523B_ADDR, AW9523B_REG_CONFIG0, 0xFF);
    if (ret == 0) {
        ret = i2c_drv->i2c_reg_write(AW9523B_ADDR, AW9523B_REG_CONFIG1, 0xFF);
    }
    if (ret == 0) {
        ret = i2c_drv->i2c_reg_write(AW9523B_ADDR, AW9523B_REG_OUTPUT0, 0xFF);
    }
    if (ret == 0) {
        ret = i2c_drv->i2c_reg_write(AW9523B_ADDR, AW9523B_REG_OUTPUT1, 0xFF);
    }

    if (ret == 0) {
        init_flag = 1;
    }

    goldie_mutex_unlock(&gpio_ext_mutex);
    return ret;
}

// 设置GPIO功能 (0:输出, 1:输入)
static int aw9523b_set_gpio_func(int io_index, int func)
{
    int ret;
    uint8_t port, bit_mask, reg_addr;
    uint8_t current_config;

    if (!init_flag || !i2c_drv) {
        return -1;
    }

    if (io_index < 0 || io_index > 15) {
        return -1; // 无效的GPIO索引
    }

    goldie_mutex_lock(&gpio_ext_mutex);

    // 计算端口和位掩码
    port = io_index / 8;
    bit_mask = 1 << (io_index % 8);
    reg_addr = AW9523B_REG_CONFIG0 + port;

    // 读取当前配置
    ret = i2c_drv->i2c_reg_read(AW9523B_ADDR, reg_addr, &current_config);
    if (ret != 0) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return ret;
    }

    // 更新配置
    if (func == GOLDIE_GPIO_DIRECTION_OUTPUT) { // 输出模式
        current_config &= ~bit_mask;
    } else { // 输入模式
        current_config |= bit_mask;
    }

    // 写回配置
    ret = i2c_drv->i2c_reg_write(AW9523B_ADDR, reg_addr, current_config);
    if (ret == 0) {
        config_state[port] = current_config;
    }

    goldie_mutex_unlock(&gpio_ext_mutex);
    return ret;
}

// 设置GPIO输出值 (0:低电平, 1:高电平)
static int aw9523b_set_gpio_value(int io_index, int value)
{
    int ret;
    uint8_t port, bit_mask, reg_addr;
    uint8_t current_output;

    if (!init_flag || !i2c_drv) {
        return -1;
    }

    if (io_index < 0 || io_index > 15) {
        return -1; // 无效的GPIO索引
    }

    goldie_mutex_lock(&gpio_ext_mutex);

    // 计算端口和位掩码
    port = io_index / 8;
    bit_mask = 1 << (io_index % 8);
    reg_addr = AW9523B_REG_OUTPUT0 + port;

    // 读取当前输出状态
    ret = i2c_drv->i2c_reg_read(AW9523B_ADDR, reg_addr, &current_output);
    if (ret != 0) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return ret;
    }

    // 更新输出状态
    if (value == 0) { // 输出低电平
        current_output &= ~bit_mask;
    } else { // 输出高电平
        current_output |= bit_mask;
    }

    // 写回输出状态
    ret = i2c_drv->i2c_reg_write(AW9523B_ADDR, reg_addr, current_output);
    if (ret == 0) {
        output_state[port] = current_output;
    }

    goldie_mutex_unlock(&gpio_ext_mutex);
    return ret;
}

// 获取GPIO输入值
static int aw9523b_get_gpio_value(int io_index)
{
    int ret;
    uint8_t port, bit_mask, reg_addr;
    uint8_t input_value;

    if (!init_flag || !i2c_drv) {
        return -1;
    }

    if (io_index < 0 || io_index > 15) {
        return -1; // 无效的GPIO索引
    }

    goldie_mutex_lock(&gpio_ext_mutex);

    // 计算端口和位掩码
    port = io_index / 8;
    bit_mask = 1 << (io_index % 8);
    reg_addr = AW9523B_REG_INPUT0 + port;

    // 读取输入状态
    ret = i2c_drv->i2c_reg_read(AW9523B_ADDR, reg_addr, &input_value);
    if (ret != 0) {
        goldie_mutex_unlock(&gpio_ext_mutex);
        return -1;
    }

    goldie_mutex_unlock(&gpio_ext_mutex);

    // 返回指定位的状态
    return (input_value & bit_mask) ? 1 : 0;
}

#ifdef DRV_CORE

static int aw9523b_dev_open_cnt = 0;

static int aw9523b_open(file_t *filp)
{
    if (aw9523b_dev_open_cnt > 0) {
        printf("AW9523B Open(%d)\r\n", aw9523b_dev_open_cnt);
        return 0;
    }

    int ret = aw9523b_drv_init();
    if (ret != 0) {
        printf("AW9523B Open Error: drv_init failed (ret=%d)\r\n", ret);
        return -1;
    }

    aw9523b_dev_open_cnt++;
    printf("AW9523B Open Success\r\n");
    return 0;
}

static int aw9523b_ioctl(file_t *filp, uint8_t cmd, void *arg)
{
    if (arg == NULL) {
        printf("AW9523B IOCTL Error: arg is null\r\n");
        return -1;
    }

    if (!init_flag || !i2c_drv) {
        printf("AW9523B IOCTL Error: driver not initialized\r\n");
        return -1;
    }

    aw9523b_ioctl_arg_t *ioctl_arg = (aw9523b_ioctl_arg_t*)arg;
    int ret = -1;

    if (ioctl_arg->io_index < 0 || ioctl_arg->io_index > 15) {
        printf("AW9523B IOCTL Error: invalid io_index %d (0-15 only)\r\n", ioctl_arg->io_index);
        return -1;
    }

    switch (cmd) {
        case AW9523B_IOCTL_SET_DIR:
            ret = aw9523b_set_gpio_func(ioctl_arg->io_index, ioctl_arg->value);
            if (ret == 0) {
                printf("AW9523B IOCTL Set Dir Success: GPIO%d -> %s\r\n",
                       ioctl_arg->io_index, ioctl_arg->value ? "INPUT" : "OUTPUT");
            } else {
                printf("AW9523B IOCTL Set Dir Error: GPIO%d (ret=%d)\r\n",
                       ioctl_arg->io_index, ret);
            }
            break;

        case AW9523B_IOCTL_SET_VAL:
            ret = aw9523b_set_gpio_value(ioctl_arg->io_index, ioctl_arg->value);
            if (ret == 0) {
                printf("AW9523B IOCTL Set Val Success: GPIO%d -> %s\r\n",
                       ioctl_arg->io_index, ioctl_arg->value ? "HIGH" : "LOW");
            } else {
                printf("AW9523B IOCTL Set Val Error: GPIO%d (ret=%d)\r\n",
                       ioctl_arg->io_index, ret);
            }
            break;

        case AW9523B_IOCTL_GET_VAL:
            ret = aw9523b_get_gpio_value(ioctl_arg->io_index);
            if (ret >= 0) {
                ioctl_arg->value = ret;
                ret = 0;
            } else {
                printf("AW9523B IOCTL Get Val Error: GPIO%d (ret=%d)\r\n",
                       ioctl_arg->io_index, ret);
                ret = -1;
            }
            break;

        default:
            printf("AW9523B IOCTL Error: invalid cmd 0x%02X\r\n", cmd);
            ret = -1;
            break;
    }

    return ret;
}

static int aw9523b_close(file_t *filp)
{
    if (aw9523b_dev_open_cnt == 0) {
		printf("AW9523B Close Success\r\n");
        return 0;
    }

    aw9523b_dev_open_cnt--;
    printf("AW9523B Close\r\n");
    return 0;
}

static drv_ops_t aw9523b_drv_ops = {
    .name = AW9523B_DRV_NAME,
    .open = aw9523b_open,
	.ioctl = aw9523b_ioctl,
    .close = aw9523b_close,
};
#endif

// GPIO驱动扩展结构体
static GpioDrvExt gpio_ext_drv = {
    .drv_init = aw9523b_drv_init,
    .set_gpio_func = aw9523b_set_gpio_func,
    .set_gpio_value = aw9523b_set_gpio_value,
    .get_gpio_value = aw9523b_get_gpio_value,
};

static void gpio_ext_drv_entry(void)
{
    goldie_mutex_init(&gpio_ext_mutex);
    register_drv(GPIO_EXT_DRV_INDEX, (void*)(&gpio_ext_drv));

#ifdef DRV_CORE
	int ret = goldie_driver_register(&aw9523b_drv_ops);
    if (ret == 0) {
        printf("AW9523B Driver Register Success: '%s'\r\n", AW9523B_DRV_NAME);
    } else {
        printf("AW9523B Driver Register Error: ret=%d\r\n", ret);
    }
#endif
}
GOLDIE_INIT_CALL_(gpio_ext_drv_entry);
