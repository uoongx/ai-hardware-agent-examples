#ifdef SUPPORT_BATTERY
#include "goldie_osal.h"
#include "driver_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#endif
#include "bat_zcv.h"
#include <stdint.h>
#include <math.h>

#define CHARGER_DET_GPIO  15   //P1_4
static GpioDrvExt * gpio_ext_drv;

#define BAT_ADC_CHN           1
/* 电池电压范围 (mV) */
#define BAT_VOLTAGE_MIN_MV    3200  /* 3.2V = 3200mV */
#define BAT_VOLTAGE_MAX_MV    4200  /* 4.2V = 4200mV */
#define BAT_VOLTAGE_RANGE_MV  (BAT_VOLTAGE_MAX_MV - BAT_VOLTAGE_MIN_MV)

/* 分压电阻比例
 * vbat -> 200k -> ADC测量点 -> 100k -> GND
 * 分压比 = 100k / (200k + 100k) = 1/3
 * 实际电池电压(mV) = ADC读取电压(mV) * 3
 */
// #define VOLTAGE_DIVIDER_RATIO 3

//新版：
/* 分压电阻比例
 * vbat -> 4.7K -> ADC测量点 -> 10k -> GND
 * 分压比 = 10k / (10k + 4.7k) = 0.68
 * 实际电池电压(mV) = ADC读取电压(mV) * 1.47
 */
#define VOLTAGE_DIVIDER_RATIO 1.47

/* ADC采样次数，用于滤波 */
#define ADC_SAMPLE_COUNT      5

// 静态全局变量，保存上次状态
static float g_vbat;          // 开路电压 (mV)
static float g_cbat;          // 容量百分比 (%)
static float g_cap_bat;       // 剩余容量 (mAh)
static float g_zbat;          // 电池内阻 (mΩ)
static goldie_timeval g_last_time; // 上次RTC时间

// 辅助函数：根据电压查容量（线性插值）
static float lookup_capacity_by_voltage(float voltage_mv) {
    int i;
    int n = sizeof(cv_table) / sizeof(cv_table[0]);
    if (voltage_mv >= cv_table[0][0]) return cv_table[0][1];
    if (voltage_mv <= cv_table[n-1][0]) return cv_table[n-1][1];
    for (i = 0; i < n-1; i++) {
        if (voltage_mv >= cv_table[i+1][0] && voltage_mv <= cv_table[i][0]) {
            float v_high = cv_table[i][0];
            float v_low = cv_table[i+1][0];
            float cap_high = cv_table[i][1];
            float cap_low = cv_table[i+1][1];
            return cap_low + (cap_high - cap_low) * (voltage_mv - v_low) / (v_high - v_low);
        }
    }
    return 0.0;
}

// 辅助函数：根据容量查内阻（线性插值），确保返回正数
static float lookup_resistance_by_capacity(float capacity_percent) {
    int i;
    int n = sizeof(zc_table) / sizeof(zc_table[0]);
    if (capacity_percent >= zc_table[0][0]) return zc_table[0][1];
    if (capacity_percent <= zc_table[n-1][0]) return zc_table[n-1][1];
    for (i = 0; i < n-1; i++) {
        if (capacity_percent <= zc_table[i][0] && capacity_percent >= zc_table[i+1][0]) {
            float cap_high = zc_table[i][0];
            float cap_low = zc_table[i+1][0];
            float res_high = zc_table[i][1];
            float res_low = zc_table[i+1][1];
            float res = res_low + (res_high - res_low) * (capacity_percent - cap_low) / (cap_high - cap_low);
            return (res > 0) ? res : 66.0f; // 防御：若计算出0则返回默认最小值66
        }
    }
    return (zc_table[n-1][1] > 0) ? zc_table[n-1][1] : 66.0f;
}

// 辅助函数：根据容量查电压（线性插值）
static float lookup_voltage_by_capacity(float capacity_percent) {
    int i;
    int n = sizeof(cv_table) / sizeof(cv_table[0]);
    if (capacity_percent >= cv_table[0][1]) return cv_table[0][0];
    if (capacity_percent <= cv_table[n-1][1]) return cv_table[n-1][0];
    for (i = 0; i < n-1; i++) {
        if (capacity_percent <= cv_table[i][1] && capacity_percent >= cv_table[i+1][1]) {
            float cap_high = cv_table[i][1];
            float cap_low = cv_table[i+1][1];
            float volt_high = cv_table[i][0];
            float volt_low = cv_table[i+1][0];
            return volt_low + (volt_high - volt_low) * (capacity_percent - cap_low) / (cap_high - cap_low);
        }
    }
    return cv_table[n-1][0];
}

static int isCharging(void){ 
    int ret = gpio_ext_drv->get_gpio_value(CHARGER_DET_GPIO);
    return ret;
}

