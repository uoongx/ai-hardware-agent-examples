#include "goldie_osal.h"

uint32_t goldie_adc_init(void)
{
    return uapi_adc_init(ADC_CLOCK_NONE);
}

uint32_t goldie_deinit(void)
{
    return uapi_adc_deinit();
}

uint32_t goldie_adc_read(uint8_t channel)
{
    uint16_t adc_value;
    adc_port_read(channel, &adc_value);
    return adc_value;
}
