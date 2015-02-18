#ifndef _UART_UTIL_H
#define _UART_UTIL_H

#include <msp430g2153.h>
#include <stdint.h>
#include "main.h"

inline void uart_putc(char c) {
    while (!(IFG2&UCA0TXIFG)) ;
    UCA0TXBUF = c;
}

inline void rs485_enable() {
    P1OUT      |= (1<<RS485_EN_PIN);
    uart_putc(0);
}

inline void rs485_disable() {
    while (UCA0STAT&UCBUSY) ;
    P1OUT      &= ~(1<<RS485_EN_PIN);
}

inline int uart_getc() {
    while (!(IFG2&UCA0RXIFG)) ;
    return UCA0RXBUF;
}

inline char nibble_to_hex(uint8_t nibble) {
    return (nibble < 0xA) ? ('0'+nibble) : ('A'+nibble-0xA);
}

inline void uart_puthword(uint16_t val){
    uart_putc(nibble_to_hex(val>>12&0xF));
    uart_putc(nibble_to_hex(val>>8&0xF));
    uart_putc(nibble_to_hex(val>>4&0xF));
    uart_putc(nibble_to_hex(val&0xF));
}

inline void escaped_putc(uint8_t c) {
    if(c == '\\')
        uart_putc(c);
    uart_putc(c);
}

inline void escaped_puts(char *c) {
    do{
        escaped_putc(*c);
    }while(*c++);
}

inline void escaped_sendbuf(char *buf, unsigned int len) {
    for (char *end = buf+len; buf<end; buf++)
        escaped_putc(*buf);
}

#define escaped_send(obj) escaped_sendbuf((char*)(obj), sizeof(*(obj)))

#endif//_UART_UTIL_H
