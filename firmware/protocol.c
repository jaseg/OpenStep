#include "uart-util.h"
#include "main.h"

#define CMD_GET_DATA          1
#define CMD_SET_LEDS          2
#define CMD_FLASH_LED         3
#define CMD_STATUS_OFF        4
#define CMD_STATUS_GREEN      5
#define CMD_STATUS_YELLOW     6
#define CMD_STATUS_BOTH       7

#define BCMD_GET_DATA         254
#define BCMD_ACQUIRE          255 /* usually followed by a break of a few milliseconds */

#define DISCOVERY_ADDRESS     0xCC
#define INVALID_ADDRESS       0xFE
#define BROADCAST_ADDRESS     0xFF


uint16_t current_address = INVALID_ADDRESS;

typedef union {
    struct {
        uint16_t r, g, b;
    };
    uint16_t ch[3];
} led_pwm_t;

#define PKT_BUF_LEN sizeof(_pkt_hack)
#define PKT_MEM (_TYPE) struct __attribute__((__packed__)) { char _pad[PKT_BUF_LEN-sizeof(_TYPE)]; _TYPE; }
 
union {
    adc_res_t;
    led_pwm_t;
} _pkt_hack;

typedef struct {
    union {
        PKT_MEM(dsc_inf_t);
        PKT_MEM(adc_res_t);
        PKT_MEM(led_pwm_t);
    };
    char _end[0];
} pkt_t;

#undef PKT_BUF_LEN
#undef PKT_MEM

/* protocol handling declarations */
void ucarx_handler(void) __attribute__((interrupt(USCIAB0RX_VECTOR)));
static int handle_discovery_packet(char *p, pkt_t *pkt, rx_state_t *state);
static int handle_command(int broadcast, pkt_t *pkt, rx_state_t *state);

void ucarx_handler() {
	static void *state;
	static char* p;
	static struct {
        int escaped:1;
        int broadcast:1;
        unsigned int cmd:6;
    } bits;
	static pkt_t pkt;

    char c = UCA0RXBUF;
    
    if (bits.escaped) {
        if (c == '?') {
            state = &&preamble;
            p = pkt.end;
            return;
        }
    } else if (c == '\\') {
        bits.escaped = 1;
        return;
    }

    goto *state;

preamble:
    if (c == BROADCAST_ADDRESS) {
        bits.broadcast = 1;
        state = &&packet_pre;
    } else if (c == current_address) {
        bits.broadcast = 0;
        state = &&packet_pre;
    } else if (c == DISCOVERY_ADDRESS) {
        p = &pkt.dsc_inf_t;
        state = &&discovery;
    } else {
        state = &&idle;
    }
    return;

discovery:
    /* We are abusing bits.broadcast here to store whether the mask ends in the middle of a byte */
    bits.broadcast = c&1;
    /* We are abusing p here as a simple integer to store the number of remaining mask bytes */
    p = (void*) ((c+1)>>1);
    state = (c&1) ? &&discovery_nibble : &&discovery_match;
    return;

discovery_match:
    if (CONFIG_MAC[(size_t) p] != c)
        state = &&idle;
    else if (!--p)
        state = bits.broadcast ? &&discovery_nibble : &&discovery_response;
    return;

discovery_nibble:
    if ((CONFIG_MAC[(size_t) p]&0xF0) != (c&0xF0))
        state = &&idle;
    else
        state = &&discovery_response;
    return;

discovery_response:
    current_address = c;
    state = &&idle;
    rs485_enable();
    uart_putc(0xFF); /* "I'm here!" */
    rs485_disable();
    return;

packet_pre:
    int n = prepare_packet(bits.broadcast, c);
    bits.cmd = c;
    if (n > 0) {
        state = &&packet_post;
        p = pkt.end - n;
    } else if (n < 0) {
        state = &&packet_wait;
        p = pkt.end + n;
    } else {
        handle_command(bits.broadcast, bits.cmd, &pkt);
        state = &&idle;
    }
    return;

packet_wait:
    if (bits.escaped && c == '!') {
        *p++ = c;
        if (p == pkt.end) {
            p -= wait_elapsed(bits.broadcast, bits.cmd);
            state = &&packet_post;
        }
    }
    return;

packet_post:
    *p++ = c;
    if (p == pkt.end) {
        handle_command(bits.broadcast, bits.cmd, &pkt);
        state = &&idle;
    }

idle:
    return;
}

inline static int prepare_packet(int broadcast, char cmd) {
    if (broadcast) {
        switch (cmd) {
        case CMD_SET_PWM:
            return sizeof(led_pwm_t);

        case CMD_GET_DATA:
            /* A bit of arcane information on this: nodes are numbered continuously beginning from one by the master.
             * payload position in the cluster-response is determined by the node's address. */
            return -(current_address-1);
        }
    }
    return 0;
}

inline static int wait_elapsed(int broadcast, char cmd) {
    switch (cmd) {
        case CMD_GET_DATA:
            return sizeof(adc_res_t);
    }
    return 0;
}

inline static int handle_command(int broadcast, int cmd, pkt_t* pkt) {
    switch (cmd) {
    case CMD_SET_PWM:
        TA1CCR0 = pkt->led_pwm_t.r;
        /*TA1CCR1 = pkt->led_pwm_t.g;*/
        TA1CCR2 = pkt->led_pwm_t.b;
        break;

    case CMD_STATUS_OFF:
    case CMD_STATUS_GREEN:
    case CMD_STATUS_YELLOW:
    case CMD_STATUS_BOTH:
        P2OUT      &= ~((1<<ST_GRN_PIN) | (1<<ST_YLW_PIN));
        P2OUT      |= ((cmd&1) ? (1<<ST_GRN_PIN) : 0) | ((cmd&2) ? (1<<ST_YLW_PIN) : 0);
        break;

    case CMD_FLASH_LED:
        P2DIR      |= (1<<ST_YLW_PIN);
        P2OUT      |= (1<<ST_YLW_PIN);
        __delay_cycles(8000000);
        P2OUT      &= ~(1<<ST_YLW_PIN);
        P2DIR      &= ~(1<<ST_YLW_PIN);
        break;

    case CMD_GET_DATA:
        __delay_cycles(16000);
        rs485_enable();
        __delay_cycles(16000);
        send_sof();
        escaped_send(&adc_res);
        __delay_cycles(16000);
        rs485_disable();
        break;

    case CMD_ACQUIRE:
        kick_adc();
        /* ...and go to sleep. */
        _BIS_SR_IRQ(LPM0_bits); /* HACK this only works because this function is always inlined into the ISR */
        break;
    }
}

