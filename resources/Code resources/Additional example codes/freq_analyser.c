
#include <stdio.h>
#include <unistd.h>

#include "system.h"
#include "sys/alt_irq.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"


void freq_relay(){
	unsigned int temp = IORD(FREQUENCY_ANALYSER_BASE, 0);
	printf("%f Hz\n", 16000/(double)temp);
	return;
}

int main()
{
	alt_irq_register(FREQUENCY_ANALYSER_IRQ, 0, freq_relay);
	while(1){
	  IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0x55);
	  usleep(1000000);
	  IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0xaa);
	  usleep(1000000);
	}

  return 0;
}
