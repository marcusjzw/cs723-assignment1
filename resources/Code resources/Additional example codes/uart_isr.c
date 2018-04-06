#include <stdio.h>
#include <system.h>
#include <string.h>
#include "sys/alt_irq.h"
#include "altera_avalon_uart_regs.h"


void uart_isr(void* context, alt_u32 id){
	char uartCharacter;
	uartCharacter = IORD_ALTERA_AVALON_UART_RXDATA(UART_BASE);
	printf("Received: %c\n", uartCharacter);

	while((IORD_ALTERA_AVALON_UART_STATUS(UART_BASE)&ALTERA_AVALON_UART_STATUS_TRDY_MSK) == 0);
	//send back the next ASCII character
	IOWR_ALTERA_AVALON_UART_TXDATA(UART_BASE, uartCharacter+1);

	return;
}

int main()
{
	//Initialize interrupts
	IOWR_ALTERA_AVALON_UART_CONTROL(UART_BASE, ALTERA_AVALON_UART_CONTROL_RRDY_MSK); //enable receive ready interrupts
	alt_irq_register(UART_IRQ, 0, uart_isr);

	while(1);

	return 0;
}
