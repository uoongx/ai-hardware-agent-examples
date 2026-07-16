#ifndef WS63_ADC_H
#define WS63_ADC_H
typedef enum adc_clock {
    ADC_CLOCK_500KHZ = 0,               /*!< @if Eng ADC clock: 500KHZ.
                                             @else   ADC时钟频率： 500KHZ。  @endif */
    ADC_CLOCK_250KHZ = 1,               /*!< @if Eng ADC clock: 250KHZ.
                                             @else   ADC时钟频率： 250KHZ。  @endif */
    ADC_CLOCK_125KHZ = 2,               /*!< @if Eng ADC clock: 125KHZ.
                                             @else   ADC时钟频率： 125KHZ。  @endif */
    ADC_CLOCK_015KHZ = 3,               /*!< @if Eng ADC clock: 015KHZ.
                                             @else   ADC时钟频率： 015KHZ。  @endif */
    ADC_CLOCK_MAX,
    ADC_CLOCK_NONE = ADC_CLOCK_MAX
} adc_clock_t;

uint32_t uapi_adc_init(adc_clock_t clock);
uint32_t adc_port_read(unsigned char channel, unsigned short *data);
uint32_t uapi_adc_deinit(void);
#endif
