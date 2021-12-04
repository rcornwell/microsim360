
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

   mkdir build 
   cd build
   cmake ..
   make


To Build on Windows:
   Install SDL2 libraries and headers in C:\SDL2

   Under Bash prompt:

   mkdir build
   cd build
   cmake .. -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Debug  -DSDL2_PATH="c:\SDL2" \
     -DSDL2_IMAGE_INCLUDE_DIR="c:\SDL2" -DSDL2_IMAGE_LIBRARY="c:\SDL2\lib\x64\SDL2_image.lib" \
     -DSDL2_TTF_PATH="c:\SDL2"
   cmake --build .

