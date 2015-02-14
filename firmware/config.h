
/* FIXME find out optimal settings for adc clock prescaler and oversampling ratio below */
#define ADC10CTL1_FLAGS_COMMON (ADC10DIV_3 | CONSEQ_2)
/* CAUTION! The current code only supports up to 6 bits of oversampling due to the 16-bit nature of the MSP430
 * architecture */
#define ADC_OVERSAMPLING_BITS 3
#define ADC_OVERSAMPLING      (1<<ADC_OVERSAMPLING_BITS)

#define PIEZO_1_CH            0
#define PIEZO_2_CH            4
#define PIEZO_3_CH            6
#define ADC10CTL1_FLAGS_CH1 ((PIEZO_1_CH<<12) | ADC10CTL1_FLAGS_COMMON)
#define ADC10CTL1_FLAGS_CH2 ((PIEZO_2_CH<<12) | ADC10CTL1_FLAGS_COMMON)
#define ADC10CTL1_FLAGS_CH3 ((PIEZO_3_CH<<12) | ADC10CTL1_FLAGS_COMMON)

#define RGB_R_PIN             0
#define RGB_G_PIN             1
#define RGB_B_PIN             4

#define ST_YLW_PIN            2
#define ST_GRN_PIN            3

#define RS485_EN_PIN          5

