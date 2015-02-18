#include "uart-util.h"
#include "main.h"

#define CMD_GET_DATA          1
#define CMD_SET_LEDS          2

#define BCMD_SET_LEDS         253
#define BCMD_GET_DATA         254
#define BCMD_ACQUIRE          255 /* usually followed by a break of a few milliseconds */

#define DISCOVERY_ADDRESS     0xCC
#define INVALID_ADDRESS       0xFE
#define BROADCAST_ADDRESS     0xFF


uint16_t current_address = INVALID_ADDRESS;

typedef struct {
    uint8_t receiving:1;
    uint8_t just_counting:1;
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
			state.just_counting = 0;
			p = (char*)&pkt;
			end = (char*)&pkt.payload;
			return;
		}
	} else if (c == '\\') {
        state.escaped = 1;
        return;
	}
	//escape sequence handling completed. c now contains the next char of the payload.

	if (!state.receiving)
		return;
    if (!state.just_counting)
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

        rs485_enable();
        uart_putc(0xFF); /* "I'm here!" */
        rs485_disable();
        current_address = pkt->payload.discovery.new_id;
    }
    return 0;
}

inline static unsigned int handle_command_packet(pkt_t *pkt, rx_state_t *state) {
    switch (pkt->cmd) {
        case CMD_GET_DATA:
            rs485_enable();
            escaped_send(&adc_res);
            rs485_disable();
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
            if (!state->just_counting) {
                if (current_address == 0 ) {
                    rs485_enable();
                    escaped_send(&adc_res);
                    rs485_disable();
                } else {
                    state->just_counting = 1;
                    /* bit of arcane information on this: nodes are numbered continuously beginning from one by the
                     * master. payload position in the cluster-response is determined by the node's address. */
                    return sizeof(pkt->payload.adc)*current_address;
                }
            } else {
                state->just_counting = 0;
                rs485_enable();
                escaped_send(&adc_res);
                rs485_disable();
            }
            break;
        case BCMD_ACQUIRE:
            kick_adc();
            /* ...and go to sleep. */
            _BIS_SR_IRQ(LPM0_bits); /* HACK this only works because this function is always inlined into the ISR */
            break;
    }
    return 0;
}

