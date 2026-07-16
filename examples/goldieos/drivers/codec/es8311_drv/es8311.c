#include "es8311_config.h"

/**************************************************/
// Revision: 1.4.4.1.0425
// 说明01: 主要针对IIS时钟异常，IIC通信异常，如果没有IIS时钟的情况下需要IIC通信，需要IIS时钟复位，防止异常
/**************************************************/

/*************** 常量定义 ***************/
#define CHIPID                0x8311          // Read 0xFD/FE == 0x8311 判断IC型号是否为ES8311
#define CHIPID_REG0           0xFD
#define CHIPID_REG1           0xFE
#define STATEconfirm          0xFC            // 状态确认 读取STATEconfirm的寄存器值是否为0x70，确认IC正常工作状态

#define NORMAL_I2S            0x00
#define NORMAL_LJ             0x01
#define NORMAL_DSPA           0x03
#define NORMAL_DSPB           0x23

#define Format_Len24          0x00
#define Format_Len20          0x01
#define Format_Len18          0x02
#define Format_Len16          0x03
#define Format_Len32          0x04

#define VDDA_3V3              0x00
#define VDDA_1V8              0x01
#define MCLK_PIN              0x00
#define SCLK_PIN              0x01
/*************** 常量定义 ***************/

/*************** 配置选项 ***************/
#define MSMode_MasterSelOn    0               // 产品工作模式选择: 默认选择0为SlaveMode, 设为1选择MasterMode
#define Ratio                 64              // 实际Ratio=MCLK/LRCK比率，需要与实际时钟匹配

#define Format                NORMAL_I2S      // 数据格式选择, 需要与实际匹配
#define Format_Len            Format_Len16    // 数据长度选择, 需要与实际匹配
#define SCLK_DIV              4               // SCLK分频选择:(选择范围1~20), SCLK=MCLK/SCLK_DIV，具体对应关系请参考DS说明
#define SCLK_INV              0               // 默认读写方式为下降沿, 1为上升沿读写, 需要与实际匹配
#define MCLK_SOURCE           SCLK_PIN        // 是否硬件没有MCLK，需要SCLK代替MCLK

#define SCLK_STOP             0               // 主模式时有效, 是否SCLK在数据长度后不再产生时钟信号

#define ADCChannelSel         1               // 录音ADC输入通道选择: CH1(MIC1P/1N)或CH2(MIC2P/2N)

#define DACChannelSel         0               // 放音DAC输出通道选择: 默认选择0:L输出, 1:R输出
#define VDDA_VOLTAGE          VDDA_3V3        // 模拟电压选择为3V3或1V8, 需要与实际硬件匹配
#define ADC_PGA_GAIN          0xa             // ADC模拟增益:(选择范围0~10), 具体对应关系请参考DS说明

#define ADC_Volume            0xCA //190  //0xBF            // ADC数字音量:(选择范围0~255), 191:0DB, 0.5dB/Step

#define DAC_Volume            150//180             // DAC数字音量:(选择范围0~255), 191:0DB, 0.5dB/Step

#define Dmic_Selon            0               // DMIC选择: 默认选择关闭0, 设为1
#define ADC2DAC_Sel           0               // LOOP选择: 内部ADC数据送给DAC回环播放: 默认选择关闭0, 设为1
#define DACHPModeOn           0               // 耳机模式控制HP输出: 默认选择关闭0, 设为1
#define  ADC_AND_DACR  5

#define ES8312_MONONOUT       0
#define ES8312_FM             0

static unsigned char current_dac_volume = DAC_Volume;
static unsigned char current_adc_volume = ADC_Volume;
/*************** 配置选项 ***************/

/*
:V1000@//Version
:w45 00@//0x45寄存器写0x00值，按顺序写值
:w01 B0@
:w02 10@
:w03 10@
:w16 24@
:w04 10@
:w05 00@
:w09 00@//设置iis格式
:w0A 00@//设置iis格式
:w0B 00@
:w0C 00@
:w10 1F@
:w11 7F@
:w00 00@
:w01 9F@//
:w00 80@
:D00 01@//delay 1ms
:w0D 01@
:w14 10@
:w12 00@
:w13 10@
:w0E 02@
:w0F 44@
:w15 00@
:w1B 0A@
:w1C 6A@
:w37 08@
:w17 BF@
:w32 BF@
*/

static es8311_i2c_adapter priv_i2c_adapter = {0};

