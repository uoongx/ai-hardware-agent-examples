#ifdef PLATFORM_TYPE_WS63

#include "driver_manager.h"
#include "driver_core.h"

static I2cDrv *i2c_drv;

typedef struct {
int is_init;
int (*read_reg)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);
int (*write_reg)(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
int (*read_reg_multi)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,uint8_t len);
int (*write_reg_multi)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,uint8_t len);
} i2c_dev_t;

static i2c_dev_t i2c_dev = {
 .is_init = 0,
 .read_reg = NULL,
 .write_reg = NULL,
 .read_reg_multi = NULL,
 .write_reg_multi = NULL,
};

static int i2c_open(file_t *filp) {
    filp->priv_data = &i2c_dev;
    i2c_dev_t *i2c = (i2c_dev_t*)filp->priv_data;

    if (i2c->is_init) {
        printf("I2C Open Error: driver already initialized (second open denied)\r\n");
        filp->priv_data = NULL;
        return -1;
    }

	i2c_drv = (I2cDrv*)wait_drv(I2C_DRV_INDEX);
    if (!i2c_drv) {
		printf("I2C Driver Register Error: wait_drv =%d\r\n", i2c_drv);
		return;
    }
	i2c_drv->i2c_init(I2C_BUS_BAUDRATE);
	i2c_dev.read_reg = i2c_drv->i2c_reg_read;
	i2c_dev.write_reg = i2c_drv->i2c_reg_write;
	i2c_dev.read_reg_multi = i2c_drv->i2c_reg_read_multi;
	i2c_dev.write_reg_multi = i2c_drv->i2c_reg_write_multi;

    i2c->is_init = 1;
    printf("I2C Open Success: driver ready\r\n");
    return 0;
}

static int i2c_read(file_t *filp, uint8_t *buf, uint16_t len) {
    if (buf == NULL || len < 3) {
        printf("I2C Read Error: invalid buffer (len=%d < 3)\r\n", len);
        return -1;
    }

    i2c_dev_t *i2c = (i2c_dev_t*)filp->priv_data;
    if (i2c == NULL || !i2c->is_init) {
        printf("I2C Read Error: driver not initialized\r\n");
        return -1;
    }

    uint8_t dev_addr = buf[0];
    uint8_t reg_addr = buf[1];
    uint8_t read_data = 0;

    int ret = i2c->read_reg(dev_addr, reg_addr, &read_data);
    if (ret != 0) {
        printf("I2C Read Error: dev 0x%02X, reg 0x%02X (ret=%d)\r\n",
               dev_addr, reg_addr, ret);
        return -1;
    }

    buf[2] = read_data;
    printf("I2C Read Success: dev 0x%02X, reg 0x%02X, data 0x%02X\r\n",
           dev_addr, reg_addr, read_data);
    return 3;
}

static int i2c_write(file_t *filp, const uint8_t *buf, uint16_t len) {
    if (buf == NULL || len < 3) {
        printf("I2C Write Error: invalid buffer (len=%d < 3)\r\n", len);
        return -1;
    }

    i2c_dev_t *i2c = (i2c_dev_t*)filp->priv_data;
    if (i2c == NULL || !i2c->is_init) {
        printf("I2C Write Error: driver not initialized\r\n");
        return -1;
    }

    uint8_t dev_addr = buf[0];
    uint8_t reg_addr = buf[1];
    uint8_t send_data = buf[2];

    int ret = i2c->write_reg(dev_addr, reg_addr, send_data);
    if (ret != 0) {
        printf("I2C Write Error: dev 0x%02X, reg 0x%02X (ret=%d)\r\n",
               dev_addr, reg_addr, ret);
        return -1;
    }

    printf("I2C Write Success: dev 0x%02X, reg 0x%02X, data 0x%02X\r\n",
           dev_addr, reg_addr, send_data);
    return 3;
}

