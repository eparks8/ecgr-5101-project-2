/**
  ******************************************************************************
  * @file    sleep.h
  * @brief   Sleep action: cover the photoresistor and the pet sleeps (Zzz)
  *          while it stays dark. Same five-function interface as the other
  *          menu actions.
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

#endif /* SLEEP_H */