/**
 * @brief 读取电池电压（单位：mV）
 *
 * @return uint32_t 电池电压值（单位：mV）
 */
static uint32_t read_battery_voltage_mv(void)
{
    uint32_t adc_value;
    uint32_t voltage_sum = 0;

    /* 多次采样取平均值，减少噪声影响 */
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        adc_value = goldie_adc_read(BAT_ADC_CHN);
        /* goldie_adc_read 返回的是分压后的电压值（单位：mV） */
        voltage_sum += adc_value;
    }

    /* 计算平均电压（单位：mV）并转换为实际电池电压（单位：mV） */
    uint32_t avg_adc_voltage_mv = voltage_sum / ADC_SAMPLE_COUNT;
    printf("adc_read: %d \r\n",avg_adc_voltage_mv);
    uint32_t battery_voltage_mv = avg_adc_voltage_mv * VOLTAGE_DIVIDER_RATIO;

    return battery_voltage_mv;
}

// 初始容量估算函数
// 初始容量估算
static void init_cbat(void) {
    uint32_t v_measured = read_battery_voltage_mv();
    int charging = isCharging();

    float vbat0 = (float)v_measured;
    float cbat0, zbat0, vdelta, vbat, cbat;

    if (!charging) { // 放电
        cbat0 = lookup_capacity_by_voltage(vbat0);
        zbat0 = lookup_resistance_by_capacity(cbat0);
        vdelta = 70.0f * zbat0 / 1000.0f; // 70mA * 内阻(mΩ) / 1000 = mV
        vbat = vbat0 + vdelta;
        cbat = lookup_capacity_by_voltage(vbat);
        g_zbat = zbat0; // 内阻采用初次计算值
    } else { // 充电
        cbat0 = lookup_capacity_by_voltage(vbat0);
        zbat0 = lookup_resistance_by_capacity(cbat0);
        vdelta = CHARGING_CURRENT_BIG * zbat0 / 1000.0f; // 1000mA * 内阻(mΩ)/1000 = zbat0(mV)
        float vbat1 = vbat0 - vdelta;
        float cbat1 = lookup_capacity_by_voltage(vbat1);
        float zbat = lookup_resistance_by_capacity(cbat1); // 步骤6得到新内阻
        float vdelta1 = CHARGING_CURRENT_BIG * zbat / 1000.0f;
        vbat = vbat0 - vdelta1;
        cbat = lookup_capacity_by_voltage(vbat);
        g_zbat = zbat; // 保存步骤6得到的内阻
    }

    g_vbat = vbat;
    
    g_cbat = cbat;
    g_cap_bat = cbat * BAT_CAP_MAX / 100.0f;

    // 保存RTC时间（逐成员赋值，避免结构体直接赋值可能带来的问题）
    goldie_gettimeofday(&g_last_time);
}

// 电量持续计量
static void calc_cbat(void) {
    goldie_timeval now;
    goldie_gettimeofday(&now);

    // 计算时间差（秒）
    double delta_sec = (now.tv_sec - g_last_time.tv_sec) +
                       (now.tv_usec - g_last_time.tv_usec) / 1000000.0;

    if (delta_sec <= 0) {
        return; // 时间未变化或倒退，不更新
    }

    uint32_t v_measured = read_battery_voltage_mv();
    printf("v_measured %d \r\n",v_measured);
    float cur_v = (float)v_measured;

    // 电压差 = 上次开路电压 - 当前端电压
    float vdelta_tmp = g_vbat - cur_v;

    // 防止内阻为零（理论上不会发生，但防御）
    float zbat_safe = (g_zbat > 0) ? g_zbat : 66.0f;

    // 计算电流（A），正值表示放电，负值表示充电
    float current_A = vdelta_tmp / zbat_safe;	
    int current_ma = current_A*1000;
    printf("current_ma %d \r\n",current_ma);

    // 容量变化(mAh) = 电流(A) * 时间(小时) * 1000
    float delta_cap_mAh = current_A * (delta_sec / 3600.0f) * 1000.0f;

    // 更新剩余容量
    g_cap_bat -= delta_cap_mAh;

    // 确保容量在合理范围
    if (g_cap_bat < 0) g_cap_bat = 0;
    if (g_cap_bat > BAT_CAP_MAX) g_cap_bat = BAT_CAP_MAX;

    // 计算新容量百分比
    g_cbat = g_cap_bat * 100.0f / BAT_CAP_MAX;

    // 根据新容量更新内阻和开路电压
    g_zbat = lookup_resistance_by_capacity(g_cbat);
    g_vbat = lookup_voltage_by_capacity(g_cbat);
    int int_vbat = g_vbat;
    printf("g_vbat %d \r\n",int_vbat);

    // 更新时间（逐成员赋值）
    g_last_time.tv_sec = now.tv_sec;
    g_last_time.tv_usec = now.tv_usec;
}

