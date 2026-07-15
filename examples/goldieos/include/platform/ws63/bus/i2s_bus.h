#ifndef _INCLUDE_I2S_H_
#define _INCLUDE_I2S_H_
#include "ws63_liteos.h"

#define CONFIG_DATA_LEN_MAX             128

typedef struct i2s_rx_data {
    uint32_t left_buff[CONFIG_DATA_LEN_MAX];    /*!< @if Eng Left data.
                                                     @else   左声道数据。 @endif */
    uint32_t right_buff[CONFIG_DATA_LEN_MAX];   /*!< @if Eng Right data.
                                                     @else   右声道数据。 @endif */
    uint32_t length;                            /*!< @if Eng Data length.
                                                     @else   数据长度。 @endif */
} i2s_rx_data_t;

/**
 * @if Eng
 * @brief  Definition of I2S TX data structure.
 * @else
 * @brief  I2S TX 传输结构体。
 * @endif
 */
typedef struct i2s_tx_data {
    uint32_t *left_buff;                        /*!< @if Eng Data send through tx left FIFO.
                                                     @else   通过TX 左FIFO发送的数据。 @endif */
    uint32_t *right_buff;                       /*!< @if Eng Data send through tx right FIFO.
                                                     @else   通过TX 右FIFO发送的数据。 @endif */
    uint32_t length;                            /*!< @if Eng Bytes of data need to send.
                                                     @else   发送数据的个数。 @endif */
} i2s_tx_data_t;

/**
 * @if Eng
 * @brief  Definition of I2S configuration.
 * @else
 * @brief  I2S 配置定义。
 * @endif
 */
typedef struct i2s_config {
    uint8_t drive_mode;                         /*!< @if Eng I2S divice modes:
                                                 *           - 0: SLAVE
                                                 *           - 1: MASTER
                                                 *   @else   I2S 设备模式：
                                                 *           - 0: 从模式
                                                 *           - 1: 主模式
                                                 *   @endif */
    uint8_t transfer_mode;                      /*!< @if Eng I2S transmission path modes:
                                                 *           - 0: Standard mode
                                                 *           - 1: Multichannel mode
                                                 *   @else   I2S 传输路径模式：
                                                 *           - 0: 标准模式
                                                 *           - 1: 多路模式
                                                 *   @endif */
    uint8_t data_width;                         /*!< @if Eng I2S data width:
                                                 *           - 0: RESERVED
                                                 *           - 1: 16 Bits
                                                 *           - 2: 18 Bits
                                                 *           - 3: 20 Bits
                                                 *           - 4: 24 Bits
                                                 *           - 5: 32 Bits
                                                 *   @else   I2S 数据宽度：
                                                 *           - 0: 保留
                                                 *           - 1: 16位
                                                 *           - 2: 18位
                                                 *           - 3: 20位
                                                 *           - 4: 24位
                                                 *           - 5: 32位
                                                 *   @endif */
    uint8_t channels_num;                       /*!< @if Eng I2S transmission Channels Number:
                                                 *           - 0: 2 Channels
                                                 *           - 1: 4 Channels
                                                 *           - 2: 8 Channels
                                                 *           - 3: 16 Channels
                                                 *   @else   I2S 传输通道数：
                                                 *           - 0: 2通道
                                                 *           - 1: 4通道
                                                 *           - 2: 8通道
                                                 *           - 3: 16通道
                                                 *   @endif */
    uint8_t timing;                             /*!< @if Eng I2S timing mode:
                                                 *           - 0: Standard timing mode
                                                 *           - 1: User-defined timing mode
                                                 *   @else   I2S 时序模式：
                                                 *           - 0: 标准时序模式
                                                 *           - 1: 自定义时序模式
                                                 *   @endif */
    uint8_t clk_edge;                           /*!< @if Eng I2S clock edge mode:
                                                 *           - 0: Falling edge
                                                 *           - 1: Rising edge
                                                 *   @else   I2S 时钟边沿模式：
                                                 *           - 0: 下降沿
                                                 *           - 1: 上升沿
                                                 *   @endif */
    uint8_t div_number;                         /*!< @if Eng Div number, see @ref i2s_config.data_width.
                                                     @else   分频系数，见 @ref i2s_config.data_width 成员。 @endif */
    uint8_t number_of_channels;                 /*!< @if Eng Number of channels, see @ref i2s_config.channels_num.
                                                     @else   通道数，见 @ref i2s_config.channels_num 成员。 @endif */
} i2s_config_t;

typedef void (*i2s_callback_t)(uint32_t *left_buff, uint32_t *right_buff, uint32_t length);

typedef enum {
    SIO_BUS_0,
    I2S_MAX_NUMBER,
    SIO_NONE = I2S_MAX_NUMBER,
} sio_bus_t;

typedef struct i2s_dma_attr {
    bool tx_dma_enable;                     /*!< @if Eng false: tx not use dma @ref uapi_i2s_write can be used. \n
                                                     true:  tx use dma @ref uapi_i2s_write_by_dma can be used.
                                             @else   false: TX没有使用DMA，使用 @ref uapi_i2s_write 发送数据 \n
                                                     true:  TX使用DMA，使用 @ref uapi_i2s_write_by_dma 发送数据 @endif */
    uint8_t tx_int_threshold;               /*!< @if Eng i2s tx fifo level to trigger interrupt.
                                             @else 触发中断的txfifo水线 @endif */
    bool rx_dma_enable;                     /*!< @if Eng false: rx not use dma @ref uapi_i2s_write can be used. \n
                                                     true:  rx use dma @ref uapi_i2s_write_by_dma can be used.
                                             @else   false: RX没有使用DMA，使用 @ref uapi_i2s_write 发送数据 \n
                                                     true:  RX使用DMA，使用 @ref uapi_i2s_write_by_dma 发送数据 @endif */
    uint8_t rx_int_threshold;               /*!< @if Eng i2s rx fifo level to trigger interrupt.
                                             @else 触发中断的rxfifo水线 @endif */
} i2s_dma_attr_t;

