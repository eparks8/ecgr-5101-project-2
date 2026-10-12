/**
  ******************************************************************************
  * @file    feed.h
  * @brief   Feed action: pick a food, update the pet, play the eating animation.
  ******************************************************************************
  */
#ifndef FEED_H
#define FEED_H

#include <stdint.h>

void    Feed_Start(void);
void    Feed_HandleInput(uint8_t pressed);
void    Feed_Update(void);
uint8_t Feed_IsActive(void);

#endif /* FEED_H */
