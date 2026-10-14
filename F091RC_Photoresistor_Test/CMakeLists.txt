cmake_minimum_required(VERSION 3.22)
project(F091RC_Photoresistor_Test C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set CUBE_F0_ROOT to the STM32CubeF0 clone, e.g. F:/STM32CubeF0.
if(NOT DEFINED CUBE_F0_ROOT)
  set(CUBE_F0_ROOT "F:/STM32CubeF0" CACHE PATH "Path to STM32CubeF0")
endif()
set(F0_DRV "${CUBE_F0_ROOT}/Drivers/STM32F0xx_HAL_Driver")
set(F0_CMSIS "${CUBE_F0_ROOT}/Drivers/CMSIS")
set(F0_DEV "${F0_CMSIS}/Device/ST/STM32F0xx")
set(F0_TEMPLATE "${CUBE_F0_ROOT}/Projects/STM32F091RC-Nucleo/Examples/ADC/ADC_Sequencer")

add_executable(F091RC_Photoresistor_Test
  main.c
  "${F0_DEV}/Source/Templates/system_stm32f0xx.c"
  "${F0_DEV}/Source/Templates/gcc/startup_stm32f091xc.s"
  "${F0_DRV}/Src/stm32f0xx_hal.c"
  "${F0_DRV}/Src/stm32f0xx_hal_cortex.c"
  "${F0_DRV}/Src/stm32f0xx_hal_rcc.c"
  "${F0_DRV}/Src/stm32f0xx_hal_rcc_ex.c"
  "${F0_DRV}/Src/stm32f0xx_hal_gpio.c"
  "${F0_DRV}/Src/stm32f0xx_hal_flash.c"
  "${F0_DRV}/Src/stm32f0xx_hal_flash_ex.c"
  "${F0_DRV}/Src/stm32f0xx_hal_pwr.c"
  "${F0_DRV}/Src/stm32f0xx_hal_pwr_ex.c"
  "${F0_DRV}/Src/stm32f0xx_hal_adc.c"
  "${F0_DRV}/Src/stm32f0xx_hal_adc_ex.c"
)
target_include_directories(F091RC_Photoresistor_Test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${F0_TEMPLATE}/Inc"
  "${F0_DRV}/Inc"
  "${F0_CMSIS}/Include"
  "${F0_DEV}/Include"
)
target_compile_definitions(F091RC_Photoresistor_Test PRIVATE
  USE_HAL_DRIVER STM32F091xC
)
target_compile_options(F091RC_Photoresistor_Test PRIVATE
  -mcpu=cortex-m0 -mthumb -ffunction-sections -fdata-sections -Wall -Wextra
)
target_link_options(F091RC_Photoresistor_Test PRIVATE
  -mcpu=cortex-m0 -mthumb
  "-TF:/STM32CubeF0/Projects/STM32F091RC-Nucleo/Templates/SW4STM32/STM32F091RC-Nucleo/STM32F091RCTx_FLASH.ld"
  --specs=nano.specs
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_PROJECT_NAME}.map
)
target_link_libraries(F091RC_Photoresistor_Test PRIVATE m)
set_target_properties(F091RC_Photoresistor_Test PROPERTIES SUFFIX ".elf")
