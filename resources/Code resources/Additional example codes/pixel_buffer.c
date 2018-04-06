#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "altera_up_avalon_video_pixel_buffer_dma.h"
int main()
{
	alt_up_pixel_buffer_dma_dev *pixel_buf;
	pixel_buf = alt_up_pixel_buffer_dma_open_dev(VIDEO_PIXEL_BUFFER_DMA_NAME);
	if(pixel_buf == NULL){
		printf("Cannot find pixel buffer device\n");
	}

	alt_up_pixel_buffer_dma_clear_screen(pixel_buf, 0);
	alt_up_pixel_buffer_dma_draw_box(pixel_buf, 100, 100, 200, 200, 0x3ff, 0); //Green
	alt_up_pixel_buffer_dma_draw_rectangle(pixel_buf, 250, 300, 400, 400, 0x3ff << 10, 0); //Red
	alt_up_pixel_buffer_dma_draw_line(pixel_buf, 500, 150, 600, 350, 0x3ff << 20, 0); //Blue
	alt_up_pixel_buffer_dma_draw_hline(pixel_buf, 300, 400, 150, ((0x3ff << 20) + (0x3ff << 10) + (0x3ff)), 0); //White
	alt_up_pixel_buffer_dma_draw_vline(pixel_buf, 50, 50, 400, ((0x3ff << 20) + (0x3ff)), 0); //Cyan

	while(1){
	}

	return 0;
}
