#ifdef SUPPORT_PCF8563_RTC
#include "service_manager.h"
#include "driver_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif
#include "app_manager.h"
// PCF8563 I2C地址（写/读）
#define PCF8563_I2C_ADDR 0x51
// PCF8563 时间寄存器地址
#define PCF8563_REG_SEC        0x02  // 秒（含VL位）
#define PCF8563_REG_MIN        0x03  // 分
#define PCF8563_REG_HOUR       0x04  // 时
#define PCF8563_REG_DATE       0x05  // 日
#define PCF8563_REG_WEEK       0x06  // 星期
#define PCF8563_REG_MON_CENT   0x07  // 月+世纪
#define PCF8563_REG_YEAR       0x08  // 年

// 时间结构体（存储转换后的十进制数据）
typedef struct {
    uint8_t sec;    // 0~59
    uint8_t min;    // 0~59
    uint8_t hour;   // 0~23
    uint8_t date;   // 1~31
    uint8_t week;   // 0~6（0=周日）
    uint8_t mon;    // 1~12
    uint16_t year;  // 2000~2099或1900~1999
    uint8_t vl_flag;// 1=数据不可靠，0=可靠
} RTC_TimeTypeDef;

// 全局变量：I2C驱动句柄、RTC时间缓存
static I2cDrv* rtc_i2c_drv = NULL;
static RTC_TimeTypeDef rtc_current_time;
// BCD码转十进制
static uint8_t BCD2DEC(uint8_t bcd) {
    return (bcd / 16 * 10) + (bcd % 16);
}
// 十进制转BCD码
static uint8_t DEC2BCD(uint8_t dec) {
    return (dec / 10) << 4 | (dec % 10);
}

static int rtc_drv_init_flag  = 0;
/**
 * @brief 读取PCF8563单个寄存器
 * @param reg_addr 寄存器地址
 * @return 读取到的字节数据，-1表示失败
 */
static int pcf8563_read_reg(uint8_t reg_addr) {
	if (rtc_i2c_drv == NULL) {
        printf("PCF8563 read fail: I2C drv is NULL!\r\n");
        return -1;
    }
    char read_data = 0;
    int ret = rtc_i2c_drv->i2c_reg_read(PCF8563_I2C_ADDR, reg_addr, &read_data);
    if (ret != 0) {
        printf("PCF8563 read reg 0x%02X failed! ret=%d\r\n", reg_addr, ret);
        return -1;
    }
    return (int)read_data;
}
/**
 * @brief 写入PCF8563单个寄存器
 * @param reg_addr 寄存器地址
 * @param data 要写入的字节数据
 * @return 0=成功，-1=失败
 */
static int pcf8563_write_reg(uint8_t reg_addr, uint8_t data) {
    if (rtc_i2c_drv == NULL) return -1;
    // 直接写寄存器+数据（PCF8563写操作时序）
    int ret = rtc_i2c_drv->i2c_reg_write(PCF8563_I2C_ADDR, reg_addr, data);
    if (ret != 0) {
        printf("PCF8563 write reg 0x%02X failed! ret=%d\r\n", reg_addr, ret);
        return -1;
    }
    return 0;
}

#if 0
/**
 * @brief 读取PCF8563时间（使用多字节读取）
 * @param tm_time 输出参数，存储读取到的时间
 * @return 0=成功，-1=失败
 */
static int pcf8563_get_time(struct tm *tm_time)
{
    if (tm_time == NULL || rtc_i2c_drv == NULL) return -1;

    uint8_t buf[7];  // 用于存放连续7个寄存器的值: SEC, MIN, HOUR, DATE, WEEK, MON_CENT, YEAR
    int ret = rtc_i2c_drv->i2c_reg_read_multi(PCF8563_I2C_ADDR, PCF8563_REG_SEC, buf, 7);
    if (ret != 0) {
        printf("PCF8563 multi read failed! ret=%d\r\n", ret);
        return -1;
    }

    RTC_TimeTypeDef rtc_time;

    // 解析秒寄存器 (含VL位)
    rtc_time.vl_flag = (buf[0] >> 7) & 0x01;
    rtc_time.sec = BCD2DEC(buf[0] & 0x7F);

    // 分
    rtc_time.min = BCD2DEC(buf[1]);

    // 时 (只取低6位)
    rtc_time.hour = BCD2DEC(buf[2] & 0x3F);

    // 日 (只取低6位)
    rtc_time.date = BCD2DEC(buf[3] & 0x3F);

    // 星期 (只取低3位)
    rtc_time.week = buf[4] & 0x07;

    // 月+世纪
    rtc_time.mon = BCD2DEC(buf[5] & 0x1F);
    uint8_t cent_bit = (buf[5] >> 7) & 0x01;

    // 年
    uint8_t year_bcd = BCD2DEC(buf[6]);
    rtc_time.year = cent_bit ? (1900 + year_bcd) : (2000 + year_bcd);

    // 转换为 struct tm
    tm_time->tm_sec  = rtc_time.sec;
    tm_time->tm_min  = rtc_time.min;
    tm_time->tm_hour = rtc_time.hour;
    tm_time->tm_mday = rtc_time.date;
    tm_time->tm_mon  = rtc_time.mon - 1;          // 1~12 → 0~11
    tm_time->tm_year = rtc_time.year - 1900;       // 实际年份 → 自1900起
    tm_time->tm_wday = rtc_time.week;
    tm_time->tm_yday = 0;
    tm_time->tm_isdst = 0;

    // 可根据VL位返回状态（此处保持原样返回0）
    return 0;
}

