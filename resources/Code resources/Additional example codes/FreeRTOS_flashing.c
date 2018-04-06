#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"


#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"



#define PGFlash_Task_P      (tskIDLE_PRIORITY+1)
TaskHandle_t PGFlash;
#define PRFlash_Task_P      (tskIDLE_PRIORITY+2)
TaskHandle_t PRFlash;


void PGFlash_Task(void *pvParameters ){
	while(1){

		IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0x55);
		vTaskDelay(500); //delay for 500 milliseconds
		IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0xaa);
		vTaskDelay(500);
	}
}


void PRFlash_Task(void *pvParameters ){
	while(1){

		IOWR_ALTERA_AVALON_PIO_DATA(RED_LEDS_BASE, 0x5555);
		vTaskDelay(800); //delay for 800 milliseconds
		IOWR_ALTERA_AVALON_PIO_DATA(RED_LEDS_BASE, 0xaaaa);
		vTaskDelay(800);
	}
}



int main()
{

	xTaskCreate( PGFlash_Task, "0", configMINIMAL_STACK_SIZE, NULL, PGFlash_Task_P, &PGFlash );
	xTaskCreate( PRFlash_Task, "1", configMINIMAL_STACK_SIZE, NULL, PRFlash_Task_P, &PRFlash );

	vTaskStartScheduler();

	while(1){

	}

  return 0;
}
