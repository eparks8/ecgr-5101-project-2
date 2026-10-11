/**
 * pet_analog.h - ADC input for the virtual pet (Aravind's analog sensor).
 *
 * Reads ONE analog sensor on PA3 (Arduino header A0, ADC1 channel 8) with
 * ADC1 in 12-bit single-conversion polling mode. No HAL ADC files are needed:
 * the starter project does not ship stm32l4xx_hal_adc.c, so this module talks
 * to the ADC registers directly through CMSIS.
 *
 * Wiring (no soldering, breadboard only):
 *   Potentiometer: one end -> 3V3, other end -> GND, wiper -> PA3
 *   Photoresistor: 3V3 -> LDR -> PA3 -> 10k resistor -> GND
 *                  (brighter light = higher voltage, so "dark" = LOW value)
 *
 * Usage:
 *   PetAnalog_Init();                 // once, after the clocks are configured
 *   PetAnalog_Update();               // every main-loop pass or 10 ms tick
 *   if (PetAnalog_IsDark()) { ... }   // pet goes to sleep
 *   PetAnalog_DrawDebug(10, 200);     // raw / mV / % on the LCD for the video
 */
#ifndef PET_ANALOG_H
#define PET_ANALOG_H

#include <stdint.h>
#include <stdbool.h>

/* Supply reference. The Nucleo's VDDA is 3.3 V. */
#define PET_ANALOG_VREF_MV      3300U
#define PET_ANALOG_MAX_RAW      4095U     /* 12-bit */

/* "Dark" detection with hysteresis, in percent of full scale (0..100). */
#define PET_ANALOG_DARK_ENTER_PCT   25U   /* below this -> dark  */
#define PET_ANALOG_DARK_EXIT_PCT    35U   /* above this -> light */

/* Number of samples in the moving average (must be a power of two). */
#define PET_ANALOG_AVG_SAMPLES  16U

void     PetAnalog_Init(void);
uint16_t PetAnalog_ReadRaw(void);          /* one blocking conversion, 0..4095 */
void     PetAnalog_Update(void);           /* take one sample, update filter   */
uint16_t PetAnalog_GetRaw(void);           /* filtered raw value, 0..4095      */
uint16_t PetAnalog_GetMillivolts(void);    /* 0..3300                          */
uint8_t  PetAnalog_GetPercent(void);       /* 0..100                           */
bool     PetAnalog_IsDark(void);           /* debounced with hysteresis        */
void     PetAnalog_DrawDebug(uint16_t x, uint16_t y);

#endif /* PET_ANALOG_H */
