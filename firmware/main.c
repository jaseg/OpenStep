
#include <msp430g2153.h>
#include "uart-util.h"
#include "main.h"


volatile adc_res_t adc_res;
volatile unsigned int adc_raw[ADC_OVERSAMPLING];


void adc10_isr(void) __attribute__((interrupt(ADC10_VECTOR)));
void adc10_isr(void) {
    ADC10CTL0 &= ~ENC;

    unsigned int acc = 0;
    /* pointer targets volatile, pointers itself not */
    volatile unsigned int *end = adc_raw+ADC_OVERSAMPLING;
    for (volatile unsigned int *p=adc_raw; p<end; p++) {
        acc += *p;
    }
    acc >>= ADC_OVERSAMPLING_BITS;

    switch (ADC10CTL1) {
        case ADC10CTL1_FLAGS_CH1:
            ADC10CTL1 = ADC10CTL1_FLAGS_CH2;
            adc_res.ch[0] = acc;
            break;
        case ADC10CTL1_FLAGS_CH2:
            ADC10CTL1 = ADC10CTL1_FLAGS_CH3;
            adc_res.ch[1] = acc;
            break;
        case ADC10CTL1_FLAGS_CH3:
            ADC10CTL1 = ADC10CTL1_FLAGS_CH1;
            adc_res.ch[2] = acc;
            return; /* End conversion sequence */
    }

    ADC10CTL0 |= ENC | ADC10SC; /* Kick ADC... */
    _BIS_SR_IRQ(LPM0_bits);
}


int app_main(void) {
    /* ADC setup */
    ADC10CTL1   = ADC10CTL1_FLAGS_CH2; /* flags set in #define directive at the head of this file */
    ADC10CTL0  |= SREF_0 | ADC10SHT_3 | ADC10ON | ADC10IE; /* FIXME find optimal ADC10SHT setting */
    ADC10AE0   |= (1<<PIEZO_1_CH) | (1<<PIEZO_2_CH) | (1<<PIEZO_3_CH);

    /* ADC DTC setup */ 
    ADC10DTC0   = 0; /* one block mode, stop after block is written */
    ADC10SA     = (unsigned int)adc_raw;
    ADC10DTC1   = 8; /* Number of conversions */

    IE2 |= UCA0RXIE;

    /* PWM setup */
/*    P2DIR      |= (1<<RGB_R_PIN) | (1<<RGB_G_PIN) | (1<<RGB_B_PIN);
    P2SEL      |= (1<<RGB_R_PIN) | (1<<RGB_G_PIN) | (1<<RGB_B_PIN);

    TA0CTL      = TASSEL_2 | ID_0 | MC_2;
    TA0CCTL0    = OUTMOD_3;
    TA0CCTL1    = OUTMOD_3;
    TA0CCTL2    = OUTMOD_3;
*/

    /* Status LED setup */
    P2DIR      |= (1<<ST_GRN_PIN) | (1<<ST_YLW_PIN);

    /* Set global interrupt enable flag */
    _BIS_SR(GIE);

    while(42){
//        TA0CCR0 = TA0CCR1 = TA0CCR2 = (ch1<<6);
        P2OUT   ^= (1<<ST_YLW_PIN);
//        _BIS_SR(LPM0_bits);
    }
    return 0;
}
