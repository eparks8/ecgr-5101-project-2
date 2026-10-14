# Separate NUCLEO-F091RC photoresistor ADC test

This is a separate minimal test project. It does not modify or replace `Lab1_NUCLEO_L496`.
It reads a 10-bit/12-bit ADC result from PA3 (ADC1 channel 3) on the STM32F091RC and turns the board's PA5/LD2 LED on when the averaged reading falls below the 25% dark threshold. Hysteresis switches back to light above 35%.

## Requirements
- Existing STM32CubeF0 clone at `F:\STM32CubeF0` (or set `CUBE_F0_ROOT` to its path).
- ARM GNU toolchain available in PATH or through your IDE's CMake toolchain setup.
- The linker script and HAL files are referenced from the CubeF0 clone.

## Sensor wiring
With power disconnected: connect 3V3 -> photoresistor -> PA3 -> 10 kOhm resistor -> GND. This divider orientation makes brighter light produce a higher ADC value. Ensure PA3 is not driven above 3.3 V.

## Build note
The CMake project is intentionally separate from the L496 application. The included `cmake-toolchain.cmake` uses the ARM GCC path from your current setup but applies Cortex-M0 flags. If your toolchain is installed elsewhere, update the compiler paths in that file. Configure with `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake-toolchain.cmake -DCUBE_F0_ROOT=F:/STM32CubeF0`, then build with `cmake --build build`. Do not reuse the L496 toolchain file. Do not flash the L496 ELF to the F091RC. The LCD and the full pet application are not ported by this minimal ADC test; this only verifies the sensor input path first.
