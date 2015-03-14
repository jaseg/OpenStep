
#include <msp430g2153.h>
#include <stdint.h>
#include "uart-util.h"
#include "main.h"


#define THRES 30


const uint8_t CONFIG_MAC[];

volatile adc_res_t adc_res = {{0,0,0}};
volatile unsigned int adc_raw[ADC_OVERSAMPLING];
volatile int autonomous = 1;


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
            if (autonomous) {
                P2OUT      &= ~(1<<0);
                P2OUT      |= ((!!((adc_res.ch[0]-acc)+THRES/2 < THRES))<<0);
            }
            adc_res.ch[0] = acc;
            break;
        case ADC10CTL1_FLAGS_CH2:
            ADC10CTL1 = ADC10CTL1_FLAGS_CH3;
            if (autonomous) {
                P2OUT      &= ~(1<<1);
                P2OUT      |= ((!!((adc_res.ch[1]-acc)+THRES/2 < THRES))<<1);
            }
            adc_res.ch[1] = acc;
            break;
        case ADC10CTL1_FLAGS_CH3:
            ADC10CTL1 = ADC10CTL1_FLAGS_CH1;
            if (autonomous) {
                P2OUT      &= ~(1<<4);
                P2OUT      |= ((!!((adc_res.ch[2]-acc)+THRES/2 < THRES))<<1);
                adc_res.ch[2] = acc;
                break;
            }
            adc_res.ch[2] = acc;
            return; /* End conversion sequence */
    }

    kick_adc();
    _BIS_SR_IRQ(LPM0_bits);
}

int main(void){
    WDTCTL = WDTPW | WDTHOLD; /* disable WDT */

    /* wait for supply rails to settle */
    __delay_cycles(500000);

    /* clock setup */
    DCOCTL      = CALDCO_16MHZ;
    BCSCTL1     = CALBC1_16MHZ;

    /* PWM setup */
    P2DIR      |= (1<<0) | (1<<1) | (1<<4);
    P2OUT      &= ~((1<<0) | (1<<1) | (1<<4));
    P2OUT      |= (1<<0) | (1<<1);

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
    UCA0MCTL    = UCBRS_7;
    UCA0CTL1   &= ~UCSWRST;

    IE2 |= UCA0RXIE;

    /* Status LED setup */
    P2DIR      |= (1<<ST_GRN_PIN) | (1<<ST_YLW_PIN);
    P2OUT      &= ~((1<<ST_GRN_PIN) | (1<<ST_YLW_PIN));
    
    /* Set global interrupt enable flag */
    _BIS_SR(GIE);

    kick_adc();

    while (42) {
        _BIS_SR(LPM0_bits);
    }
    return 0;
}
