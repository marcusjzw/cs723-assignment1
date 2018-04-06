#include <unistd.h> //for usleep
#include "system.h"
#include "io.h"

int main()
{
	unsigned int temp = 0;
	while(1){
		IOWR(SEVEN_SEG_BASE, 0, temp);
		++temp;
		usleep(1000000);
	}
	return 0;
}
