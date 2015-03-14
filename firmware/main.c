
#include <msp430g2153.h>
#include <stdint.h>
#include "uart-util.h"
#include "main.h"


#define THRES       100
#define LED_DELAY   50


const uint8_t CONFIG_MAC[];

adc_res_t adc_res = {{0,0,0}};
volatile unsigned int adc_raw[ADC_OVERSAMPLING];
volatile int autonomous = 1;

static unsigned int volatile *adc_acc_ptr;

void adc10_isr(void) __attribute__((interrupt(ADC10_VECTOR), wakeup)); /* "wakeup" makes this not re-enter low-power modes on exit */
void adc10_isr(void) {
    static void *adc_isr_state = &&adc_ch0;

    ADC10CTL0 &= ~ENC;
    while(ADC10CTL1 & BUSY); /* wait until ADC is ready to be reconfigured */
    goto *adc_isr_state;

adc_ch0:
    ADC10CTL1 = ADC10CTL1_FLAGS_CH1;
    adc_acc_ptr = adc_res.ch+0;
    adc_isr_state = &&adc_ch1;
    goto adc_exit;

adc_ch1:
    ADC10CTL1 = ADC10CTL1_FLAGS_CH2;
    adc_acc_ptr = adc_res.ch+1;
    adc_isr_state = &&adc_ch2;
    goto adc_exit;

adc_ch2:
    ADC10CTL1 = ADC10CTL1_FLAGS_CH0;
    adc_acc_ptr = adc_res.ch+2;
    adc_isr_state = &&adc_ch0;

adc_exit:
    return;
}

void timera1_isr(void) __attribute__((interrupt(TIMER1_A1_VECTOR)));
void timera1_isr(void) {
    TA1CTL   &= ~TAIFG;
    TA1CCTL0 &= ~OUTMOD_7;
    TA1CCTL1 &= ~OUTMOD_7;
    TA1CCTL2 &= ~OUTMOD_7;
    TA1CCTL0 |= OUTMOD_5;
    TA1CCTL1 |= OUTMOD_5;
    TA1CCTL2 |= OUTMOD_5;
}

static unsigned int update_adc_acc(void) {
    unsigned int old = *adc_acc_ptr;
    unsigned int new = 0;
    /* pointer targets volatile, pointers itself not */
    volatile unsigned int *end = adc_raw+ADC_OVERSAMPLING;
    for (volatile unsigned int *p=adc_raw; p<end; p++) {
        new += *p;
    }
    new >>= ADC_OVERSAMPLING_BITS;

    *adc_acc_ptr = new;
    return new-old+THRES/2 > THRES;
}

int main(void){
    WDTCTL = WDTPW | WDTHOLD; /* disable WDT */

    /* Wait for supply rails to settle */
    __delay_cycles(500000);

    /* Clock setup, uses internal calibrated RC osc */
    DCOCTL      = CALDCO_16MHZ;
    BCSCTL1     = CALBC1_16MHZ;

    /* Non-PWM setup */
    P2DIR      |= (1<<0) | (1<<1) | (1<<4);
    P2OUT      &= ~((1<<0) | (1<<1) | (1<<4));
    P2OUT      |= (1<<0) | (1<<1); /* nice pink */

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

    kick_adc(); /* Do one initial conversion */
    _BIS_SR(LPM0_bits); /* ...and go to sleep. */

    /* The following loop contains some magic. Once the ADC completes a conversion sequence, the CPU is woken up from
     * sleep and the ADC interrupt above is invoked. This interrupt sets up the conversion result pointer, sets the
     * input control register for the next channel and exits *without sending the CPU back to sleep*. Consequently,
     * control is returned to the main loop which is now at the respective handler block below, which will average the
     * measurements and update the result memory in update_adc_acc before re-starting the ADC for the next conversion
     * and going back to sleep.
     *
     * If the system is not running in autonomous mode, the ADC is not restarted after channel 3's conversion sequence
     * and the ADC nees to be re-started from the protocol handler via a CMD_ACQUIRE. If the system is running in
     * autonomous mode, the ADC is running continuously.
     */
    unsigned int led_ctr[3] = {};
    while (42) {
        /* Channel 1 */
        if (update_adc_acc())
            led_ctr[0] = LED_DELAY;
        kick_adc();
        _BIS_SR(LPM0_bits);

        /* Channel 2 */
        if (update_adc_acc())
            led_ctr[1] = LED_DELAY;
        kick_adc();
        _BIS_SR(LPM0_bits);

        /* Channel 3 */
        if (update_adc_acc())
            led_ctr[2] = LED_DELAY;

        if (autonomous) {
            unsigned int p2reg = P2OUT & ~(1<<0 | 1<<1);
            if (led_ctr[0]) led_ctr[0]--;
            if (led_ctr[1]) led_ctr[1]--;
            if (led_ctr[2]) led_ctr[2]--;
            p2reg      |= !led_ctr[0] <<0;
            p2reg      |= !led_ctr[1] <<1;
            p2reg      |= !led_ctr[2] <<1; /* Only use r and b leds for nice, pink color */
            P2OUT = p2reg;

            kick_adc(); /* Restart sequence */
        }
        _BIS_SR(LPM0_bits);
    }
    return 0;
}
