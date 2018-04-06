#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "altera_up_avalon_video_character_buffer_with_dma.h"
#include "altera_up_avalon_video_pixel_buffer_dma.h"

int main()
{
	//reset the display
	alt_up_pixel_buffer_dma_dev *pixel_buf;
	pixel_buf = alt_up_pixel_buffer_dma_open_dev(VIDEO_PIXEL_BUFFER_DMA_NAME);
	if(pixel_buf == NULL){
		printf("Cannot find pixel buffer device\n");
	}
	alt_up_pixel_buffer_dma_clear_screen(pixel_buf, 0);
	
	//initialize character buffer
	alt_up_char_buffer_dev *char_buf;
	char_buf = alt_up_char_buffer_open_dev("/dev/video_character_buffer_with_dma");
	if(char_buf == NULL){
		printf("can't find char buffer device\n");
	}

	while(1){
		alt_up_char_buffer_string(char_buf, "Hello World", 40, 30);
		usleep(1000000);
		alt_up_char_buffer_draw(char_buf, '!', 51, 30);
		usleep(1000000);
		alt_up_char_buffer_clear(char_buf);
		usleep(1000000);
	}

  return 0;
}
