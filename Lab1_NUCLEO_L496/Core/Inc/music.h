/**
  ******************************************************************************
  * @file    music.h
  * @brief   MUSIC action: a rhythm game played by moving a hand over the
  *          ultrasonic sensor. Same five-function interface as the other
  *          menu actions.
  ******************************************************************************
  */
#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>

void    Music_Start(void);
void    Music_HandleInput(uint8_t pressed);
void    Music_Update(void);
void    Music_Abort(void);
uint8_t Music_IsActive(void);

#endif /* MUSIC_H */
