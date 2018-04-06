/* This example shows how to use the software timer in FreeRTOS
 * 1 Hz timer time up causes the call back function to execute,
 * which in turn flashes the green LED. If any of the push button is pressed,
 * the timer resets and has to wait for a new second for the LED to flash.
 */

#include <stdio.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include "FreeRTOS/timers.h"


#define Timer_Reset_Task_P      (tskIDLE_PRIORITY+1)
TimerHandle_t timer;
TaskHandle_t Timer_Reset;


void Timer_Reset_Task(void *pvParameters ){ //reset timer if any of the push button is pressed
	while(1){
		if (IORD_ALTERA_AVALON_PIO_DATA(PUSH_BUTTON_BASE) != 0x7){
			xTimerReset( timer, 10 );
		}

	}
}


void vTimerCallback(xTimerHandle t_timer){ //Timer flashes green LEDs
	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0xFF^IORD_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE));
}


int main()
{


	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0x55);

	timer = xTimerCreate("Timer Name", 1000, pdTRUE, NULL, vTimerCallback);

	if (xTimerStart(timer, 0) != pdPASS){
		printf("Cannot start timer");
	}
	xTaskCreate( Timer_Reset_Task, "0", configMINIMAL_STACK_SIZE, NULL, Timer_Reset_Task_P, &Timer_Reset );

	vTaskStartScheduler();
	while(1){

	}

  return 0;
}
