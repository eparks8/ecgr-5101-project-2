/**
  ******************************************************************************
  * @file    dance.h
  * @brief   Dance action: a rhythm game - copy the moves to make the pet dance.
  ******************************************************************************
  */
#ifndef DANCE_H
#define DANCE_H

#include <stdint.h>

void    Dance_Start(void);
void    Dance_HandleInput(uint8_t pressed);
void    Dance_Update(void);
void    Dance_Abort(void);
uint8_t Dance_IsActive(void);

#endif /* DANCE_H */
