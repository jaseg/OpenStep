#include "uart-util.h"
#include "main.h"

#define CMD_GET_DATA          1
#define CMD_SET_PWM           2
#define CMD_FLASH_LED         3
#define CMD_STATUS_OFF        4
#define CMD_STATUS_GREEN      5
#define CMD_STATUS_YELLOW     6
#define CMD_STATUS_BOTH       7
#define CMD_ACQUIRE           8 /* usually followed by a break of a few milliseconds */

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
#define PKT_MEM(type, name) struct __attribute__((__packed__)) { char _pad_##type[PKT_BUF_LEN-sizeof(type)]; type name; }
 
union {
    adc_res_t adc;
    led_pwm_t pwm;
} _pkt_hack;

typedef struct {
    union {
        PKT_MEM(adc_res_t, adc);
        PKT_MEM(led_pwm_t, pwm);
    };
    char _end[0];
} pkt_t;

#undef PKT_BUF_LEN
#undef PKT_MEM

/* protocol handling declarations */
void ucarx_handler(void) __attribute__((interrupt(USCIAB0RX_VECTOR)));
static int prepare_packet(int broadcast, char cmd);
static int wait_elapsed(int broadcast, char cmd);
static int handle_command(int broadcast, int cmd, pkt_t* pkt);

/* Please excuse the state machine hackery. This is supposed to be *fast*. Fun fact: three more states and we're nearly
 * as complex as TCP. This function is compiled about 400 byte large. */
void ucarx_handler() {
	static void *state = &&idle;
	static char* p;
	static struct {
        int escaped:1;
        int broadcast:1;
        unsigned int cmd:6;
    } bits = {0, 0, 0};
	static pkt_t pkt;

    unsigned char c = UCA0RXBUF;
    
    if (bits.escaped) {
        if (c == '?') {
            state = &&preamble;
            p = pkt._end;
            goto exit;
        }
    } else if (c == '\\') {
        bits.escaped = 1;
        goto exit;
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
        state = &&discovery_mask;
    } else {
        state = &&idle;
    }
    goto exit;

/* Bus enumeration stuff */
discovery_mask:
    /* We are abusing bits.broadcast here to store whether the mask ends in the middle of a byte */
    /* We are abusing p here as a simple integer to store the number of remaining mask bytes */
    p = (void*) (c>>1);
    state = (c&1) ? &&discovery_nibble : &&discovery_match;
    goto exit;

    // 5f b3 5d dd 5b b6 65 89
discovery_nibble:
    if ((CONFIG_MAC[(unsigned int) p]&0xF0) != (c&0xF0))
        state = &&idle;
    else
        state = p ? &&discovery_match : &&discovery_response;
    goto exit;

discovery_match:
    if (CONFIG_MAC[(unsigned int) --p] != c)
        state = &&idle;
    else if (!p)
        state = &&discovery_response;
    goto exit;

discovery_response:
    current_address = c;
    state = &&idle;
    rs485_enable();
    uart_putc(0xFF); /* "I'm here!" */
    rs485_disable();
    goto exit;

/* Command packet handling */
packet_pre:
    bits.cmd = c;
    int n = prepare_packet(bits.broadcast, c);
    if (n > 0) {
        state = &&packet_post;
        p = pkt._end - n;
    } else if (n < 0) {
        state = &&packet_wait;
        p = pkt._end + n;
    } else {
        if (handle_command(bits.broadcast, bits.cmd, &pkt))
            _BIS_SR_IRQ(LPM0_bits);
        state = &&idle;
    }
    goto exit;

packet_wait:
    if (bits.escaped && c == '!') {
        p++;
        if (p == pkt._end) {
            p -= wait_elapsed(bits.broadcast, bits.cmd);
            state = &&packet_post;
        }
    }
    goto exit;

packet_post:
    *p++ = c;
    if (p == pkt._end) {
        if (handle_command(bits.broadcast, bits.cmd, &pkt))
            _BIS_SR_IRQ(LPM0_bits);
        state = &&idle;
    }

idle:
exit:
    return;
}

static int prepare_packet(int broadcast, char cmd) {
    if (broadcast) {
        switch (cmd) {
        case CMD_SET_PWM:
            return sizeof(led_pwm_t);

        case CMD_GET_DATA:
            /* A bit of arcane information on this: nodes are numbered continuously beginning from one by the master.
             * payload position in the cluster-response is determined by the node's address. */
            return -current_address;
        }
    }
    return 0;
}

static int wait_elapsed(int broadcast, char cmd) {
    switch (cmd) {
        case CMD_GET_DATA:
            return sizeof(adc_res_t);
    }
    return 0;
}

static int handle_command(int broadcast, int cmd, pkt_t* pkt) {
    switch (cmd) {
    case CMD_SET_PWM:
        TA1CCR0 = pkt->pwm.r;
        /*TA1CCR1 = pkt->pwm.g;*/
        TA1CCR2 = pkt->pwm.b;
        break;

    case CMD_STATUS_OFF:
    case CMD_STATUS_GREEN:
    case CMD_STATUS_YELLOW:
    case CMD_STATUS_BOTH:
        P2OUT      &= ~((1<<ST_GRN_PIN) | (1<<ST_YLW_PIN));
        P2OUT      |= ((cmd&1) ? (1<<ST_GRN_PIN) : 0) | ((cmd&2) ? (1<<ST_YLW_PIN) : 0);
        break;

    case CMD_FLASH_LED:
//        P2DIR      |= (1<<ST_YLW_PIN);
        P2OUT      |= (1<<ST_YLW_PIN);
        __delay_cycles(8000000);
        P2OUT      &= ~(1<<ST_YLW_PIN);
//        P2DIR      &= ~(1<<ST_YLW_PIN);
        break;

    case CMD_GET_DATA: /* FIXME try to get rid of the following delays */
        __delay_cycles(1600);
        rs485_enable();
        send_sof();
        escaped_send(&adc_res);
        rs485_disable();
        break;

    case CMD_ACQUIRE:
        kick_adc();
        /* ...and go to sleep. */
        return 1;
    }
    return 0;
}

