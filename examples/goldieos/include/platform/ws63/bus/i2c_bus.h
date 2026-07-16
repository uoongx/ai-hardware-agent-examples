#ifndef _INCLUDE_I2C_BUS_H
#define _INCLUDE_I2C_BUS_H
#include "ws63_liteos.h"

typedef struct i2c_data {
    uint8_t *send_buf;              /*!< @if Eng Send buffer pointer.
                                         @else   发送数据的buffer指针。  @endif */
    uint32_t send_len;              /*!< @if Eng Send buffer len.
                                         @else   发送数据的buffer长度。  @endif */
    uint8_t *receive_buf;           /*!< @if Eng Receive buffer pointer.
                                         @else   接收数据的buffer指针。  @endif */
    uint32_t receive_len;           /*!< @if Eng Receive buffer pointer.
                                         @else   接收数据的buffer长度。  @endif */
} i2c_data_t;


typedef enum {
    I2C_BUS_0,               // !< I2C0
    I2C_BUS_1,               // !< I2C1
    I2C_BUS_2,               // !< I2C2
    I2C_BUS_3 = 3,
    I2C_BUS_4 = 4,
    I2C_BUS_MAX_NUMBER,
    I2C_BUS_NONE = I2C_BUS_MAX_NUMBER,
} i2c_bus_t;

extern errcode_t uapi_i2c_master_write(i2c_bus_t bus, uint16_t dev_addr, i2c_data_t *data);
extern errcode_t uapi_i2c_master_read(i2c_bus_t bus, uint16_t dev_addr, i2c_data_t *data);
extern errcode_t uapi_i2c_master_init(i2c_bus_t bus, uint32_t baudrate, uint8_t hscode);

#endif
