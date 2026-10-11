# Adding pet_analog to the project

1. Copy `pet_analog.h` -> `Core/Inc/`, `pet_analog.c` -> `Core/Src/`.
2. In `CMakeLists.txt`, under `target_sources(... PRIVATE`, add:
       Core/Src/pet_analog.c
   and under `target_include_directories(... PRIVATE`, add:
       Drivers/BSP/GFX01M2
   (only if `ili9341.h` is not already found).
3. In `main.c`:
       #include "pet_analog.h"
       ... after SystemClock_Config() and MX_GPIO_Init():  PetAnalog_Init();
       ... in the main loop:  PetAnalog_Update();
4. Smoke test before using it in the pet: in the loop call
       PetAnalog_DrawDebug(10, 200);
   Turn the potentiometer (or cover the photoresistor) and watch RAW go 0..4095.
   (RAW 0 with the sensor attached usually means a wiring problem.)

Pin check: PA3 is not used by the starter's LCD SPI (PA5/6/7, PA9, PA1, PB10)
or joystick (PB6, PC7, PB4, PB0, PC0). BUT the X-NUCLEO-GFX01M2 sits on the
morpho/Arduino headers, so confirm with a multimeter or the Nucleo-144 user
manual (UM2179) that the A0 / PA3 pin is physically reachable with the board
attached. If not, use another free ADC1 pin (e.g. PC3 = ADC1_IN4) and change
PET_ADC_CHANNEL, the GPIO port/pin and SMPR1 field in pet_analog.c.
