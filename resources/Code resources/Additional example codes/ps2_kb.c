#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "altera_up_avalon_ps2.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

void ps2_isr(void* ps2_device, alt_u32 id){
	unsigned char byte;
	alt_up_ps2_read_data_byte_timeout(ps2_device, &byte);
	printf("Scan code: %x\n", byte);
}

int main()
{
	alt_up_ps2_dev * ps2_device = alt_up_ps2_open_dev(PS2_NAME);

	if(ps2_device == NULL){
		printf("can't find PS/2 device\n");
		return 1;
	}

	alt_up_ps2_enable_read_interrupt(ps2_device);
	alt_irq_register(PS2_IRQ, ps2_device, ps2_isr);

	while(1){
		IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0xaa);
		usleep(1000000);
		IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0x55);
		usleep(1000000);
	}

	return 0;
}
