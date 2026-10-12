/**
  ******************************************************************************
  * @file    fetch.h
  * @brief   Fetch action: time the throw, then the pet runs to get the ball.
  ******************************************************************************
  */
#ifndef FETCH_H
#define FETCH_H

#include <stdint.h>

void    Fetch_Start(void);
void    Fetch_HandleInput(uint8_t pressed);
void    Fetch_Update(void);
void    Fetch_Abort(void);
uint8_t Fetch_IsActive(void);

#endif /* FETCH_H */