static int i2c_ioctl(file_t *filp, uint8_t cmd, void *arg) {
    if (arg == NULL) {
        printf("I2C IOCTL Error: arg is null\r\n");
        return -1;
    }

    i2c_dev_t *i2c = (i2c_dev_t*)filp->priv_data;
    if (i2c == NULL || !i2c->is_init) {
        printf("I2C IOCTL Error: driver not initialized\r\n");
        return -1;
    }

    i2c_ioctl_arg_t *ioctl_arg = (i2c_ioctl_arg_t*)arg;
    int ret = -1;

    switch (cmd) {
        case I2C_IOCTL_READ_REG:
            ret = i2c->read_reg(ioctl_arg->dev_addr, ioctl_arg->reg_addr, &ioctl_arg->reg_data[0]);
            if (ret == 0) {
                printf("I2C IOCTL Read Success: dev 0x%02X, reg 0x%02X, data 0x%02X\r\n",
                       ioctl_arg->dev_addr, ioctl_arg->reg_addr, ioctl_arg->reg_data[0]);
            } else {
                printf("I2C IOCTL Read Error: dev 0x%02X, reg 0x%02X (ret=%d)\r\n",
                       ioctl_arg->dev_addr, ioctl_arg->reg_addr, ret);
            }
            break;

        case I2C_IOCTL_WRITE_REG:
            ret = i2c->write_reg(ioctl_arg->dev_addr, ioctl_arg->reg_addr, ioctl_arg->reg_data[0]);
            if (ret == 0) {
                printf("I2C IOCTL Write Success: dev 0x%02X, reg 0x%02X, data 0x%02X\r\n",
                       ioctl_arg->dev_addr, ioctl_arg->reg_addr, ioctl_arg->reg_data[0]);
            } else {
                printf("I2C IOCTL Write Error: dev 0x%02X, reg 0x%02X (ret=%d)\r\n",
                       ioctl_arg->dev_addr, ioctl_arg->reg_addr, ret);
            }
            break;
			
		case I2C_IOCTL_READ_MULTI_REG:
            ret = i2c->read_reg_multi(ioctl_arg->dev_addr, ioctl_arg->reg_addr, &ioctl_arg->reg_data[0], ioctl_arg->len);
            if (ret == 0) {
				for (int i=0; i<ioctl_arg->len; i++)
					printf("I2C IOCTL Multi Read Success: dev 0x%02X, reg 0x%02X, data 0x%02X\r\n",
						ioctl_arg->dev_addr, ioctl_arg->reg_addr+i, ioctl_arg->reg_data[i]);
            } else {
                printf("I2C IOCTL Read Error: dev 0x%02X, reg 0x%02X (ret=%d)\r\n",
                       ioctl_arg->dev_addr, ioctl_arg->reg_addr, ret);
            }
            break;

        case I2C_IOCTL_WRITE_MULTI_REG:
            ret = i2c->write_reg_multi(ioctl_arg->dev_addr, ioctl_arg->reg_addr, &ioctl_arg->reg_data[0], ioctl_arg->len);
            if (ret == 0) {
				for (int i=0; i<ioctl_arg->len; i++)
					printf("I2C IOCTL Multi Write Success: dev 0x%02X, reg 0x%02X, data 0x%02X\r\n",
						ioctl_arg->dev_addr, ioctl_arg->reg_addr+i, ioctl_arg->reg_data[i]);
            } else {
                printf("I2C IOCTL Multi Write Error: dev 0x%02X, reg 0x%02X (ret=%d)\r\n",
                       ioctl_arg->dev_addr, ioctl_arg->reg_addr, ret);
            }
            break;

        default:
            printf("I2C IOCTL Error: invalid cmd 0x%02X\r\n", cmd);
            ret = -1;
            break;
    }

    return ret == 0 ? 0 : -1;
}

static int i2c_close(file_t *filp) {
    i2c_dev_t *i2c = (i2c_dev_t*)filp->priv_data;
    if (i2c == NULL || !i2c->is_init) {
        return 0;
    }

    i2c->is_init = 0;
    filp->priv_data = NULL;
    printf("I2C Close Success: driver reset\r\n");
    return 0;
}

static drv_ops_t i2c_drv_ops = {
    .name = I2C_DRV_NAME,
    .open = i2c_open,
    .read = i2c_read,
    .write = i2c_write,
    .ioctl = i2c_ioctl,
    .close = i2c_close,
};

void i2c_register(void)
{
    int ret = goldie_driver_register(&i2c_drv_ops);
    if (ret == 0) {
        printf("I2C Driver Register Success: '%s'\r\n", I2C_DRV_NAME);
    } else {
        printf("I2C Driver Register Error: ret=%d\r\n", ret);
		return;
    }
}

GOLDIE_INIT_CALL_(i2c_register);
#endif
