/* This example shows how a queue can be used in FreeRTOS.
 * The FlashTask() causes the green LEDs to flash and is blocked by the queue Q_flash.
 * The green LEDs flash once when a message is received by the queue. In this example, there are two
 * sources of messages. One is from the push button ISR which is raise by pushing keys 1, 2, or 3,
 * and the second is from the timer call back function running at 1 Hz using the FreeRTOS software timer.
 */

#include <stdio.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include "FreeRTOS/timers.h"
#include "FreeRTOS/queue.h"
TimerHandle_t timer;

#define Flash_Task_P      (tskIDLE_PRIORITY+1)
TimerHandle_t Timer_Reset;

static QueueHandle_t Q_flash;



void FlashTask(void *pvParameters ){
	int temp;
	while(1){
		if(xQueueReceive( Q_flash, &temp, portMAX_DELAY ) == pdTRUE )
			IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0xFF^IORD_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE));
	}
}

void PushButtonISR(){
	int temp = 0;
	xQueueSendToBackFromISR( Q_flash, &temp, pdFALSE ); //push button (manual trigger) causes LED to flash
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); //write 1 to clear all detected falling edges
	return;
}


void vTimerCallback(TimerHandle_t t_timer){ //Timer time up (automatic 1 Hz trigger) causes LED to flash
	int temp = 3;
	xQueueSendToBack( Q_flash, &temp, pdFALSE );
}


int main()
{
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PUSH_BUTTON_BASE, 0x7); //enable interrupt for all three push buttons (Keys 1-3 -> bits 0-2)
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); //write 1 to edge capture to clear pending interrupts
	alt_irq_register(PUSH_BUTTON_IRQ, 0, PushButtonISR);  //register ISR for push button interrupt request
	Q_flash = xQueueCreate( 100, sizeof(int) );

	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0x55); //initialize LEDs

	timer = xTimerCreate("Timer Name", 1000, pdTRUE, NULL, vTimerCallback);

	if (xTimerStart(timer, 0) != pdPASS){
		printf("Cannot start timer");
	}
	xTaskCreate( FlashTask, "0", configMINIMAL_STACK_SIZE, NULL, Flash_Task_P, &Timer_Reset );

	vTaskStartScheduler();
	while(1){

	}

  return 0;
}