typedef struct i2s_dma_config {
    uint8_t src_width;          /*!< @if Eng Transfer data width of the source.
                                 *           - 0: 1byte
                                 *           - 1: 2byte
                                 *           - 2: 4byte
                                 *   @else   源端传输数据宽度 \n
                                 *           - 0: 1字节
                                 *           - 1: 2字节
                                 *           - 2: 4字节
                                 *   @endif */
    uint8_t dest_width;         /*!< @if Eng Transfer data width of the destination.
                                 *            - 0: 1byte
                                 *            - 1: 2byte
                                 *            - 2: 4byte
                                 *   @else   目的端传输数据宽度 \n
                                 *           - 0: 1字节
                                 *           - 1: 2字节
                                 *           - 2: 4字节
                                 *   @endif */
    uint8_t burst_length;       /*!< @if Eng Number of data items, to be written to the destination every time
                                 *           a destination burst transaction request is made from
                                 *           either the corresponding hardware or software handshaking interface.
                                 *           - 0: burst length is 1
                                 *           - 1: burst length is 4
                                 *           - 2: burst length is 8
                                 *           - 3: burst length is 16
                                 *   @else   每次从相应的硬件或软件握手接口发出目的burst请求时,要写入目的端数据量
                                 *           - 0: burst长度是1
                                 *           - 1: burst长度是4
                                 *           - 2: burst长度是8
                                 *           - 3: burst长度是16
                                 *   @endif */
    uint8_t priority;           /*!< @if Eng Transfer channel priority(Minimum: 0 and Maximum: 3).
                                 *   @else   传输通道优先级(最小为0以及最大为3)  @endif */
} i2s_dma_config_t;

typedef enum hal_sio_driver_mode {
    SLAVE,                                      /*!< @if Eng SLAVE.
                                                     @else   从模式。 @endif */
    MASTER                                      /*!< @if Eng MASTER.
                                                     @else   主模式。 @endif */
} hal_sio_driver_mode_t;

typedef enum {
    STD_MODE = 0,                               /*!< @if Eng Standard mode.
                                                     @else   标准模式。 @endif */
    MULTICHANNEL_MODE                           /*!< @if Eng Multichannel mode.
                                                     @else   多路模式。 @endif */
} hal_sio_transfer_mode_t;

typedef enum hal_sio_data_width {
    RESERVED,                                   /*!< @if Eng RESERVED.
                                                     @else   保留。 @endif */
    SIXTEEN_BIT,                                /*!< @if Eng 16 BITS.
                                                     @else   16位。 @endif */
    EIGHTEEN_BIT,                               /*!< @if Eng 18 BITS.
                                                     @else   18位。 @endif */
    TWENTY_BIT,                                 /*!< @if Eng 20 BITS.
                                                     @else   20位。 @endif */
    TWENTY_FOUR_BIT,                            /*!< @if Eng 24 BITS.
                                                     @else   24位。 @endif */
    THIRTY_TWO_BIT                              /*!< @if Eng 32 BITS.
                                                     @else   32位。 @endif */
} hal_sio_data_width_t;

typedef enum hal_sio_channel_number {
    TWO_CH,                                     /*!< @if Eng 2 Channels.
                                                     @else   2通道。 @endif */
    FOUR_CH,                                    /*!< @if Eng 4 Channels.
                                                     @else   4通道。 @endif */
    EIGHT_CH,                                   /*!< @if Eng 8 Channels.
                                                     @else   8通道。 @endif */
    SIXTEEN_CH                                  /*!< @if Eng 16 Channels.
                                                     @else   16通道。 @endif */
} hal_sio_channel_number_t;

typedef enum hal_sio_iming_mode {
    PCM_STD_MODE = 0,                           /*!< @if Eng PCM standard timing mode.
                                                     @else   PCM标准时序模式。 @endif */
    PCM_UD_MODE,                                /*!< @if Eng PCM user-defined timing mode.
                                                     @else   PCM自定义时序模式。 @endif */
    NONE_TIMING_MODE
} hal_sio_timing_mode_t;

typedef enum hal_sio_clk_edge {
    FALLING_EDGE = 0,                           /*!< @if Eng Falling edge.
                                                     @else   下降沿。 @endif */
    RISING_EDGE,                                /*!< @if Eng Rising edge
                                                     @else   上升沿。 @endif */
    NONE_EDGE
} hal_sio_clk_edge_t;



typedef void (*dma_transfer_cb_t)(uint8_t intr, uint8_t channel, uintptr_t arg);
extern errcode_t uapi_i2s_deinit(sio_bus_t bus);
extern errcode_t uapi_i2s_init(sio_bus_t bus, i2s_callback_t callback);
extern void sio_porting_i2s_pinmux(void);
extern errcode_t uapi_i2s_set_config(sio_bus_t bus, const i2s_config_t *config);
extern int32_t uapi_i2s_dma_config(sio_bus_t bus, i2s_dma_attr_t *i2s_dma_cfg);
extern void enable_i2s_clock(int bus);
extern void disable_i2s_clock(int bus);
extern int32_t uapi_i2s_read_write_by_dma(sio_bus_t bus, const void *buffer_tx, const void *buffer_rx, uint32_t length,
                                                                                                                i2s_dma_config_t *dma_cfg, dma_transfer_cb_t callback);

#endif