void ES8311_init_i2c_adapter(i2c_read_func readfunc, i2c_write_func writefunc)
{
    priv_i2c_adapter.i2c_reg_read = readfunc;
    priv_i2c_adapter.i2c_reg_write = writefunc;
}

int es8311_reg_write(char reg, char value)
{
    if (priv_i2c_adapter.i2c_reg_write) {
        return priv_i2c_adapter.i2c_reg_write(ES8311_ADDR, reg, value);
    }
    return 0;
}

int es8311_reg_read(char reg, char* value)
{
    if (priv_i2c_adapter.i2c_reg_read) {
        return priv_i2c_adapter.i2c_reg_read(ES8311_ADDR, reg, value);
    }
    return 0;
}

/* 140 ~ 190 */
void es8311_set_dac_volume(unsigned char volume)
{
    current_dac_volume = volume + VOLUME_ZERO_POINT;
    I2CWRNBYTE_CODEC(0x32, current_dac_volume);
}

void es8311_set_dac_mute(int mute)
{
    printf("es8311_set_dac_mute:%d  \r\n",mute);
    if(mute){
    	I2CWRNBYTE_CODEC(0x31, 0xc0);
    }else{
	I2CWRNBYTE_CODEC(0x31, 0x0);
   }
}

unsigned char es8311_get_dac_volume(void)
{
    return (current_dac_volume - VOLUME_ZERO_POINT);
}
	
void es8311_set_adc_volume(unsigned char volume)
{
    current_adc_volume = volume  + 124;
    I2CWRNBYTE_CODEC(0x17, current_adc_volume);  
}

unsigned char es8311_get_adc_volume(void)
{
    return (current_adc_volume -124);
}

void ES8311_read_chipid(void)
{
    char chipid[2] = {0};
    I2CREADBYTE_CODEC(CHIPID_REG0, chipid);
    I2CREADBYTE_CODEC(CHIPID_REG1, (chipid + 1));
    printf("es8311 chipid: %x %x \r\n", chipid[0], chipid[1]);
}
void ES8311_Codec(void);

void es8311_dump_reg(char start_addr, char end_addr)
{
    char i = start_addr;
    char value = 0;
    printf("es8311 dump start+++++++++++++++++++++++++ \r\n");
    for (; i < end_addr; i++) {
        I2CREADBYTE_CODEC(i, &value);
        printf("reg_addr[%x]:value[%x] \r\n", i, value);
    }
    printf("es8311 dump end--------------------------- \r\n");
    I2CREADBYTE_CODEC(0x12, &value);
    if(value != 0x28){
        printf("reset es8311 \r\n");
        ES8311_Codec();
    }
}

