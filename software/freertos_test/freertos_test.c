// Standard includes
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "io.h"
#include "math.h"

// Scheduler includes
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"
#include "FreeRTOS/queue.h"
#include "FreeRTOS/semphr.h"
#include "FreeRTOS/timers.h"

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
#define VGA_TASK_PRIORITY 				(tskIDLE_PRIORITY+1)
#define CALCULATION_TASK_PRIORITY 		(tskIDLE_PRIORITY+4)
#define FSM_TASK_PRIORITY 				(tskIDLE_PRIORITY+3)
#define KEYBOARD_UPDATE_TASK_PRIORITY 	(tskIDLE_PRIORITY+2)

// Definition of Queue Sizes
#define HW_DATA_QUEUE_SIZE 	100
#define KB_DATA_QUEUE_SIZE 	10

// Definition of system parameters
#define SAMPLING_FREQ 16000.0
#define NO_OF_LOADS 5
#define TIMER_PERIOD (500 / portTICK_RATE_MS)

// Macro to check if bit is set
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

// Definition of enums and structs
typedef enum {NORMAL_OPERATION, LOAD_MGMT_MONITOR_STABLE, LOAD_MGMT_MONITOR_UNSTABLE, MAINTENANCE_MODE} state;

typedef struct{
    unsigned int x1;
    unsigned int y1;
    unsigned int x2;
    unsigned int y2;
} Line;

typedef enum { false, true } bool;

// Definition of RTOS Handles
SemaphoreHandle_t freq_roc_sem; // mutex to protect freq and roc global arrays - written to in RoC_Calculation task, read in VGA task
SemaphoreHandle_t thresholds_sem; // mutex to protect threshold global vars - written in kb update task, read in vga task & roc calculation task
SemaphoreHandle_t shed_sem; // mutex to protect shedding variables - written in roc calculation task, read in vga task, written and read to in fsm task

QueueHandle_t HW_dataQ; // contains frequency values from analyser
QueueHandle_t kb_dataQ; // stores keystrokes

TimerHandle_t fsm_timer;

// Global variables

// Related to frequency and RoC values
int freq_idx = 99; // used for configuring HW_dataQ with f values and displaying
double freq[100];
double roc[100];

// Related to system thresholds and states
double freq_threshold = 50; 
double roc_threshold = 10;
bool system_stable = true; // system_stable is manipulated when thresholds are good/bad
state system_state = NORMAL_OPERATION; // note: not the same as system_stable, system_state describes current mode of operation
state prev_state;
static bool load_states[NO_OF_LOADS];
static bool sw_load_states[NO_OF_LOADS];
bool timer_expired_flag = false; // high when 500ms timer expires, does not need a sem since data R/W on here is atomic and done by one task

// Related to timing mechanisms for shedding

volatile unsigned int time_before_shed = 0;
unsigned int shed_time = 0;
unsigned int shed_time_measurements[5] = {0}; // init so no garbage values screwing up avg
unsigned int min_shed_time = 0;
unsigned int max_shed_time = 0;
float avg_shed_time = 0;
bool array_filled = 0;
unsigned int shed_count = 0;




// ISR for capturing freq data from analyser
void freq_relay() {
	unsigned int adc_samples = IORD(FREQUENCY_ANALYSER_BASE, 0);	// number of ADC samples
	double new_freq = SAMPLING_FREQ/(double)adc_samples;
	// ROC calculation done in separate Calculation task to minimise ISR time
	xQueueSendToBackFromISR(HW_dataQ, &new_freq, NULL);
	return;
}

// ISR for keyboard input
void ps2_isr (void* context, alt_u32 id)
{
	char ascii;
	int status = 0;
	unsigned char key = 0;
	KB_CODE_TYPE decode_mode;
	status = decode_scancode (context, &decode_mode , &key , &ascii) ;
	if ( status == 0 ) //success
	{
		xQueueSendFromISR(kb_dataQ, &key, pdFALSE);
	}
}

// ISR to enable maintenance mode
void button_irq(void* context, alt_u32 id)
{
	if (IORD_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE) == 4) {
		if (system_state != MAINTENANCE_MODE) {
			prev_state = system_state; // save previous state
			system_state = MAINTENANCE_MODE;

		}
		else {
			system_state = prev_state;
		}
	}
   //clears the edge capture register
  IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7);
  return;
}

