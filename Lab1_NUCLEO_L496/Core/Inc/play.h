/**
  ******************************************************************************
  * @file    play.h
  * @brief   Play action: a physical trigger makes the pet bounce; raises
  *          happiness. Same five-function interface as the other menu actions.
  ******************************************************************************
  */
#ifndef PLAY_H
#define PLAY_H

#include <stdint.h>

void    Play_Start(void);
void    Play_HandleInput(uint8_t pressed);
void    Play_Update(void);
void    Play_Abort(void);
uint8_t Play_IsActive(void);

#endif /* PLAY_H */
