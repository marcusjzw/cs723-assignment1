# cs723-assignment1

# Important Note:
- Make sure all switches for loads are set high initially for ease of testing, or else they will be switched off by default by the load management task (it allows switching off).
## To do:
- Draw VGA task according to assignment specification for freq and ROC
- Create Thresholds + task and ISR to adjust thresholds
- Link to VGA task
- Button ISR to get into maintenance mode
- Load management FSM with LEDs
- Maintenance mode

## How to fix Nios II Issues:
#### Missing ELF file:
- Go to 'run configurations' and toggle the ELF file, run

#### Indexer error when starting Nios II 
- Delete the .pdom files from \\.metadata\.plugins\org.eclipse.cdt.core and \software\.metadata\.plugins\org.eclipse.cdt.core