void Keyboard_Update_Task(void *pvParameters) {
	unsigned char key;
	while(1) {
		xQueueReceive(kb_dataQ, &key, portMAX_DELAY);
		xSemaphoreTake(thresholds_sem, portMAX_DELAY);

		// adjust thresholds according to keycode
		// 0.5 because every time you press a key it goes twice
		if (key == 0x75) { // up arrow
			freq_threshold += 0.5;
		}
		else if (key == 0x72) { // down arrow
			freq_threshold -= 0.5;
		}
		else if (key == 0x7d) { // pg up
			roc_threshold += 0.5;
		}
		else if (key == 0x7a) { // pg down
			roc_threshold -= 0.5;
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
		xSemaphoreGive(freq_roc_sem);

		// also update whether system is stable or not, done here since it's got both freq and roc
		xSemaphoreTake(thresholds_sem, portMAX_DELAY);
		if (((freq[freq_idx] < freq_threshold) || (fabs(roc[freq_idx]) >= roc_threshold)) && (system_state != MAINTENANCE_MODE)) {
			xSemaphoreTake(shed_sem, portMAX_DELAY);
			time_before_shed = xTaskGetTickCountFromISR(); // instability will first be detected here, so get t=0 from here 
			xSemaphoreGive(shed_sem);
			system_stable = false;
		}
		else {
			system_stable = true;
		}
		xSemaphoreGive(thresholds_sem);
		freq_idx = (++freq_idx) % 100; // point to the next data (oldest) to be overwritten
	}
}

void update_shed_stats() {
	xSemaphoreTake(shed_sem, portMAX_DELAY);
	shed_time = xTaskGetTickCount() - time_before_shed;
	int i = 0;

	if (shed_time_measurements[0] == 0) { // first assignment of minimum should be the first shed time
		min_shed_time = shed_time;
	}

	// when not all array elements have been filled yet
	if (array_filled == 0) {
		for (i = 0; i < 5; i++) {
			if (shed_time_measurements[i] == 0) { // if array element has yet to be assigned
				if (shed_time == 0) {
					shed_time = 1; // round up
				}
				shed_time_measurements[i] = shed_time;
				shed_count++;
				if (i == 4) { // if we are on the last array element
					array_filled = 1;
				}
				break;
			}
		}
	}
	else {
		for (i = 0; i < 4; i++) {
			// if array is full oldest element discards, adding new shed time to end of array
			shed_time_measurements[i] = shed_time_measurements[i+1]; // shift elements to the left
		}
		if (shed_time == 0) {
			shed_time = 1; // round up
		}
		shed_time_measurements[4] = shed_time;
	}
	// calculate running minimum and maximum
	for (i = 0; i < 5; i++) {
		if (shed_time_measurements[i] < min_shed_time && shed_time_measurements[i] != 0) {
			min_shed_time = shed_time_measurements[i];
		}
		if (shed_time_measurements[i] > max_shed_time) {
			max_shed_time = shed_time_measurements[i];
		}
	}
	// calculate average
	unsigned int sum = 0;
	for (i = 0; i < 5; i++) {
		sum += shed_time_measurements[i];
	}
	avg_shed_time = (float)sum/(float)shed_count;
	xSemaphoreGive(shed_sem);
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
		xSemaphoreTake(thresholds_sem, portMAX_DELAY);
		sprintf(vga_info_buf, "Frequency threshold: %2.1f", freq_threshold);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 40);
		sprintf(vga_info_buf, "ROC threshold: %2.1f ", roc_threshold);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 42);
		xSemaphoreGive(thresholds_sem);
		// print system state
		if (system_state == NORMAL_OPERATION) {
			alt_up_char_buffer_string(char_buf, "System state: Normal operation           ", 4, 44);
		}
		else if (system_state == LOAD_MGMT_MONITOR_UNSTABLE) {
			alt_up_char_buffer_string(char_buf, "System state: Load mgmt, monitor unstable", 4, 44);
		}
		else if (system_state == LOAD_MGMT_MONITOR_STABLE) {
			alt_up_char_buffer_string(char_buf, "System state: Load mgmt, monitor stable  ", 4, 44);
		}
		else if (system_state == MAINTENANCE_MODE) {
			alt_up_char_buffer_string(char_buf, "System state: Maintenance mode           ", 4, 44);
		}
		if (system_stable == true) {
			alt_up_char_buffer_string(char_buf, "System is stable    ", 4, 46);
		}
		else {
			alt_up_char_buffer_string(char_buf, "System is not stable", 4, 46);
		}

		// print shed times
		xSemaphoreTake(shed_sem, portMAX_DELAY);
		sprintf(vga_info_buf, "Time taken for initial load shed: %d ms   ", shed_time);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 48);
		sprintf(vga_info_buf, "Last 5 initial load sheds: %d ms, %d ms, %d ms, %d ms, %d ms ", shed_time_measurements[0], shed_time_measurements[1], shed_time_measurements[2], shed_time_measurements[3], shed_time_measurements[4]);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 50);
		sprintf(vga_info_buf, "Minimum shed time: %d ms   ", min_shed_time);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 52);
		sprintf(vga_info_buf, "Maximum shed time: %d ms   ", max_shed_time);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 54);
		sprintf(vga_info_buf, "Average shed time: %2.1f ms   ", avg_shed_time);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 56);
		xSemaphoreGive(shed_sem);
		unsigned int uptime = xTaskGetTickCount()/1000;
		// System active time
		sprintf(vga_info_buf, "System uptime: %d m %d s    ", uptime/60, uptime%60);
		alt_up_char_buffer_string(char_buf, vga_info_buf, 4, 58);

		//clear old graph to draw new graph
		alt_up_pixel_buffer_dma_draw_box(pixel_buf, 101, 0, 639, 199, 0, 0);
		alt_up_pixel_buffer_dma_draw_box(pixel_buf, 101, 201, 639, 299, 0, 0);
		xSemaphoreTake(freq_roc_sem, portMAX_DELAY);
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
		xSemaphoreGive(freq_roc_sem);
		// vTaskDelay(15);
	}
}



