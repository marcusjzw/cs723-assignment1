// Part of the file provided by Terasic
#include <stdio.h>
#include "system.h"
#include "io.h"
#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_keyboard.h"
int main()
{
  alt_up_ps2_dev * ps2_device = alt_up_ps2_open_dev(PS2_NAME);

  if(ps2_device == NULL){
    printf("can't find PS/2 device\n");
    return 1;
  }

  alt_up_ps2_clear_fifo (ps2_device) ;
  
  char ascii;
  int status = 0;
  unsigned char key = 0;
  KB_CODE_TYPE decode_mode;  
  while(1)
  {
      // blocking function call      
      status = decode_scancode (ps2_device, &decode_mode , &key , &ascii) ;
      if ( status == 0 ) //success
      {
        // print out the result
        switch ( decode_mode )
        {
          case KB_ASCII_MAKE_CODE :
            printf ( "ASCII   : %x\n", key ) ;
            break ;
          case KB_LONG_BINARY_MAKE_CODE :
            // do nothing
          case KB_BINARY_MAKE_CODE :
            printf ( "MAKE CODE : %x\n", key ) ;             
            break ;
          case KB_BREAK_CODE :
            // do nothing
          default :
            printf ( "DEFAULT   : %x\n", key ) ;
            break ;
        }
        IOWR(SEVEN_SEG_BASE,0 ,key);
      }

  }  
  return 0;
}
