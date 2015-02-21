
#include <msp430g2153.h>
#include <stdint.h>
#include "uart-util.h"
#include "main.h"


const uint8_t CONFIG_MAC[];

volatile adc_res_t adc_res = {{0,0,0}};
volatile unsigned int adc_raw[ADC_OVERSAMPLING];


void adc10_isr(void) __attribute__((interrupt(ADC10_VECTOR)));
void adc10_isr(void) {
    ADC10CTL0 &= ~ENC;
    while(ADC10CTL1 & BUSY); /* wait until ADC is ready to be reconfigured */

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

    kick_adc();
    _BIS_SR_IRQ(LPM0_bits);
}


/*
void timera1_isr(void) __attribute__((interrupt(TIMER1_A1_VECTOR)));
void timera1_isr(void) {
    TA1CTL   &= ~TAIFG;
    TA1CCTL0 &= ~OUTMOD_7;
    TA1CCTL0 |= OUTMOD_5;
    TA1CCTL1 &= ~OUTMOD_7;
    TA1CCTL1 |= OUTMOD_5;
    TA1CCTL2 &= ~OUTMOD_7;
    TA1CCTL2 |= OUTMOD_5;
}
*/


int main(void){
    WDTCTL = WDTPW | WDTHOLD; /* disable WDT */

    /* clock setup */
    DCOCTL      = CALDCO_16MHZ;
    BCSCTL1     = CALBC1_16MHZ;

    /* PWM setup */
    /*
    P2DIR      |= (1<<0) | (1<<1) | (1<<4);
    P2OUT      &= ~((1<<0) | (1<<1) | (1<<4));
    P2SEL      |= (1<<0) | (1<<1) | (1<<4);

    TA1CTL      = TASSEL_2 | ID_0 | MC_0 | TACLR | TAIE;
    TA1CCTL0    = OUTMOD_5 | OUT;
    TA1CCTL1    = OUTMOD_5 | OUT;
    TA1CCTL2    = OUTMOD_5 | OUT;
    TA1CCR0     = 0xC000;
    TA1CCR1     = 0xC000;
    TA1CCR2     = 0xC000;
    TA1CTL     |= MC_2;
    */

    /* ADC setup */
    ADC10CTL1   = ADC10CTL1_FLAGS_CH2; /* flags set in #define directive at the head of this file */
    ADC10CTL0  |= SREF_0 | ADC10SHT_3 | MSC | ADC10ON | ADC10IE; /* FIXME find optimal ADC10SHT setting */
    ADC10AE0   |= (1<<PIEZO_1_CH) | (1<<PIEZO_2_CH) | (1<<PIEZO_3_CH);

    /* UART setup */
    P1SEL      |= 0x06;
    P1SEL2     |= 0x06;

    /* RS485 enable */
    P1DIR      |= (1<<RS485_EN_PIN);
    rs485_disable();

    /* Set for 115.2kBd @ 16MHz */
    UCA0CTL1   |= UCSSEL1;
    UCA0BR0     = 138;
    UCA0BR1     = 0;
    UCA0MCTL    = UCBRS2;
    UCA0CTL1   &= ~UCSWRST;

    IE2 |= UCA0RXIE;

    /* Status LED setup */
    P2DIR      |= (1<<ST_GRN_PIN) | (1<<ST_YLW_PIN);
    P2OUT      &= ~((1<<ST_GRN_PIN) | (1<<ST_YLW_PIN));
    
//    P2OUT      &= ~(1<<ST_YLW_PIN);
//    P2OUT      |= (1<<ST_GRN_PIN);

    /* Set global interrupt enable flag */
    _BIS_SR(GIE);

    while (42) {
//        TA0CCR0 = TA0CCR1 = TA0CCR2 = (ch1<<6);
//        _BIS_SR(LPM0_bits);
    }
    return 0;
}
