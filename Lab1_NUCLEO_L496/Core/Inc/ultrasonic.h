/**
  ******************************************************************************
  * @file    ultrasonic.h
  * @brief   HC-SR04 ultrasonic distance sensor, timed by TIM1.
  *
  * Wiring (sensor VCC on 5 V, Zio CN8 pin 9):
  *   Trig -> PE13 (Zio CN10 pin 10, D3)
  *   Echo -> 1k -> PE9 (Zio CN10 pin 4, D6), and PE9 -> 2k -> GND
  *           (the divider turns the sensor's 5 V echo into 3.3 V)
  ******************************************************************************
  */
#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdint.h>

#define ULTRASONIC_NONE  0xFFFFU   /* "no reading": nothing in range, or the sensor isn't answering */

void     Ultrasonic_Init(void);
void     Ultrasonic_Update(void);   /* call every main-loop pass */
uint16_t Ultrasonic_GetMm(void);    /* distance in mm, or ULTRASONIC_NONE */

#endif /* ULTRASONIC_H */