void ES8311_Codec_init_8k16b2ch(void)
{
    printf("ES8311_Codec_init_8k16b2ch--------------------------- \r\n");
    /* reset */
    I2CWRNBYTE_CODEC(0x45, 0x00);
    I2CWRNBYTE_CODEC(0x01, 0xb0);
    I2CWRNBYTE_CODEC(0x02, 0x10);
    // I2CWRNBYTE_CODEC(0x06, 0x23);
    /* config */
    I2CWRNBYTE_CODEC(0x03, 0x10);
    I2CWRNBYTE_CODEC(0x16, 0x24);
    I2CWRNBYTE_CODEC(0x04, 0x20);
    I2CWRNBYTE_CODEC(0x05, 0x00);
    /* i2s format */
    I2CWRNBYTE_CODEC(0x09, 0x0C);
    I2CWRNBYTE_CODEC(0x0A, 0x0C);
    I2CWRNBYTE_CODEC(0x0b, 0x00);
    I2CWRNBYTE_CODEC(0x0c, 0x00);
    I2CWRNBYTE_CODEC(0x10, 0x1f);
    I2CWRNBYTE_CODEC(0x11, 0x7f);
    I2CWRNBYTE_CODEC(0x00, 0x00);
    I2CWRNBYTE_CODEC(0x01, 0x9f);
    I2CWRNBYTE_CODEC(0x00, 0x80);
    DELAY_MS(5);
    I2CWRNBYTE_CODEC(0x0D, 0x01);
    I2CWRNBYTE_CODEC(0x14, 0x1A);
    I2CWRNBYTE_CODEC(0x12, 0x00);
    I2CWRNBYTE_CODEC(0x13, 0x10);
    I2CWRNBYTE_CODEC(0x0E, 0x02);
    I2CWRNBYTE_CODEC(0x0F, 0x44);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x1B, 0x0A);
    I2CWRNBYTE_CODEC(0x1C, 0x6A);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x17, 0xBF);
    I2CWRNBYTE_CODEC(0x32, 0xBF);
}
void ES8311_Codec(void) // 上电后执行一次初始化之后如果不再使用 可Standby/Powerdown关闭+resume恢复
{
    printf("ES8311_Codec \r\n");
    I2CWRNBYTE_CODEC(0x45, 0x00);
    I2CWRNBYTE_CODEC(0x02, 0x10);

    if (Ratio == 1536) { // Ratio=MCLK/LRCK=1536 12M288-8K
        I2CWRNBYTE_CODEC(0x02, 0xA0); // MCLK DIV=6
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 1500) { // Ratio=MCLK/LRCK=1500 12M-8K
        I2CWRNBYTE_CODEC(0x02, 0x90); // MCLK = 12/5*4 =9M6 1200Ratio
        I2CWRNBYTE_CODEC(0x03, 0x19); // 400
        I2CWRNBYTE_CODEC(0x16, 0x20);
        I2CWRNBYTE_CODEC(0x04, 0x19);
        I2CWRNBYTE_CODEC(0x05, 0x22);
    }
    if (Ratio == 1280) { // Ratio=MCLK/LRCK=1280 24M576-19K2
        I2CWRNBYTE_CODEC(0x02, 0x80); // MCLK DIV=5
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 1024) { // Ratio=MCLK/LRCK=1024 8M192-8K
        I2CWRNBYTE_CODEC(0x02, 0x60); // MCLK DIV=4
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 960) { // Ratio=MCLK/LRCK=960 15M36-16K
        I2CWRNBYTE_CODEC(0x02, 0x20); // MCLK = 15M36 /2=7M68 480Ratio
        I2CWRNBYTE_CODEC(0x03, 0x1E);
        I2CWRNBYTE_CODEC(0x16, 0x20);
        I2CWRNBYTE_CODEC(0x04, 0x1E);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 768) { // Ratio=MCLK/LRCK=768 12M288-16K 6M144-8K
        I2CWRNBYTE_CODEC(0x02, 0x40); // MCLK DIV=3
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 750) { // Ratio=MCLK/LRCK=750 12M-16K
        I2CWRNBYTE_CODEC(0x02, 0x9A); // MCLK = 12/5*8 =9M6 1200Ratio
        I2CWRNBYTE_CODEC(0x03, 0x19); // 400
        I2CWRNBYTE_CODEC(0x16, 0x21);
        I2CWRNBYTE_CODEC(0x04, 0x19);
        I2CWRNBYTE_CODEC(0x05, 0x22);
    }
    if (Ratio == 544) { // Ratio=MCLK/LRCK=544
        // 外部提供24M-44K1采样率会影响THDN，但是SNR没影响
        I2CWRNBYTE_CODEC(0x02, 0x20); // MCLK DIV=2
        I2CWRNBYTE_CODEC(0x03, 0x11);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x11);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 512) { // Ratio=MCLK/LRCK=512，24M576-48K，8M192-16K
        I2CWRNBYTE_CODEC(0x02, 0x20); // MCLK DIV=2
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 500) { // Ratio=MCLK/LRCK=500，24M-48K，8M-16K
        I2CWRNBYTE_CODEC(0x02, 0x92); // CLK =MCLK/5*4=400
        I2CWRNBYTE_CODEC(0x03, 0x19); // ADC 400 FS
        I2CWRNBYTE_CODEC(0x16, 0x21);
        I2CWRNBYTE_CODEC(0x04, 0x19); // DAC 400 FS
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 480) { // Ratio=MCLK/LRCK=480，15M36-32K
        I2CWRNBYTE_CODEC(0x02, 0x00); // MCLK = 480Ratio
        I2CWRNBYTE_CODEC(0x03, 0x1E);
        I2CWRNBYTE_CODEC(0x16, 0x20);
        I2CWRNBYTE_CODEC(0x04, 0x1E);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 384) { // Ratio=MCLK/LRCK=384，12M288-32K，6M144-16K，3M072-8K
        I2CWRNBYTE_CODEC(0x02, 0x00); //
        I2CWRNBYTE_CODEC(0x03, 0x18); // 384
        I2CWRNBYTE_CODEC(0x16, 0x21); // 384
        I2CWRNBYTE_CODEC(0x04, 0x18); // 384
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 320) { // Ratio=MCLK/LRCK=320，5M12-16K
        I2CWRNBYTE_CODEC(0x02, 0x92); // 320 /5 *4 = 256Ratio
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 272) { // Ratio=MCLK/LRCK=272
        // 外部提供12M-44K1采样率会影响THDN，但是SNR没影响
        I2CWRNBYTE_CODEC(0x02, 0x00); // MCLK DIV=1
        I2CWRNBYTE_CODEC(0x03, 0x11);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x11);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 256) { // Ratio=MCLK/LRCK=256，12M288-48K，4M096-16K; 2M048-8K
        I2CWRNBYTE_CODEC(0x02, 0x00); // MCLK DIV=1
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
#if 0 // 8K采样率特殊配置
        I2CWRNBYTE_CODEC(0x02, 0x09); // MCLK DIV=1
        I2CWRNBYTE_CODEC(0x03, 0x20);
        I2CWRNBYTE_CODEC(0x16, 0x20);
        I2CWRNBYTE_CODEC(0x04, 0x40);
        I2CWRNBYTE_CODEC(0x05, 0x00);
#endif
    }
    if (Ratio == 250) { // Ratio=MCLK/LRCK=250，12M-48K，4M-16K
        I2CWRNBYTE_CODEC(0x02, 0x9A); // CLK =MCLK/5*8=400
        I2CWRNBYTE_CODEC(0x03, 0x19); // ADC 400 FS
        I2CWRNBYTE_CODEC(0x16, 0x21);
        I2CWRNBYTE_CODEC(0x04, 0x19); // DAC 400 FS
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 192) { // Ratio=MCLK/LRCK=192，12M288-64K，6M144-32K，3M072-16K; 1M536-8K
        I2CWRNBYTE_CODEC(0x02, 0x51); // MCLK = MCLK/3*4 =9M6 256Ratio
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 128) { // Ratio=MCLK/LRCK=128，6M144-48K，2M048-16K; 1M024-8K
        I2CWRNBYTE_CODEC(0x02, 0x0A);
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 64) { // Ratio=MCLK/LRCK=64，3M072-48K，1M024-16K; 512k-8K
        /* 当512K/8K时，DVDD需要为1V8，否则无法正常工作 */
        I2CWRNBYTE_CODEC(0x02, 0x10);
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }
    if (Ratio == 48) { // Ratio=MCLK/LRCK=64，3M072-48K，1M024-16K; 512k-8K
        // 不支持8K 48FS时钟频率
        /* 当48K时，DVDD需要为3V3，当LRCK=16K时，DVDD需要=1V8，否则无法正常工作 */
        I2CWRNBYTE_CODEC(0x02, 0x1A); // 48FS*8=384
        I2CWRNBYTE_CODEC(0x05, 0x21); // 128FS
        I2CWRNBYTE_CODEC(0x03, 0x20); // 128FS
        I2CWRNBYTE_CODEC(0x16, 0x20);
        I2CWRNBYTE_CODEC(0x04, 0x30); // 192FS
    }
    if (Ratio == 32) { // Ratio=MCLK/LRCK=32，1M536-48K，512K-16K;
        // 不支持8K 32FS时钟频率
        /* 当512K/16K以及768K/24K时，DVDD需要为1V8，否则无法正常工作 */
        I2CWRNBYTE_CODEC(0x02, 0x1A);
        I2CWRNBYTE_CODEC(0x03, 0x10);
        I2CWRNBYTE_CODEC(0x16, 0x24);
        I2CWRNBYTE_CODEC(0x04, 0x20);
        I2CWRNBYTE_CODEC(0x05, 0x00);
    }

    I2CWRNBYTE_CODEC(0x06, (SCLK_STOP << 6) + (SCLK_INV << 5) + SCLK_DIV - 1); // SCLK DIV
    I2CWRNBYTE_CODEC(0x07, ((Ratio - 1) >> 8)); // LRCK DIV
    I2CWRNBYTE_CODEC(0x08, ((Ratio - 1) & 0xFF)); // LRCK DIV

    I2CWRNBYTE_CODEC(0x09, (DACChannelSel << 7) + Format + (Format_Len << 2));
    I2CWRNBYTE_CODEC(0x0A, Format + (Format_Len << 2));

    I2CWRNBYTE_CODEC(0x0B, 0x00);
    I2CWRNBYTE_CODEC(0x0C, 0x00);

    I2CWRNBYTE_CODEC(0x10, (0x1C * DACHPModeOn) + (0x5E * VDDA_VOLTAGE) + 0x03); // 3V3对应03，1V8对应61

    I2CWRNBYTE_CODEC(0x11, 0x7F);

    // I2CWRNBYTE_CODEC(0x01, 0x3F + (MCLK_SOURCE << 7)); // 内部选择BCLK只作为输入，外部选择MCLK
    I2CWRNBYTE_CODEC(0x01, 0xbf);
    I2CWRNBYTE_CODEC(0x00, 0x80 + (MSMode_MasterSelOn << 6)); // Slave Mode

    DELAY_MS(10);

    I2CWRNBYTE_CODEC(0x0D, 0x01);

    I2CWRNBYTE_CODEC(0x14, (Dmic_Selon << 6) + (ADCChannelSel << 4) + ADC_PGA_GAIN); // 选择CH1输入+30DB GAIN

    I2CWRNBYTE_CODEC(0x12, 0x28);

    I2CWRNBYTE_CODEC(0x13, 0x00 + (DACHPModeOn << 4));
    /*****************************************/
    // ES8311测试，ES8312测试
    if (ES8312_MONONOUT == 1) {
        I2CWRNBYTE_CODEC(0x12, 0xC8); // ES8312测试MONOOUT
    }
    if (ES8312_FM == 1) {
        I2CWRNBYTE_CODEC(0x13, 0xA0); // ES8312测试FM输入
        // 注意如果需要HPOUT的话 13改为B0
    }
    /*****************************************/
    I2CWRNBYTE_CODEC(0x0E, 0x02);
    I2CWRNBYTE_CODEC(0x0F, 0x44);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x1B, 0x0A);
    I2CWRNBYTE_CODEC(0x1C, 0x6A);
    I2CWRNBYTE_CODEC(0x37, 0x08);
