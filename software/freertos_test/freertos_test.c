// Standard includes
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "io.h"

// Scheduler includes
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include "FreeRTOS/queue.h"
#include "FreeRTOS/semphr.h"

#include <altera_avalon_pio_regs.h>
#include <altera_up_avalon_video_pixel_buffer_dma.h>
#include <altera_up_avalon_video_character_buffer_with_dma.h>
#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_keyboard.h"
#include "sys/alt_irq.h"

// Forward declarations
int initOSDataStructs(void);
int initCreateTasks(void);

// Definitions for frequency plot
#define FREQPLT_ORI_X 101		//x axis pixel position at the plot origin
#define FREQPLT_GRID_SIZE_X 5	//pixel separation in the x axis between two data points
#define FREQPLT_ORI_Y 199.0		//y axis pixel position at the plot origin
#define FREQPLT_FREQ_RES 20.0	//number of pixels per Hz (y axis scale)

#define ROCPLT_ORI_X 101
#define ROCPLT_GRID_SIZE_X 5
#define ROCPLT_ORI_Y 259.0
#define ROCPLT_ROC_RES 0.5		//number of pixels per Hz/s (y axis scale)

#define MIN_FREQ 45.0 //minimum frequency to draw
// Definition of Task Stacks
#define   TASK_STACKSIZE       2048

// Definition of Task Priorities
#define VGA_TASK_PRIORITY 				(tskIDLE_PRIORITY+4)
#define CALCULATION_TASK_PRIORITY 		(tskIDLE_PRIORITY+4)
#define FSM_TASK_PRIORITY 				(tskIDLE_PRIORITY+5)
#define KEYBOARD_UPDATE_TASK_PRIORITY 	(tskIDLE_PRIORITY+6)

// Definition of Queue Sizes
#define HW_DATA_QUEUE_SIZE 	100
#define ROC_DATA_QUEUE_SIZE 10
#define KB_DATA_QUEUE_SIZE 	10

// Definition of enums and structs
typedef enum {STABLE, LOAD_MANAGEMENT_MODE, MAINTENANCE_MODE} state;

typedef struct{
    unsigned int x1;
    unsigned int y1;
    unsigned int x2;
    unsigned int y2;
} Line;

// Definition of Semaphore Handles
SemaphoreHandle_t freq_roc_sem;
SemaphoreHandle_t thresholds_sem;

// Definition of Queue Handles
QueueHandle_t HW_dataQ; // contains frequency
QueueHandle_t kb_dataQ; // stores keystrokes

// Global variables
int freq_idx = 99; // used for configuring HW_dataQ with f values and displaying
double freq[100];
double roc[100];

double freq_threshold = 50;
double roc_threshold = 10;
state system_state = STABLE;


// ISR
void freq_relay() {
	#define SAMPLING_FREQ 16000.0

	unsigned int adc_samples = IORD(FREQUENCY_ANALYSER_BASE, 0);	// number of ADC samples
	double new_freq = SAMPLING_FREQ/(double)adc_samples;

	// ROC calculation done in separate Calculation task to minimise ISR time
	if (xQueueSendToBackFromISR(HW_dataQ, &new_freq, NULL) == pdPASS) {
		printf("Queue send successful\n");
		printf("Frequency sent: %f\n", new_freq);
	}
	else {
		//printf("UNSUCCESSFUL\n");
	}
	return;
}

// ISR for keyboard input
// can this all just be done in the ISR?? is there any need for a kb_inputQ/kb_update_task??
void ps2_isr (void* context, alt_u32 id)
{
	char ascii;
	int status = 0;
	unsigned char key = 0;
	KB_CODE_TYPE decode_mode;
	status = decode_scancode (context, &decode_mode , &key , &ascii) ;
	if ( status == 0 ) //success
	{
		if (xQueueSendFromISR(kb_dataQ, &key, pdFALSE) == pdPASS) {
			printf("keycode sent successfully: %x\n", key);
		}
		else {
			printf("Keycode queue full!");
		}
	}
}

