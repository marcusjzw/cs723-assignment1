#include <stdio.h>
#include <string.h>
#include "system.h"
int main(void)
{
  char* stringToOutput = "example1";
  // To operate on a file descriptor, a file pointer is required
  FILE* fp;
  // a file descriptor can be open by using fopen function
  // the syntax of fopen, which will return a file pointer
  // fopen(the_string_of_the_name_of_file_descriptor¡¨, ¡§the_string_of_the_attribute¡¨)
  // if success, it will return the pointer to the device
  // otherwise, a NULL pointer will be returned
  // open up UART with write accesss
  fp = fopen("/dev/uart", "w");
  // check if the UART is open successfully
  if (fp != NULL)
  {
	// use fprintf to write things to file
    fprintf(fp, "%s", stringToOutput);
	// remember to close the file
    fclose(fp);
  }
  return 0;
}
