#include <system.h>
#include <altera_avalon_pio_regs.h>
#include <stdio.h>
int main()
{
  // declare the working variables
  unsigned int uiSwitchValue = 0;
  unsigned int uiButtonsValue = 0;
  unsigned int uiButtonsValuePrevious = 0;
  // the pointer to the lcd
  FILE *lcd;
  // print the information to the stdout
  printf("Hello from Nios II!\n");
  // set the value to the green leds
  IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LEDS_BASE, 0xaa);
  // open the character LCD
  lcd = fopen(CHARACTER_LCD_NAME, "w");
  // the program loop
  while(1)
  {
    // read the value of the switch and store to uiSwitchValue
    uiSwitchValue = IORD_ALTERA_AVALON_PIO_DATA(SLIDE_SWITCH_BASE);
    // write the value of the switches to the red LEDs
    IOWR_ALTERA_AVALON_PIO_DATA(RED_LEDS_BASE, uiSwitchValue);
    // also write the value of the switches  to the seven segments display
    IOWR_ALTERA_AVALON_PIO_DATA(SEVEN_SEG_BASE, uiSwitchValue);
    // store the prevous pressed buttons
    uiButtonsValuePrevious = uiButtonsValue;
    // read the current pressed buttons
    uiButtonsValue = IORD_ALTERA_AVALON_PIO_DATA(PUSH_BUTTON_BASE);
    // if the lcd is open successfully
    if(lcd != NULL)
    {
      if(uiButtonsValuePrevious != uiButtonsValue)
      {
        // print the value of the buttons in the character lcd
        #define ESC 27
        #define CLEAR_LCD_STRING "[2J"
        fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
        fprintf(lcd, "BUTTON VALUE: %d\n", uiButtonsValue);
      }
    }
  }
  // close the lcd
  fclose(lcd);
  return 0;
}