/*
 * Red LEDs essentially correspond to the load state. In all tasks (including maintenance), red LEDs show this.
 * Green LEDs only exist in the fsm_task, and turns on when the relay SHEDS a load, turns off when relay RECONNECTS a load.
 *
 * From assignment:
 * Use green LEDs to represent whether the load is being switched off by the relay. For example, red LED 7 and
 * green LED 7 correspond to the same load. If the relay sheds that load, the red LED should turn off, and the
 * green one should turn on. Once the relay turns the load back on, the green LED should go off and the red LED
 * goes back on.
 */

// Manages operation of switches
// i.e. in Normal Operation or Maintenance Mode, loads can be switched on/off freely
// However under Load Management Mode, switches can only turn off loads
void update_loads_from_switches() {
	unsigned long switch_cfg = IORD_ALTERA_AVALON_PIO_DATA(SLIDE_SWITCH_BASE);
	unsigned int i;
	if ((system_state == NORMAL_OPERATION) || (system_state == MAINTENANCE_MODE)) { // can turn on or off loads w/ switches freely
		// load state (red LEDs) reflects whatever the switch config is
		for (i = 0; i < NO_OF_LOADS; i++) {
			if (CHECK_BIT(switch_cfg, i)) { // if bit is set
				load_states[i] = 1;
				sw_load_states[i] = 1;
			}
			else {
				load_states[i] = 0;
				sw_load_states[i] = 0;
			}
		}
	}
	else if ((system_state == LOAD_MGMT_MONITOR_UNSTABLE) || (LOAD_MGMT_MONITOR_STABLE)) { // can only turn off loads
		for (i = 0; i < NO_OF_LOADS; i++) {
			if (!(CHECK_BIT(switch_cfg, i))) { // if bit is not set
				load_states[i] = 0; // turn off the load
				sw_load_states[i] = 0;
			}
		}
	}
}

/* Load Management Task Helper Functions
 * update_leds_from_fsm: updates leds based on current state in FSM_task, different to in maintenance since green leds stuff (may refactor into one function later)
 * shed_load: shed a single load from the network, starting from lowest prio (lowest led no)
 * reconnect_load: reconnect a single load to network, starting from highest prio (highest led no)
 * check_if_all_loads_connected: used to go back into NORMAL_OPERATION state if all loads are connected back
 * reset_timer: resets the timer expiry flag and the actual timer handle
 * timer_expiry_callback: runs when timer expires, sets expiry flag to high
 */

void update_leds_from_fsm() {
	unsigned long red_led = 0, green_led = 0, bit = 1; // for pio call, ending result of 0 is off and 1 is on
	unsigned int i;

	update_loads_from_switches();

	for (i = 0; i < NO_OF_LOADS; i++) {
		red_led |= (load_states[i] == true && sw_load_states[i] == true) ? bit : 0;
		green_led |= (load_states[i] == false && sw_load_states[i] == true) ? bit : 0; // green leds turn on when relay switches off loads AND switch is high
		bit = bit << 1; // shift left to do logic on next led
	}
	IOWR_ALTERA_AVALON_PIO_DATA(RED_LEDS_BASE, red_led);
	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, green_led);
}