/*	
    I2CWRNBYTE_CODEC(0x44, (ADC2DAC_Sel << 7));
*/
    I2CWRNBYTE_CODEC(0x44, (ADC_AND_DACR << 4));
    I2CWRNBYTE_CODEC(0x17, ADC_Volume);
    I2CWRNBYTE_CODEC(0x32, DAC_Volume);
    DELAY_MS(100);	

}

void ES8311_PowerDown(void) // 功耗0uA模式
{
    printf("ES8311_PowerDown \r\n");
    I2CWRNBYTE_CODEC(0x32, 0x00);
    I2CWRNBYTE_CODEC(0x17, 0x00);
    I2CWRNBYTE_CODEC(0x0E, 0x5F);
    I2CWRNBYTE_CODEC(0x12, 0x02);
    I2CWRNBYTE_CODEC(0x14, 0x00);
    I2CWRNBYTE_CODEC(0x0D, 0xF9);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x02, 0x10);
    I2CWRNBYTE_CODEC(0x00, 0x00);
    I2CWRNBYTE_CODEC(0x00, 0x1F);
    I2CWRNBYTE_CODEC(0x01, 0x30);
    I2CWRNBYTE_CODEC(0x01, 0x00);
    I2CWRNBYTE_CODEC(0x45, 0x00);
    I2CWRNBYTE_CODEC(0x0D, 0xFC);
    I2CWRNBYTE_CODEC(0x02, 0x00);
    I2CWRNBYTE_CODEC(0x10, 0x03);
}

