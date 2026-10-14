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
void    Feed_StartRandom(void);   /* skip the chooser: feed a random food (5 claps) */
void    Feed_HandleInput(uint8_t pressed);
void    Feed_Update(void);
void    Feed_Abort(void);   /* stop immediately, e.g. the pet died mid-animation */
uint8_t Feed_IsActive(void);

#endif /* FEED_H */