/**
 * @brief 写入PCF8563时间（使用多字节写入）
 * @param tm_time 要写入的时间
 * @return 0=成功，-1=失败
 */
static int pcf8563_set_time(const struct tm *tm_time)
{
    if (tm_time == NULL || rtc_i2c_drv == NULL) return -1;

    // 将 struct tm 转换为内部格式
    RTC_TimeTypeDef rtc_time;
    rtc_time.sec  = tm_time->tm_sec;
    rtc_time.min  = tm_time->tm_min;
    rtc_time.hour = tm_time->tm_hour;
    rtc_time.date = tm_time->tm_mday;
    rtc_time.week = tm_time->tm_wday;
    rtc_time.mon  = tm_time->tm_mon + 1;
    rtc_time.year = tm_time->tm_year + 1900;
    rtc_time.vl_flag = 0;   // 写入时强制时钟有效

    // 构造连续7字节的写入缓冲区
    int ret = 0;
    uint8_t cent_bit = (rtc_time.year >= 2000) ? 0 : 1;
    ret = pcf8563_write_reg(PCF8563_REG_SEC, DEC2BCD(rtc_time.sec) & 0x7F);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_MIN, DEC2BCD(rtc_time.min));
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_HOUR, DEC2BCD(rtc_time.hour) & 0x3F);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_DATE, DEC2BCD(rtc_time.date) & 0x3F);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_WEEK, rtc_time.week & 0x07);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_MON_CENT, (cent_bit << 7) | (DEC2BCD(rtc_time.mon) & 0x1F));
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_YEAR, DEC2BCD(rtc_time.year % 100));
    if (ret != 0) return -1;


    return 0;
}
#else
/**
 * @brief 读取PCF8563完整时间（转换为十进制）
 * @return 0=成功，-1=失败
 */
 
static int pcf8563_get_time(struct tm *tm_time)
{
    if (tm_time == NULL || rtc_i2c_drv == NULL) return -1;

    RTC_TimeTypeDef rtc_time;   // 临时变量，用于存放从芯片读出的原始数据

    int reg_val;
    // 读取秒（含VL）
    reg_val = pcf8563_read_reg(PCF8563_REG_SEC);
    if (reg_val < 0) return -1;
    rtc_time.vl_flag = (reg_val >> 7) & 0x01;
    rtc_time.sec = BCD2DEC(reg_val & 0x7F);

    // 读取分、时、日、星期
    reg_val = pcf8563_read_reg(PCF8563_REG_MIN);
    if (reg_val < 0) return -1;
    rtc_time.min = BCD2DEC(reg_val);

    reg_val = pcf8563_read_reg(PCF8563_REG_HOUR);
    if (reg_val < 0) return -1;
    rtc_time.hour = BCD2DEC(reg_val & 0x3F);

    reg_val = pcf8563_read_reg(PCF8563_REG_DATE);
    if (reg_val < 0) return -1;
    rtc_time.date = BCD2DEC(reg_val & 0x3F);

    reg_val = pcf8563_read_reg(PCF8563_REG_WEEK);
    if (reg_val < 0) return -1;
    rtc_time.week = reg_val & 0x07;

    // 读取月+世纪
    reg_val = pcf8563_read_reg(PCF8563_REG_MON_CENT);
    if (reg_val < 0) return -1;
    rtc_time.mon = BCD2DEC(reg_val & 0x1F);
    uint8_t cent_bit = (reg_val >> 7) & 0x01;

    // 读取年
    reg_val = pcf8563_read_reg(PCF8563_REG_YEAR);
    if (reg_val < 0) return -1;
    uint8_t year_bcd = BCD2DEC(reg_val);
    rtc_time.year = cent_bit ? (1900 + year_bcd) : (2000 + year_bcd);

    // 将 RTC_TimeTypeDef 转换为 struct tm
    tm_time->tm_sec  = rtc_time.sec;
    tm_time->tm_min  = rtc_time.min;
    tm_time->tm_hour = rtc_time.hour;
    tm_time->tm_mday = rtc_time.date;
    tm_time->tm_mon  = rtc_time.mon - 1;          // 1~12 → 0~11
    tm_time->tm_year = rtc_time.year - 1900;       // 实际年份 → 自1900起
    tm_time->tm_wday = rtc_time.week;               // 双方一致
    tm_time->tm_yday = 0;                            // 可计算，也可填0（非关键）
    tm_time->tm_isdst = 0;                           // 默认不使用夏令时

    /*
       printf("pcf8563_get_time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
		   tm_time->tm_year, tm_time->tm_mon, tm_time->tm_mday,
		  tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec,
		   tm_time->tm_wday);	
   */

    // 根据 VL 位返回状态（0=成功，-1=数据无效）
    //return (rtc_time.vl_flag == 0) ? 0 : -1;
    return 0;
}