void ES8311_Standby(void) // 待机模式(300UA，无开机POP)--对应ES8311_Resume(void)恢复
{
    printf("ES8311_Standby \r\n");
    I2CWRNBYTE_CODEC(0x32, 0x00);
    I2CWRNBYTE_CODEC(0x17, 0x00);
    I2CWRNBYTE_CODEC(0x0E, 0x5F); // FF
    I2CWRNBYTE_CODEC(0x12, 0x02);
    I2CWRNBYTE_CODEC(0x14, 0x00);
    I2CWRNBYTE_CODEC(0x0D, 0xF9);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x02, 0x10);
    I2CWRNBYTE_CODEC(0x00, 0x00);
    I2CWRNBYTE_CODEC(0x00, 0x1F);
    I2CWRNBYTE_CODEC(0x01, 0x30);
    I2CWRNBYTE_CODEC(0x01, 0x00);
    I2CWRNBYTE_CODEC(0x45, 0x00);
    I2CWRNBYTE_CODEC(0x10, (0x60 * VDDA_VOLTAGE) + 0x03);
}

void ES8311_Resume(void) // 恢复模式(未掉电)--对应ES8311_Standby(void)以及ES8311_PowerDown
{
    printf("ES8311_Resume \r\n");
    I2CWRNBYTE_CODEC(0x0D, 0x01);
    I2CWRNBYTE_CODEC(0x45, 0x00);
    I2CWRNBYTE_CODEC(0x10, (0x1C * DACHPModeOn) + (0x5E * VDDA_VOLTAGE) + 0x03); // 3V3对应03，1V8对应61
    I2CWRNBYTE_CODEC(0x01, 0x3F + (MCLK_SOURCE << 7)); // 内部选择BCLK只作为输入，外部选择MCLK
    I2CWRNBYTE_CODEC(0x00, 0x80);
    DELAY_MS(1);
    I2CWRNBYTE_CODEC(0x0D, 0x01);
    I2CWRNBYTE_CODEC(0x02, 0x00);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x15, 0x40);
    I2CWRNBYTE_CODEC(0x14, (Dmic_Selon << 6) + (ADCChannelSel << 4) + ADC_PGA_GAIN); // 选择CH1输入+30DB GAIN
    I2CWRNBYTE_CODEC(0x12, 0x00);
    I2CWRNBYTE_CODEC(0x0E, 0x00);
    I2CWRNBYTE_CODEC(0x17, ADC_Volume);
    I2CWRNBYTE_CODEC(0x32, DAC_Volume);
}

