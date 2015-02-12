
#include <msp430g2153.h>
#include <stdint.h>


#define PIEZO_1_CH        0
#define PIEZO_2_CH        4
#define PIEZO_3_CH        6
#define ADC10CTL1_FLAGS_CH1 ((PIEZO_1_CH<<12) | ADC10DIV_3)
#define ADC10CTL1_FLAGS_CH2 ((PIEZO_2_CH<<12) | ADC10DIV_3)
#define ADC10CTL1_FLAGS_CH3 ((PIEZO_3_CH<<12) | ADC10DIV_3)

#define RGB_R_PIN         0
#define RGB_G_PIN         1
#define RGB_B_PIN         4

#define ST_YLW_PIN        2
#define ST_GRN_PIN        3

#define RS485_EN_PIN      5


/* protocol definitions */
#define CONFIG_NAME  "徳川家康"

#define ECHO_NAME         1
#define GET_DATA          2

#define ADDRESS_DISCOVERY 0xCC
#define ADDRESS_INVALID   0xFF

const uint8_t CONFIG_MAC[] = {207, 105, 193, 102, 191, 213, 19, 7};

volatile int cur_adc_ch;
volatile int adc_res[3];


void rs485_enable(){
    P2OUT      |= (1<<RS485_EN_PIN);
}

void rs485_disable(){
    rs485_enable();
//    P2OUT      &= ~(1<<RS485_EN_PIN);
}

int uart_getc(){
    while(!(IFG2&UCA0RXIFG));
    return UCA0RXBUF;
}

void uart_putc(char c){
    while(!(IFG2&UCA0TXIFG));
    UCA0TXBUF = c;
}

char nibble_to_hex(uint8_t nibble){
    return (nibble < 0xA) ? ('0'+nibble) : ('A'+nibble-0xA);
}

void uart_puthword(uint16_t val){
    uart_putc(nibble_to_hex(val>>12&0xF));
    uart_putc(nibble_to_hex(val>>8&0xF));
    uart_putc(nibble_to_hex(val>>4&0xF));
    uart_putc(nibble_to_hex(val&0xF));
}

void escaped_putc(uint8_t c){
    if(c == '\\')
        uart_putc(c);
    uart_putc(c);
}

void escaped_puts(char *c){
    do{
        escaped_putc(*c);
    }while(*c++);
}

void escaped_putadc(uint16_t val){
    escaped_putc(val>>8);
    escaped_putc(val&0xFF);
}


uint16_t current_address = ADDRESS_INVALID;

void ucarx_handler(void) __attribute__((interrupt(USCIAB0RX_VECTOR)));
void ucarx_handler(){
	static struct {
		uint8_t receiving:1;
		uint8_t escaped:1;
	} state;
	static uint8_t* p;
	static uint8_t* end;
	static struct {
		uint8_t node_id;
		uint8_t cmd;
        struct {
            uint8_t new_id;
            uint8_t mac_mask[8];
        } discovery_payload;
	} pkt;

    uint8_t c = UCA0RXBUF;

	if (state.escaped) {
        state.escaped = 0;
		if (c == '#') {
			state.receiving = 1;
			p = (uint8_t*)&pkt;
			end = (uint8_t*)&pkt.discovery_payload;
			return;
		}
	} else if (c == '\\') {
        state.escaped = 1;
        return;
	}
	//escape sequence handling completed. c now contains the next char of the payload.

	if (!state.receiving)
		return;
	*p++ = c;

	if(p == end) {
        state.receiving = 0;
        if (pkt.node_id == ADDRESS_DISCOVERY) {
            if (p == (uint8_t*)&pkt.discovery_payload) {
                state.receiving = 1;
                end += sizeof(pkt.discovery_payload)-1;
            } else {
                uint8_t bcnt = pkt.cmd>>1;
                uint8_t nibble = pkt.cmd&1;
                for (uint8_t i=0; i<bcnt; i++)
                    if (CONFIG_MAC[i] != pkt.discovery_payload.mac_mask[i])
                        return;
                if(nibble)
                    if ((CONFIG_MAC[bcnt]&0xF0) != (pkt.discovery_payload.mac_mask[bcnt]&0xF0))
                        return;
                uart_putc(0xFF); /* "I'm here!" */
                current_address = pkt.discovery_payload.new_id;
            }
        } else if (pkt.node_id == current_address) {
            switch (pkt.cmd) {
                case ECHO_NAME:
                    escaped_puts(CONFIG_NAME);
                    break;
                case GET_DATA:
                    escaped_putadc(adc_res[0]);
                    escaped_putadc(adc_res[1]);
                    escaped_putadc(adc_res[2]);
                    break;
            }
        }
    }
}


void adc10_isr(void) __attribute__((interrupt(ADC10_VECTOR)));
void adc10_isr(void) {
    ADC10CTL0 &= ~ENC;
    switch(cur_adc_ch){
        case ADC10CTL1_FLAGS_CH1:
            cur_adc_ch = ADC10CTL1_FLAGS_CH2;
            adc_res[0] = ADC10MEM;
            _BIS_SR_IRQ(LPM0_bits);
            break;
        case ADC10CTL1_FLAGS_CH2:
            cur_adc_ch = ADC10CTL1_FLAGS_CH3;
            adc_res[1] = ADC10MEM;
            _BIS_SR_IRQ(LPM0_bits);
            break;
        case ADC10CTL1_FLAGS_CH3:
            cur_adc_ch = ADC10CTL1_FLAGS_CH1;
            adc_res[2] = ADC10MEM;
            _BIC_SR_IRQ(LPM0_bits);
            break;
    }
    ADC10CTL1  = cur_adc_ch;
    ADC10CTL0 |= ENC | ADC10SC;
}

int main(void){
    WDTCTL = WDTPW | WDTHOLD; /* disable WDT */

    /* clock setup */
    DCOCTL      = CALDCO_16MHZ;
    BCSCTL1     = CALBC1_16MHZ;

    /* ADC setup */
    ADC10CTL1   = ADC10CTL1_FLAGS_CH2;
    cur_adc_ch  = ADC10CTL1_FLAGS_CH2;
//    ADC10CTL0  |= SREF_0 | ADC10SHT_3 | ADC10ON | ADC10IE;
    ADC10CTL0  |= SREF_0 | ADC10SHT_3 | ADC10ON;
    ADC10AE0   |= (1<<PIEZO_1_CH) | (1<<PIEZO_2_CH) | (1<<PIEZO_3_CH);

    /* UART setup */
    P1SEL      |= 0x06;
    P1SEL2     |= 0x06;

    /* RS485 enable */
    P2DIR      |= (1<<RS485_EN_PIN);
    rs485_enable();

    /* Set for 115.2kBd @ 16MHz */
    UCA0CTL1   |= UCSSEL1;
    UCA0BR0     = 138;
    UCA0BR1     = 0;
    UCA0MCTL    = UCBRS2;
    UCA0CTL1   &= ~UCSWRST;
    
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

    /* Kick ADC... */
    ADC10CTL0 |=  ENC | ADC10SC;

    /* ...and go to sleep. */
//    _BIS_SR(LPM0_bits);

    while(42){
//        TA0CCR0 = TA0CCR1 = TA0CCR2 = (ch1<<6);
        P2OUT   ^= (1<<ST_YLW_PIN);
//        _BIS_SR(LPM0_bits);
    }
    return 0;
}
