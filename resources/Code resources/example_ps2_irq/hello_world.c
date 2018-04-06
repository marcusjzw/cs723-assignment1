#include <stdio.h>
#include "system.h"
#include "io.h"
#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_keyboard.h"
#include "sys/alt_irq.h"

void ps2_isr (void* context, alt_u32 id)
{
  char ascii;
  int status = 0;
  unsigned char key = 0;
  KB_CODE_TYPE decode_mode;
  status = decode_scancode (context, &decode_mode , &key , &ascii) ;
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
int main()
{
  alt_up_ps2_dev * ps2_device = alt_up_ps2_open_dev(PS2_NAME);

  if(ps2_device == NULL){
    printf("can't find PS/2 device\n");
    return 1;
  }

  alt_up_ps2_clear_fifo (ps2_device) ;

  alt_irq_register(PS2_IRQ, ps2_device, ps2_isr);
  // register the PS/2 interrupt
  IOWR_8DIRECT(PS2_BASE,4,1);
  while(1){}
  return 0;
}