void Keyboard_Update_Task(void *pvParameters) {
	unsigned char key;
	printf("in keyboard task!!!");
	while(1) {
		xQueueReceive(kb_dataQ, &key, portMAX_DELAY);
		xSemaphoreTake(thresholds_sem, portMAX_DELAY);

		// adjust thresholds according to keycode
		// 0.5 because every time you press a key it goes twice
		// may need to implement real debouncing w/ timers?? delays??
		if (key == 0x75) { // up arrow
			freq_threshold += 0.5;
			printf("Frequency threshold: %f\n", freq_threshold);
		}
		else if (key == 0x72) { // down arrow
			freq_threshold -= 0.5;
			printf("Frequency threshold: %f\n", freq_threshold);
		}
		else if (key == 0x7d) { // pg up
			roc_threshold += 0.5;
			printf("RoC threshold: %f\n", roc_threshold);
		}
		else if (key == 0x7a) { // pg down
			roc_threshold -= 0.5;
			printf("RoC threshold: %f\n", roc_threshold);
		}

		xSemaphoreGive(thresholds_sem);
	}
}

// ROC Calculation Task
void ROC_Calculation_Task(void *pvParameters) {
	while(1) {
		xQueueReceive(HW_dataQ, freq+freq_idx, portMAX_DELAY); // pops new f value from back of q to freq array
		xSemaphoreTake(freq_roc_sem, portMAX_DELAY);

		// calculate frequency ROC based on example code
		if(freq_idx == 0) { // roc needs two points, account for edge case
			roc[0] = (freq[0]-freq[99]) * 2.0 * freq[0] * freq[99] / (freq[0]+freq[99]);
		} else {
			roc[freq_idx] = (freq[freq_idx]-freq[freq_idx-1]) * 2.0 * freq[freq_idx]* freq[freq_idx-1] / (freq[freq_idx]+freq[freq_idx-1]);
		}

		if (roc[freq_idx] > 100.0){
			roc[freq_idx] = 100.0;
		}

		freq_idx = (++freq_idx) % 100; // point to the next data (oldest) to be overwritten
		xSemaphoreGive(freq_roc_sem);

		// xTaskNotifyGive(); // arg is the task handler for next task
	}
}

// VGA_Task
void VGA_Task(void *pvParameters){
	//initialize VGA controllers
	alt_up_pixel_buffer_dma_dev *pixel_buf;
	pixel_buf = alt_up_pixel_buffer_dma_open_dev(VIDEO_PIXEL_BUFFER_DMA_NAME);
	if(pixel_buf == NULL){
		printf("can't find pixel buffer device\n");
	}
	alt_up_pixel_buffer_dma_clear_screen(pixel_buf, 0);

	alt_up_char_buffer_dev *char_buf;
	char_buf = alt_up_char_buffer_open_dev("/dev/video_character_buffer_with_dma");
	if(char_buf == NULL){
		printf("can't find char buffer device\n");
	}
	alt_up_char_buffer_clear(char_buf);

	//Set up plot axes
	alt_up_pixel_buffer_dma_draw_hline(pixel_buf, 100, 590, 200, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);
	alt_up_pixel_buffer_dma_draw_hline(pixel_buf, 100, 590, 300, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);
	alt_up_pixel_buffer_dma_draw_vline(pixel_buf, 100, 50, 200, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);
	alt_up_pixel_buffer_dma_draw_vline(pixel_buf, 100, 220, 300, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0);

	alt_up_char_buffer_string(char_buf, "Frequency(Hz)", 4, 4);
	alt_up_char_buffer_string(char_buf, "52", 10, 7);
	alt_up_char_buffer_string(char_buf, "50", 10, 12);
	alt_up_char_buffer_string(char_buf, "48", 10, 17);
	alt_up_char_buffer_string(char_buf, "46", 10, 22);

	alt_up_char_buffer_string(char_buf, "df/dt(Hz/s)", 4, 26);
	alt_up_char_buffer_string(char_buf, "60", 10, 28);
	alt_up_char_buffer_string(char_buf, "30", 10, 30);
	alt_up_char_buffer_string(char_buf, "0", 10, 32);
	alt_up_char_buffer_string(char_buf, "-30", 9, 34);
	alt_up_char_buffer_string(char_buf, "-60", 9, 36);

	int j = 0;
	Line line_freq, line_roc;
	char vga_info_buf[50];
	while(1) {

		// print out thresholds
		sprintf(vga_info_buf, "Frequency threshold: %2.1f", freq_threshold);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 12, 42);
		sprintf(vga_info_buf, "ROC threshold: %2.1f ", roc_threshold);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 12, 44);

		// print system state
		if (system_state == STABLE) {
			sprintf(vga_info_buf, "Current state: STABLE");
		}
		else if (system_state == LOAD_MANAGEMENT_MODE) {
			sprintf(vga_info_buf, "Current state: MANAGING LOAD");
		}
		else if (system_state == MAINTENANCE_MODE) {
			sprintf(vga_info_buf, "Current state: MAINTENANCE MODE");
		}
		alt_up_char_buffer_string(char_buf, vga_info_buf, 12, 46);

		//clear old graph to draw new graph
		alt_up_pixel_buffer_dma_draw_box(pixel_buf, 101, 0, 639, 199, 0, 0);
		alt_up_pixel_buffer_dma_draw_box(pixel_buf, 101, 201, 639, 299, 0, 0);

		for(j=0;j<99;++j){ //i here points to the oldest data, j loops through all the data to be drawn on VGA
			if (((int)(freq[(freq_idx+j)%100]) > MIN_FREQ) && ((int)(freq[(freq_idx+j+1)%100]) > MIN_FREQ)){
				//Frequency plot
				line_freq.x1 = FREQPLT_ORI_X + FREQPLT_GRID_SIZE_X * j;
				line_freq.y1 = (int)(FREQPLT_ORI_Y - FREQPLT_FREQ_RES * (freq[(freq_idx+j)%100] - MIN_FREQ));

				line_freq.x2 = FREQPLT_ORI_X + FREQPLT_GRID_SIZE_X * (j + 1);
				line_freq.y2 = (int)(FREQPLT_ORI_Y - FREQPLT_FREQ_RES * (freq[(freq_idx+j+1)%100] - MIN_FREQ));

				//Frequency RoC plot
				line_roc.x1 = ROCPLT_ORI_X + ROCPLT_GRID_SIZE_X * j;
				line_roc.y1 = (int)(ROCPLT_ORI_Y - ROCPLT_ROC_RES * roc[(freq_idx+j)%100]);

				line_roc.x2 = ROCPLT_ORI_X + ROCPLT_GRID_SIZE_X * (j + 1);
				line_roc.y2 = (int)(ROCPLT_ORI_Y - ROCPLT_ROC_RES * roc[(freq_idx+j+1)%100]);

				//Draw
				alt_up_pixel_buffer_dma_draw_line(pixel_buf, line_freq.x1, line_freq.y1, line_freq.x2, line_freq.y2, 0x3ff << 0, 0);
				alt_up_pixel_buffer_dma_draw_line(pixel_buf, line_roc.x1, line_roc.y1, line_roc.x2, line_roc.y2, 0x3ff << 0, 0);
			}
		}

	}
}