#if 0
/**
 * @brief 计算电池电量百分比
 *
 * @param voltage_mv 电池电压（单位：mV）
 * @return int 电量百分比（0~100）
 */
static int calculate_battery_percentage(uint32_t voltage_mv)
{
    int percentage;

    /* 限制电压在有效范围内 */
    if (voltage_mv <= BAT_VOLTAGE_MIN_MV) {
        percentage = 0;
    } else if (voltage_mv >= BAT_VOLTAGE_MAX_MV) {
        percentage = 100;
    } else {
        /* 线性计算电量百分比，使用整数运算避免浮点 */
        /* 公式：百分比 = (电压 - 最小电压) * 100 / 电压范围 */
        uint32_t diff = voltage_mv - BAT_VOLTAGE_MIN_MV;
        percentage = (int)((diff * 100) / BAT_VOLTAGE_RANGE_MV);
    }

    return percentage;
}
#endif

/**
 * @brief 读取电池电量百分比（提供给上层的接口）
 *
 * @return int 电池电量百分比（0~100）
 */
static int battery_read_power(void)
{
    int percentage = g_cbat;
    return percentage;
}

/**
 * @brief 电池驱动初始化
 *
 * @return int 0表示成功，非0表示失败
 */
static int battery_drv_init(void)
{
    uint32_t ret;

    /* 初始化ADC */
    ret = goldie_adc_init();
    if (ret != 0) {
        /* ADC初始化失败 */
        return -1;
    }

gpio_ext_drv = wait_drv(GPIO_EXT_DRV_INDEX);
gpio_ext_drv->drv_init();
gpio_ext_drv->set_gpio_func(CHARGER_DET_GPIO,GOLDIE_GPIO_DIRECTION_INPUT);

    init_cbat();
    return 0;
}

/* 电池驱动操作结构体 */
static BatDrv bat_drv = {
    .init = battery_drv_init,
    .calc_soc = calc_cbat,
    .read_power = battery_read_power,
    .is_charging = isCharging,
};

#ifdef DRV_CORE
static int battery_open_cnt = 0;

/**
 * @brief 电池驱动open接口（框架回调）
 */
static int battery_open(file_t *filp) {
    if (battery_open_cnt > 0) {
        printf("Battery ReOpen(%d)\r\n", battery_open_cnt);
        return 0;
    }

    int ret = battery_drv_init();
    if (ret != 0) {
        printf("Battery Open Error: driver init failed\r\n");
        return -1;
    }

    battery_open_cnt++;
    printf("Battery Open Success\r\n");
    return 0;
}

/**
 * @brief 电池驱动read接口（框架回调）
 * @param buf 输出缓冲区
 * @return 成功返回4，失败返回-1
 */
static int battery_read(file_t *filp, uint8_t *buf, uint16_t len) {
    int *out_value = (int *)buf;
    if (buf == NULL || len < sizeof(int)) {
        printf("Battery Read Error: invalid buffer (len=%d < 5)\r\n", len);
        return -1;
    }
    *out_value = battery_read_power();
    return 5;
}

/**
 * @brief 电池驱动close接口（框架回调）
 */
static int battery_close(file_t *filp) {
    if (battery_open_cnt == 0) {
		printf("Battery Close Success\r\n");
        return 0;
    }

    battery_open_cnt--;
    printf("Battery Close(%d)\r\n", battery_open_cnt);
    return 0;
}

static int battery_ioctl(file_t *filp, uint8_t cmd, void *arg){
    int *out_value;
    switch (cmd) {
        case BAT_IOCTL_CAL_POWER:
		calc_cbat();
		break;
	case BAT_IOCTL_GET_CHGSTATUS:
		out_value = (int*)arg;
		*out_value = isCharging();
		break;
	default:
		break;
    	}
}


static drv_ops_t battery_drv_ops = {
    .name = BAT_DRV_NAME,
    .open = battery_open,
    .read = battery_read,
    .ioctl = battery_ioctl,
    .close = battery_close,
};
#endif

/**
 * @brief 电池驱动注册函数
 * 在系统初始化时调用此函数注册电池驱动
 */
void battery_driver_register(void)
{
	/* 注册电池驱动 */
    #ifdef DRV_CORE
    int ret = goldie_driver_register(&battery_drv_ops);
    if (ret == 0) {
        printf("Battery Driver Register Success: '%s'\r\n", BAT_DRV_NAME);
    } else {
        printf("Battery Driver Register Error: ret=%d\r\n", ret);
    }
    #else
    register_drv(BATTERY_DRV_INDEX, &bat_drv);
    #endif
}

GOLDIE_INIT_CALL_(battery_driver_register);
#endif