/**
 * @brief 写入PCF8563初始时间（仅初始化时调用一次）
 * @param tm_time 要写入的初始时间（十进制）
 * @return 0=成功，-1=失败
 */

static int pcf8563_set_time(const struct tm *tm_time)
{
    if (tm_time == NULL || rtc_i2c_drv == NULL) return -1;
	
	printf("pcf8563_set_time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
		   tm_time->tm_year, tm_time->tm_mon, tm_time->tm_mday,
		  tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec,
		   tm_time->tm_wday);	

    // 将 struct tm 转换为 RTC_TimeTypeDef 的格式
    RTC_TimeTypeDef rtc_time;
    rtc_time.sec  = tm_time->tm_sec;
    rtc_time.min  = tm_time->tm_min;
    rtc_time.hour = tm_time->tm_hour;
    rtc_time.date = tm_time->tm_mday;
    rtc_time.week = tm_time->tm_wday;           // 双方都约定 0=Sunday
    rtc_time.mon  = tm_time->tm_mon + 1;         // struct tm 月份 0~11 → 1~12
    rtc_time.year = tm_time->tm_year + 1900;     // struct tm 年份从1900起
    rtc_time.vl_flag = 0;                         // 主动设置的时间视为有效

    // 以下使用原 pcf8563_write_reg 逻辑写入（代码不变）
    int ret = 0;
    uint8_t cent_bit = (rtc_time.year >= 2000) ? 0 : 1;

    ret = pcf8563_write_reg(PCF8563_REG_SEC, DEC2BCD(rtc_time.sec) & 0x7F);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_MIN, DEC2BCD(rtc_time.min));
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_HOUR, DEC2BCD(rtc_time.hour) & 0x3F);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_DATE, DEC2BCD(rtc_time.date) & 0x3F);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_WEEK, rtc_time.week & 0x07);
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_MON_CENT, (cent_bit << 7) | (DEC2BCD(rtc_time.mon) & 0x1F));
    if (ret != 0) return -1;
    ret = pcf8563_write_reg(PCF8563_REG_YEAR, DEC2BCD(rtc_time.year % 100));
    if (ret != 0) return -1;

    return 0;
}
#endif

/**
 * @brief RTC初始化（获取I2C驱动，校验通信）
 * @return 0=成功，-1=失败
 */