// Load Management Task
void Load_Management_Task(void *pvParameters) {

}

int initCreateTasks(void) {
	// 4th arg is to pass to pvParameters
	xTaskCreate(VGA_Task, "VGA_Task", configMINIMAL_STACK_SIZE, NULL, VGA_TASK_PRIORITY, NULL);
	xTaskCreate(ROC_Calculation_Task, "Calculation_Task", configMINIMAL_STACK_SIZE, NULL, CALCULATION_TASK_PRIORITY, NULL);
	xTaskCreate(Load_Management_Task, "FSM_Task", configMINIMAL_STACK_SIZE, NULL, FSM_TASK_PRIORITY, NULL);
	xTaskCreate(Keyboard_Update_Task, "Keyboard_Update_Task", configMINIMAL_STACK_SIZE, NULL, KEYBOARD_UPDATE_TASK_PRIORITY, NULL);
	return 0;
}

int initOSDataStructs(void)
{
	HW_dataQ = xQueueCreate(HW_DATA_QUEUE_SIZE, sizeof(double));
	kb_dataQ = xQueueCreate(KB_DATA_QUEUE_SIZE, sizeof(unsigned char));
	freq_roc_sem = xSemaphoreCreateMutex();
	thresholds_sem = xSemaphoreCreateMutex();
	if (freq_roc_sem == NULL) {
		printf("Creation of vga_sem failed\n");
	}


	return 0;
}

// Initialisations for keyboard
int ps2_init(void) {
	alt_up_ps2_dev * ps2_device = alt_up_ps2_open_dev(PS2_NAME);

	if(ps2_device == NULL){
		printf("can't find PS/2 device\n");
		return 1;
	}

	alt_up_ps2_clear_fifo (ps2_device) ;

	alt_irq_register(PS2_IRQ, ps2_device, ps2_isr);
	// register the PS/2 interrupt
	IOWR_8DIRECT(PS2_BASE,4,1);
}

int main(int argc, char* argv[], char* envp[])
{
	alt_irq_register(FREQUENCY_ANALYSER_IRQ, 0, freq_relay);
	ps2_init();
	initOSDataStructs();
	initCreateTasks();
	vTaskStartScheduler();
	for (;;);

	return 0;
}

