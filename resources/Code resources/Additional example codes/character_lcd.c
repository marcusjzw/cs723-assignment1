#include <stdio.h>
#include <unistd.h>
#include "system.h"

static unsigned char esc = 0x1b;

int main()
{
	FILE* fp;
	fp = fopen(CHARACTER_LCD_NAME, "w"); //open the character LCD as a file stream for write

	if (fp == NULL) {
		printf("open failed\n");
		return 1;
	}

	while(1){
		fprintf(fp, "%c%sHello\n", esc, "[2J"); //esc character (0x1b) followed by "[2J]" clears the screen
		usleep(1000000);
		fprintf(fp, "World!\n");
		usleep(1000000);
		fprintf(fp, "%c%sFrom NIOS II\n", esc, "[2J");
		usleep(1000000);
	}
	return 0;
}
