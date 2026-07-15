#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_
#include "goldie_osal.h"
#define ES8311_ADDR    0X18   //0X30 <<1
#define VOLUME_ZERO_POINT (70)
#define DELAY_MS(ms)   goldie_msleep(ms)

typedef struct es8311_i2c_adapter {
    int (*i2c_reg_read)(char,char,char *);
    int (*i2c_reg_write)(char,char,char);
} es8311_i2c_adapter;

typedef int (*i2c_write_func)(char addr,char reg, char data);
typedef int (*i2c_read_func)(char addr,char reg, char* data);

int es8311_reg_write(char reg,char value);
int es8311_reg_read(char reg,char* value);

#define I2CWRNBYTE_CODEC(addr,value)  es8311_reg_write(addr,value)
#define I2CREADBYTE_CODEC(addr,value)  es8311_reg_read(addr,value)

void ES8311_init_i2c_adapter(i2c_read_func readfunc,i2c_write_func writefunc);

void ES8311_read_chipid(void);
void ES8311_Codec_init_8k16b2ch(void);
void es8311_dump_reg(char start_addr,char end_addr);
void ES8311_Codec(void);
void ES8311_PowerDown(void);
void ES8311_Standby(void);
void ES8311_Resume(void);
void ES8311_Reset(void);
void ES8311_ADC_Standby(void);
void ES8311_ADC_Resume(void);
void ES8311_DAC_Standby(void);
void ES8311_DAC_Resume(void);
void es8311_set_dac_volume(unsigned char volume);
unsigned char es8311_get_dac_volume(void);
void es8311_set_adc_volume(unsigned char volume);
unsigned char es8311_get_adc_volume(void);
void es8311_set_dac_mute(int mute);

#endif
