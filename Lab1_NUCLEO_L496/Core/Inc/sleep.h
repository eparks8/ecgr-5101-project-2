/**
  ******************************************************************************
  * @file    sleep.h
  * @brief   Sleep action: the pet naps, gets rested, and wakes on a press,
  *          on a loud noise, or by itself.
  ******************************************************************************
  */
#ifndef SLEEP_H
#define SLEEP_H

#include <stdint.h>

void    Sleep_Start(void);
void    Sleep_HandleInput(uint8_t pressed);
void    Sleep_Update(void);
void    Sleep_Abort(void);
uint8_t Sleep_IsActive(void);
void    Sleep_OnClap(uint8_t claps);

#endif /* SLEEP_H */
