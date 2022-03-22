
Microcode simulation for various IBM360 machines. Currently only
the IBM360 model 30 is simulated. The simulator uses SDL to draw a
front panel and a picture of configured devices. Currently only
the Model 1442 Card Reader/Punch is supported.

The simulator will eventually include simulations for the:  

*  IBM 360 Model 30   2030
*  IBM 360 Model 50   2050
*  IBM 360 Model 65   2065

Others may be added it more microcode is recovered.

Planed devices include:  

*  IBM 1442 Card Reader/Punch.
*  IBM 1403 Printer.
*  IBM 2540R Card Reader.
*  IBM 2540P Card Punch.
*  IBM 2841 (IBM 2311 disk controller). Microcode based.
*  IBM 2844 (IBM 2314 disk controller). Microcode based.
*  IBM 2415 Tape drive.
*  IBM 2803 Tape controller.
*  IBM 2703 Terminal controller.

To Build on Linux:

````
   mkdir build 
   cd build
   cmake ..
   make
````


To Build on Windows:
   Install SDL2 libraries and headers in C:\SDL2

   Under Bash prompt:

````
   mkdir build
   cd build
   cmake .. -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Debug  -DSDL2_PATH="c:\SDL2" \
     -DSDL2_IMAGE_INCLUDE_DIR="c:\SDL2" -DSDL2_IMAGE_LIBRARY="c:\SDL2\lib\x64\SDL2_image.lib" \
     -DSDL2_TTF_PATH="c:\SDL2"
   cmake --build .
````

# Instruction

On startup there will be two windows open. One the front panel, the other devices.
On the front panel there is a pair of numbers on the top, the first is the number of
cycles run per 20ms chunck. This number should stay around 20000, if it is less you
are not running full speed. The second fps=# is how many ms it is taking to update
the displays. This number should stay under 15. If it gets over 15 the system might
be slow responding to mouse presses.

To see rewind tape animation, select one of the tape drives. In the popup window,
press the "Reset" button, the ready light should go out. Next press "EOM" the drive
will move to the end of tape. Then press "Load/Rewind" and the tape will start
to rewind. It will take about 1-2 minutes to rewind to the load point.

To change the tape loaded press, "Reset", then press "Unload", the supply reel and
tape will be removed. Change the file name, then press "Load/Rewind" the tape will
now show. Press "Start" to enable the drive.

Connect a telnet session to 3270 for the console.

To IPL BOS, select the printer, enter a file name, and press "Save". Next press "Start"
to bring the printer online. On the front panel press "INT TMR" to flip the switch up 
exposing the OFF indicator. Next select "0C0" in the "Load Unit" dials. Pressing
on the right side will rotate the dials counter clockwise, on the left clock wise.
Next press the "LOAD" button. When the "WAIT" light comes on press "<esc>" on the
console. This will give the "Enter IPL statments". To attach a deck to the
card reader, select the card reader. Enter the file name into "Hopper:". Press "LOAD"
the number of cards loaded will be shown in the last column. Press "END OF FILE" to 
turn on the EOF indicator. Then "Start" the ready light should now light.

To run stand alone card test programs, Open the card reader, enter the deck name
in hopper. Change the AUTO to EBCDIC, then press LOAD, End of File, Start. On front
panel set "Load unit" to 0x00A. Then press "LOAD".