static int pcf8563_init(void) {
     int reg_val;

     if(rtc_drv_init_flag)
     {
	return 0;
    }
    // 获取I2C驱动（阻塞等待I2C驱动就绪）
    rtc_i2c_drv = (I2cDrv*)wait_drv(I2C_DRV_INDEX);
    if (rtc_i2c_drv == NULL) {
        printf("RTC init failed: I2C driver not found!\r\n");
        return -1;
    }
	if (rtc_i2c_drv->i2c_init(I2C_BUS_BAUDRATE) != 0) {
        printf("RTC init failed: I2C hardware init error!\r\n");
        return -1;
    }
	printf("pcf8563_init \r\n");
	rtc_drv_init_flag = 1;
	pcf8563_write_reg(0x0,0x0);
	pcf8563_write_reg(0x1,0x0);

	// 读取秒（含VL）
	reg_val = pcf8563_read_reg(PCF8563_REG_SEC);
	if (reg_val < 0) 
		return -1;
	if((reg_val >> 7) & 0x01){
		printf("0x02 = %x  need reset \r\n",reg_val);
		pcf8563_write_reg(0x0,0x0);
		pcf8563_write_reg(0x1,0x0);
		pcf8563_write_reg(0x2,0x0);
#if 1	
    // ==========写入初始时间 ==========
    struct tm rtc_init_tm = {
        .tm_sec  = 0,
        .tm_min  = 8,
        .tm_hour = 14,
        .tm_mday = 24,
        .tm_mon  = 1,        // 0 = January
        .tm_year = 2026 - 1900,  // 2025
        .tm_wday = 4,         // 假设为 Thursday
        .tm_yday = 0,
        .tm_isdst = 0
    };
    if (pcf8563_set_time(&rtc_init_tm) == 0) {
        struct tm read_tm;
        pcf8563_get_time(&read_tm);
        printf("RTC init success! Boot time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
               read_tm.tm_year, read_tm.tm_mon, read_tm.tm_mday,
               read_tm.tm_hour, read_tm.tm_min, read_tm.tm_sec,
               read_tm.tm_wday);    
    }else{
		printf("rtc set time failed \r\n");
	}
#endif        
	}else{
		printf("0x02 = %x  no need reset \r\n",reg_val);
	}

    return 0;
}

// 驱动接口定义
static Rtc_Driver pcf8563_rtc = {
    .drv_init = pcf8563_init,
    .set_time = pcf8563_set_time,
    .read_time = pcf8563_get_time,
};

#ifdef DRV_CORE

static int pcf8563_open_cnt = 0;
/**
 * @brief PCF8563驱动open接口（框架回调）
 */
static int pcf8563_open(file_t *filp) {
    if (pcf8563_open_cnt > 0) {
        printf("PCF8563 ReOpen(%d)\r\n", pcf8563_open_cnt);
        return 0;
    }

    int ret = pcf8563_init();
    if (ret != 0) {
        printf("PCF8563 Open Error: driver init failed\r\n");
        return -1;
    }

    pcf8563_open_cnt++;
    printf("PCF8563 Open Success\r\n");
    return 0;
}

/**
 * @brief PCF8563驱动ioctl接口（替代原read接口）
 * @param cmd 命令字（PCF8563_IOCTL_GET_TIME/PCF8563_IOCTL_SET_TIME）
 * @param arg 入参/出参指针（指向struct tm）
 * @return 0=成功，-1=失败
 */
static int pcf8563_ioctl(file_t *filp, uint32_t cmd, void *arg) {
    if (arg == NULL) {
        printf("PCF8563 IOCTL Error: invalid argument\r\n");
        return -1;
    }

    int ret = -1;
    switch (cmd) {
        case PCF8563_IOCTL_GET_TIME:
            ret = pcf8563_get_time((struct tm *)arg);
            if (ret != 0){
                printf("PCF8563 IOCTL Error: get time failed\r\n");
            }
            break;

        case PCF8563_IOCTL_SET_TIME:
            ret = pcf8563_set_time((const struct tm *)arg);
            if (ret != 0){
                printf("PCF8563 IOCTL Error: set time failed\r\n");
            }
            break;

        default:
            printf("PCF8563 IOCTL Error: unknown cmd(0x%08X)\r\n", cmd);
            ret = -1;
            break;
    }

    return ret;
}

/**
 * @brief PCF8563驱动close接口（框架回调）
 */
static int pcf8563_close(file_t *filp) {
    if (pcf8563_open_cnt == 0) {
        printf("PCF8563 Close Success\r\n");
        return 0;
    }

    pcf8563_open_cnt--;
    printf("PCF8563 Close(%d)\r\n", pcf8563_open_cnt);
    return 0;
}

static drv_ops_t pcf8563_drv_ops = {
    .name = RTC_DRV_NAME,
    .open = pcf8563_open,
    .ioctl = pcf8563_ioctl,
    .close = pcf8563_close,
};
#endif

void pcf8563_entry(void)
{
    #ifdef DRV_CORE
    int ret = goldie_driver_register(&pcf8563_drv_ops);
    if (ret == 0) {
        printf("RTC Driver Register Success: '%s'\r\n", RTC_DRV_NAME);
    } else {
        printf("RTC Driver Register Error: ret=%d\r\n", ret);
    }
	#else
    register_drv(RTC_DRV_INDEX, &pcf8563_rtc);
    printf("Register pcf8563 rtc driver at index %d\n", RTC_DRV_INDEX);
	#endif
}
GOLDIE_INIT_CALL_(pcf8563_entry);
#endif
