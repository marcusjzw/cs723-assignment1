# cs723-assignment1

# Important Note:
- Make sure all switches for loads are set high initially for ease of testing, or else they will be switched off by default by the load management task (it allows switching off).
## To do:
- Fix switches problem somehow
- Maintenance mode task
- Show timing for 200ms guarantee and display it on the VGA
- Testing everything
- Writing the doc

## How to fix Nios II Issues:
#### Missing ELF file:
- Go to 'run configurations' and toggle the ELF file, run

#### Indexer error when starting Nios II:
- Delete the .pdom files from \\.metadata\.plugins\org.eclipse.cdt.core and \software\.metadata\.plugins\org.eclipse.cdt.core

#### FreeRTOS symbols not resolving 
- Delete freertos_test project from workspace, re-import 