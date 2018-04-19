# cs723-assignment1

# Important Note:
- Make sure all switches for loads are set high initially for ease of testing, or else they will be switched off by default by the load management task (it allows switching off).

# Instructions:

The system can be run using an Altera DE2-115 FPGA running the Nios II processor. Ensure that a PS2 keyboard has been plugged in, and a VGA cable connects the DE2 board to an external monitor. 

The number of loads present in the system can be modified by altering the definition of NO_OF_LOADS in the C code. By default we have the maximum number of loads configured (eight, as there are only eight green LEDs).

### 1. DE2 Board
The red LEDs, LEDR7 to LEDR0, show the current status of connectivity for the loads where on means that the load is connected, and off means it is disconnected. The green LEDs show the current shed status for each appropriate load, and is only turned enabled when the system is managing loads. 
The push button, KEY3, toggles the operation of maintenance mode, and wall switches for all loads can be toggled using SW7 to SW0.’

### 2. Keyboard
The Up and Down arrow keys on the keyboard will increment and decrement the frequency threshold by 1 Hz respectively. The Pg Up and Pg Down keys will increment and decrement the rate of change (RoC) threshold by 1 Hz/s. 

### 3. VGA Display
The display shows various metrics surrounding system operation. Most prominently, there is a graph displaying the frequency and rate of change in the system. The rightmost values in the graph are the most current values.

There is also text displaying:
•	the current frequency and rate of change thresholds
•	current state the system is operating in 
•	whether the system is currently stable or not
•	The time taken for an initial load shed, to verify it meets the 200ms timing requirement
•	The last five initial load shed times as well as minimum, maximum and average reaction times
•	The total run time of the system


# How to fix Nios II Issues:
#### Missing ELF file:
- Go to 'run configurations' and toggle the ELF file, run

#### Indexer error when starting Nios II:
- Delete the .pdom files from \\.metadata\.plugins\org.eclipse.cdt.core and \software\.metadata\.plugins\org.eclipse.cdt.core

#### FreeRTOS symbols not resolving 
- Delete freertos_test project from workspace, re-import 
