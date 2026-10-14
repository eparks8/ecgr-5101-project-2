/**
  ******************************************************************************
  * @file    mictest.h
  * @brief   MIC menu item: live microphone readings for checking the wiring
  *          and tuning the loudness thresholds.
  ******************************************************************************
  */
#ifndef MICTEST_H
#define MICTEST_H

#include <stdint.h>

void    MicTest_Start(void);
void    MicTest_HandleInput(uint8_t pressed);
void    MicTest_Update(void);
void    MicTest_Abort(void);
uint8_t MicTest_IsActive(void);
void    MicTest_OnClap(uint8_t claps);   /* shows the count instead of running a command */

#endif /* MICTEST_H */
