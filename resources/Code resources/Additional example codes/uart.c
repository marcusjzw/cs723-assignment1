
#include <stdio.h>
#include <unistd.h>
#include "system.h"


int main()
{
	FILE* fp;
	fp = fopen (UART_NAME, "w+"); //Open file for read and write
	char message;

	while(1){
		fprintf(fp, "Type a character\n");
		fscanf(fp, "%c", &message);
		printf("scan: %c\n", message);

	}
	return 0;
}
