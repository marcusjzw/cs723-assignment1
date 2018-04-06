#include <unistd.h> //for usleep
#include "system.h"
#include "sys/alt_irq.h"
#include "altera_avalon_pio_regs.h"


void push_button_irq(){
	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, IORD_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE));
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); //write 1 to clear all detected falling edges
	return;
}

int main()
{
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PUSH_BUTTON_BASE, 0x7); //enable interrupt for all three push buttons (Keys 1-3 -> bits 0-2)
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); //write 1 to edge capture to clear pending interrupts
	alt_irq_register(PUSH_BUTTON_IRQ, 0, push_button_irq);  //register ISR for push button interrupt request

	int sw_result;
	while(1){
		sw_result=IORD_ALTERA_AVALON_PIO_DATA(SLIDE_SWITCH_BASE); //read slide switches
		IOWR_ALTERA_AVALON_PIO_DATA(RED_LEDS_BASE, sw_result);    //light red LEDs according to switch positions
		usleep(500000); //wait 0.5 seconds
	}
	return 0;
}