void ES8311_Reset(void) // 不提供IIS时钟时需要复位
{
    printf("ES8311_Reset \r\n");
    I2CWRNBYTE_CODEC(0x00, 0x1F);
    I2CWRNBYTE_CODEC(0x00, 0x80);
    DELAY_MS(10); // DELAY_1MS
    I2CWRNBYTE_CODEC(0x0D, 0x01);
}

void ES8311_ADC_Standby(void) // ADC待机模式--对应ES8311_ADC_Resume
{
    printf("ES8311_ADC_Standby \r\n");
    I2CWRNBYTE_CODEC(0x17, 0x00);
    I2CWRNBYTE_CODEC(0x0A, 0x40);
    I2CWRNBYTE_CODEC(0x0E, 0x5F);
    I2CWRNBYTE_CODEC(0x14, 0x00);
    I2CWRNBYTE_CODEC(0x0D, 0x31);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x00, 0x82);
    I2CWRNBYTE_CODEC(0x01, 0x35);
}

void ES8311_ADC_Resume(void) // ADC恢复模式--对应ES8311_ADC_Standby
{
    printf("ES8311_ADC_Resume \r\n");
    I2CWRNBYTE_CODEC(0x0D, 0x01);
    I2CWRNBYTE_CODEC(0x01, 0x3F + (MCLK_SOURCE << 7)); // 内部选择BCLK只作为输入，外部选择MCLK
    I2CWRNBYTE_CODEC(0x00, 0x80);
    DELAY_MS(1);
    I2CWRNBYTE_CODEC(0x0D, 0x01);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x14, (Dmic_Selon << 6) + (ADCChannelSel << 4) + ADC_PGA_GAIN); // 选择CH1输入+30DB GAIN
    I2CWRNBYTE_CODEC(0x0E, 0x02);
    I2CWRNBYTE_CODEC(0x17, 0xBF);
    DELAY_MS(50);
    I2CWRNBYTE_CODEC(0x0A, 0x00);
}

void ES8311_DAC_Standby(void) // DAC待机模式--对应ES8311_DAC_Resume
{
    printf("ES8311_DAC_Standby \r\n");
    I2CWRNBYTE_CODEC(0x32, 0x00);
    I2CWRNBYTE_CODEC(0x0E, 0x0F);
    I2CWRNBYTE_CODEC(0x12, 0x02);
    I2CWRNBYTE_CODEC(0x0D, 0x09);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x00, 0x81);
    I2CWRNBYTE_CODEC(0x01, 0x3A);
}

void ES8311_DAC_Resume(void) // DAC恢复模式--对应ES8311_DAC_Standby
{
    printf("ES8311_DAC_Resume \r\n");
    I2CWRNBYTE_CODEC(0x0D, 0x01);
    I2CWRNBYTE_CODEC(0x01, 0x3F + (MCLK_SOURCE << 7)); // 内部选择BCLK只作为输入，外部选择MCLK
    I2CWRNBYTE_CODEC(0x00, 0x80);
    DELAY_MS(1);
    I2CWRNBYTE_CODEC(0x0D, 0x01);
    I2CWRNBYTE_CODEC(0x37, 0x08);
    I2CWRNBYTE_CODEC(0x15, 0x00);
    I2CWRNBYTE_CODEC(0x12, 0x00);
    I2CWRNBYTE_CODEC(0x0E, 0x02);
    I2CWRNBYTE_CODEC(0x32, 0xBF);
}
