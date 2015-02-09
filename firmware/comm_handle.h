#ifndef COMM_HANDLE_H
#define COMM_HANDLE_H

#include "comm.h"
#include "uart.h"
//AVR/MSP430 targets
#define be16toh(x) ((x>>8)|(x<<8))
//from <bits/byteswap.h>
# define be64toh(x) \
   (__extension__ ((((x) & 0xff00000000000000ull) >> 56) \
				 | (((x) & 0x00ff000000000000ull) >> 40) \
				 | (((x) & 0x0000ff0000000000ull) >> 24) \
				 | (((x) & 0x000000ff00000000ull) >> 8) \
				 | (((x) & 0x00000000ff000000ull) << 8) \
				 | (((x) & 0x0000000000ff0000ull) << 24) \
				 | (((x) & 0x000000000000ff00ull) << 40) \
				 | (((x) & 0x00000000000000ffull) << 56)))
#define comm_debug_print(...)
#define comm_debug_print2(...)

static inline void comm_handle(uint8_t c){
	static struct {
		uint8_t receiving:1;
		uint8_t escaped:1;
	} state;
	static uint8_t* p;
	static uint8_t* end;
	static uint16_t current_address = ADDRESS_INVALID;
	static struct {
		uint8_t node_id;
		uint8_t cmd;
        struct {
            uint8_t new_id;
            uint64_t mac_mask;
        } discovery_payload;
        uint8_t end[];
	} pkt;

	if (state.escaped) {
        state.escaped = 0;
		if (c == '#') {
			state.receiving = 1;
			p = &pkt;
			end = &pkt.discovery_payload;
			return;
		}
	} else if (c == '\\') {
        state.escaped = 1;
        return;
	}
	//escape sequence handling completed. 'c' now contains the next char of the payload.

	if (!state.receiving)
		return;
	*p++ = c;

	if(p == end) {
        state.receiving = 0;
        if (pkt.node_id == ADDRESS_DISCOVERY) {
            if (p == &pkt.end) {
                if ((CONFIG_MAC & (0xFFFFFFFFFFFFFFFFull>>(pkt->cmd&63))) == be64toh(pkt.discovery_payload.mac_mask)) {
                    uart_putc(0xFF); /* "I'm here!" */
                    current_address = pkt->discovery_payload.new_id;
                }
            } else { /* p == &pkt.discovery_payload */
                state.receiving = 1;
                end = &pkt.end;
            }
        } else if (pkt.node_id == current_address) {
            switch (pkt.cmd) {
                case ECHO_NAME:
                    escaped_uart_puts(CONFIG_NAME);
                    break;
            }
        }
    }
}

#endif//COMM_HANDLE_H

