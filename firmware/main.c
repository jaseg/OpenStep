
#include <msp430g2153.h>
#include <stdint.h>

#define PIEZO_1_CH	0
#define PIEZO_2_CH	4
#define PIEZO_3_CH	6
#define PIEZO_HIGHEST_CHANNEL	PIEZO_3_CH

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

void uart_putadc(uint16_t val){
	uart_putc(nibble_to_hex(val>>8));
	uart_putc(nibble_to_hex(val>>4&0xF));
	uart_putc(nibble_to_hex(val&0xF));
}

int main(void){
	WDTCTL = WDTPW | WDTHOLD; /* disable WDT */

	/* ADC setup */
	uint16_t results[PIEZO_HIGHEST_CHANNEL+1];

	DCOCTL = CALDCO_16MHZ;
	BCSCTL1 = CALBC1_16MHZ;
	ADC10CTL0 |= SREF0 | ADC10SHT1 | ADC10SHT0 | MSC | ADC10ON;
	ADC10CTL1 |= CONSEQ0 | CONSEQ1 | (PIEZO_HIGHEST_CHANNEL<<12);
	ADC10AE0 |= (1<<PIEZO_1_CH) | (1<<PIEZO_2_CH) | (1<<PIEZO_3_CH);
	ADC10CTL0 |= ADC10SC | ENC;

	ADC10DTC0 |= ADC10CT;
	ADC10DTC1 = 3;
	ADC10SA = results;

	/* UART setup */
	P1SEL |= 0x06;
	P1SEL2 |= 0x06;

	/* set for 115.2kBd @ 16MHz */
	UCA0CTL1 |= UCSSEL1;
	UCA0BR0 = 138;
	UCA0BR1 = 0;
	UCA0MCTL = UCBRS2;
	UCA0CTL1 &= ~UCSWRST;

	/* Set global interrupt enable flag */
	_BIS_SR(GIE);

	while(42){
		uint16_t ch1 = results[PIEZO_1_CH];
		uint16_t ch2 = results[PIEZO_2_CH];
		uint16_t ch3 = results[PIEZO_3_CH];
		uart_putc('R');
		uart_putc(' ');
		uart_putadc(ch1);
		uart_putc(' ');
		uart_putadc(ch2);
		uart_putc(' ');
		uart_putadc(ch3);
		uart_putc('\n');
	}
	return 0;
}
