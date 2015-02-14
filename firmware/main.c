
#include <msp430g2153.h>
#include <stdint.h>

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

#define CMD_GET_DATA          1
#define CMD_SET_LEDS          2

#define BCMD_SET_LEDS         253
#define BCMD_GET_DATA         254
#define BCMD_ACQUIRE          255 /* usually followed by a break of a few milliseconds */

#define DISCOVERY_ADDRESS     0xCC
#define INVALID_ADDRESS       0xFE
#define BROADCAST_ADDRESS     0xFF

const uint8_t CONFIG_MAC[] = {0x5f, 0xb3, 0x5d, 0xdd, 0x5b, 0xb6, 0x5e, 0xb7};

typedef struct {
    uint16_t ch[3];
} adc_res_t;

volatile adc_res_t adc_res;
volatile unsigned int adc_raw[ADC_OVERSAMPLING];


void rs485_enable() {
    P2OUT      |= (1<<RS485_EN_PIN);
}

void rs485_disable() {
    rs485_enable();
//    P2OUT      &= ~(1<<RS485_EN_PIN);
}

int uart_getc() {
    while (!(IFG2&UCA0RXIFG)) ;
    return UCA0RXBUF;
}

void uart_putc(char c) {
    while (!(IFG2&UCA0TXIFG)) ;
    UCA0TXBUF = c;
}

char nibble_to_hex(uint8_t nibble) {
    return (nibble < 0xA) ? ('0'+nibble) : ('A'+nibble-0xA);
}

void uart_puthword(uint16_t val){
    uart_putc(nibble_to_hex(val>>12&0xF));
    uart_putc(nibble_to_hex(val>>8&0xF));
    uart_putc(nibble_to_hex(val>>4&0xF));
    uart_putc(nibble_to_hex(val&0xF));
}

void escaped_putc(uint8_t c) {
    if(c == '\\')
        uart_putc(c);
    uart_putc(c);
}

void escaped_puts(char *c) {
    do{
        escaped_putc(*c);
    }while(*c++);
}

void escaped_sendbuf(char *buf, unsigned int len) {
    for (char *end = buf+len; buf<end; buf++)
        escaped_putc(*buf);
}

#define escaped_send(obj) escaped_sendbuf((char*)(obj), sizeof(*(obj)))

uint16_t current_address = INVALID_ADDRESS;

typedef struct {
    uint8_t receiving:1;
    uint8_t ignoring:1;
    uint8_t escaped:1;
} rx_state_t;

typedef struct {
    uint8_t node_id;
    uint8_t cmd;
    union {
        struct {
            uint8_t new_id;
            uint8_t mac_mask[8];
        } discovery;
        adc_res_t adc;
    } payload;
} pkt_t;

/* protocol handling declarations */
void ucarx_handler(void) __attribute__((interrupt(USCIAB0RX_VECTOR)));
static unsigned int handle_discovery_packet(char *p, pkt_t *pkt, rx_state_t *state);
static unsigned int handle_command_packet(pkt_t *pkt, rx_state_t *state);
static unsigned int handle_broadcast_packet(pkt_t *pkt, rx_state_t *state);

void ucarx_handler() {
	static rx_state_t state;
	static pkt_t pkt;
	static char* p;
	static char* end;

    char c = UCA0RXBUF;

	if (state.escaped) {
        state.escaped = 0;
		if (c == '#') {
			state.receiving = 1;
			state.ignoring = 0;
			p = (char*)&pkt;
			end = (char*)&pkt.payload.discovery;
			return;
		}
	} else if (c == '\\') {
        state.escaped = 1;
        return;
	}
	//escape sequence handling completed. c now contains the next char of the payload.

	if (!state.receiving)
		return;
    if (!state.ignoring)
        *p = c;
    p++;

	if (p == end) {
        unsigned int n = 0;
        if (pkt.node_id == DISCOVERY_ADDRESS) {
            n = handle_discovery_packet(p, &pkt, &state);
        } else if (pkt.node_id == current_address) {
            n = handle_command_packet(&pkt, &state);
        } else if (pkt.node_id == BROADCAST_ADDRESS) {
            n = handle_broadcast_packet(&pkt, &state);
        }
        state.receiving = !!n;
        end += n;
    }
}

inline static unsigned int handle_discovery_packet(char *p, pkt_t *pkt, rx_state_t *state) {
    if (p == (char*)&pkt->payload.discovery) {
        return sizeof(pkt->payload.discovery)-1;
    } else {
        unsigned int bcnt = pkt->cmd>>1;
        unsigned int nibble = pkt->cmd&1;
        for (unsigned int i=0; i<bcnt; i++)
            if (CONFIG_MAC[i] != pkt->payload.discovery.mac_mask[i])
                return 0;
        if(nibble)
            if ((CONFIG_MAC[bcnt]&0xF0) != (pkt->payload.discovery.mac_mask[bcnt]&0xF0))
                return 0;
        uart_putc(0xFF); /* "I'm here!" */
        current_address = pkt->payload.discovery.new_id;
    }
    return 0;
}

inline static unsigned int handle_command_packet(pkt_t *pkt, rx_state_t *state) {
    switch (pkt->cmd) {
        case CMD_GET_DATA:
            escaped_send(&adc_res);
            break;
        case CMD_SET_LEDS:
            /* FIXME */
            break;
    }
    return 0;
}

inline static unsigned int handle_broadcast_packet(pkt_t *pkt, rx_state_t *state) {
    switch (pkt->cmd) {
        case CMD_SET_LEDS:
            /* FIXME */
            break;
        case BCMD_GET_DATA:
            if (!state->ignoring) {
                if (current_address == 0 ) {
                    escaped_send(&adc_res);
                } else {
                    state->ignoring = 1;
                    /* bit of arcane information on this: nodes are numbered continuously beginning from one by the
                     * master. payload position in the cluster-response is determined by the node's address. */
                    return sizeof(pkt->payload.adc)*current_address;
                }
            } else {
                state->ignoring = 0;
                escaped_send(&adc_res);
            }
            break;
        case BCMD_ACQUIRE:
            /* Kick ADC... */
            ADC10CTL0 |= ENC | ADC10SC;
            /* ...and go to sleep. */
            _BIS_SR_IRQ(LPM0_bits); /* HACK this only works because this function is always inlined into the ISR */
            break;
    }
    return 0;
}


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

int main(void){
    WDTCTL = WDTPW | WDTHOLD; /* disable WDT */

    /* clock setup */
    DCOCTL      = CALDCO_16MHZ;
    BCSCTL1     = CALBC1_16MHZ;

    /* ADC setup */
    ADC10CTL1   = ADC10CTL1_FLAGS_CH2; /* flags set in #define directive at the head of this file */
    ADC10CTL0  |= SREF_0 | ADC10SHT_3 | ADC10ON | ADC10IE;
    ADC10AE0   |= (1<<PIEZO_1_CH) | (1<<PIEZO_2_CH) | (1<<PIEZO_3_CH);

    /* ADC DTC setup */ 
    ADC10DTC0   = 0; /* one block mode, stop after block is written */
    ADC10SA     = (unsigned int)adc_raw;
    ADC10DTC1   = 8; /* Number of conversions */

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

    while(42){
//        TA0CCR0 = TA0CCR1 = TA0CCR2 = (ch1<<6);
        P2OUT   ^= (1<<ST_YLW_PIN);
//        _BIS_SR(LPM0_bits);
    }
    return 0;
}