void shed_load() {
	unsigned int i;
	for (i = 0; i < NO_OF_LOADS; i++) {
		if (load_states[i] == true) {
			load_states[i] = false;
			break;
		}
	}
	update_leds_from_fsm();
}

void reconnect_load() {
	unsigned int i;
	for (i = NO_OF_LOADS - 1; i >= 0; i--) {
		if (load_states[i] == false && sw_load_states[i] == true) { // only turn back on the load if it is actually switched on
			load_states[i] = true;
			break;
		}
	}
	update_leds_from_fsm();
}

bool check_if_all_loads_connected() {
	unsigned int i;
	for (i = 0; i < NO_OF_LOADS; i++) {
		if (load_states[i] == false && sw_load_states[i] == true) { // do not check if load is connected if it's not actually switched on
			return false;
		}
	}
	return true;
}

void reset_timer() {
	timer_expired_flag = false;
	//10 ticks that fsm_task is held in blocked state to wait for successful send to timer command q. incr if more timers added to system
	xTimerReset(fsm_timer, 10);
}

void timer_expiry_callback(xTimerHandle xTimer) {
	timer_expired_flag = true;
}

// Load Management Task

void Load_Management_Task(void *pvParameters) {

	while(1) {
		switch(system_state)
		{
			case MAINTENANCE_MODE:
				update_leds_from_fsm();
				break;
			case NORMAL_OPERATION:
				// check if things are still normal
				if (system_stable != true) {
					shed_load();
					update_shed_stats(); // t1 here since load has been shed here 
					reset_timer();
					system_state = LOAD_MGMT_MONITOR_UNSTABLE;
				}
				else {
					update_leds_from_fsm();
					system_state = NORMAL_OPERATION;
				}
				break;
			case LOAD_MGMT_MONITOR_UNSTABLE:
				if (timer_expired_flag == true) {
					reset_timer();
					shed_load();
				}
				else if (system_stable == false) {
					system_state = LOAD_MGMT_MONITOR_UNSTABLE;
				}
				else {
					reset_timer(); // check if it is a fluke or not, move to monitor stable state
					system_state = LOAD_MGMT_MONITOR_STABLE;
				}
				break;

			case LOAD_MGMT_MONITOR_STABLE:
				if (timer_expired_flag == true) {
					reset_timer();
					if (check_if_all_loads_connected()) {
						system_state = NORMAL_OPERATION; // everything back to normal
					}
					else {
						reconnect_load();
						system_state = LOAD_MGMT_MONITOR_STABLE;
					}
				}
				else if (system_stable == false) {
					reset_timer();
					system_state = LOAD_MGMT_MONITOR_UNSTABLE;
				}
				else {
					system_state = LOAD_MGMT_MONITOR_STABLE;
				}
				break;
		}
		vTaskDelay(5); // place into blocked state for 5ms so other low prio tasks can run
	}
}


int initCreateTasks(void) {
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
	shed_sem = xSemaphoreCreateMutex();
	fsm_timer = xTimerCreate("fsm_timer", TIMER_PERIOD, pdFALSE, (void*)0, timer_expiry_callback); // create 500ms timer with autoreload, callback sets timer expiry flag high

	xTimerStart(fsm_timer, 0);
	unsigned int i;
	for (i = 0; i < NO_OF_LOADS; i++) {
		load_states[i] = true; // turn all LEDs on initially because all loads are on
	}

	return 0;
}

// Initialisations for keyboard/ISR
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

void button_init(void) {
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(PUSH_BUTTON_BASE, 0x7); // enable interrupt for 1 button
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PUSH_BUTTON_BASE, 0x7); //write 1 to edge capture to clear pending interrupts
	alt_irq_register(PUSH_BUTTON_IRQ, 0, button_irq);  //register ISR for push button interrupt request
}

int main(int argc, char* argv[], char* envp[])
{
	alt_irq_register(FREQUENCY_ANALYSER_IRQ, 0, freq_relay);
	ps2_init();
	button_init();
	initOSDataStructs();
	initCreateTasks();
	vTaskStartScheduler();
	for (;;);

	return 0;
}

