/**
  ******************************************************************************
  * @file    runaway.h
  * @brief   Screen shown when the pet gets bored and runs away.
  ******************************************************************************
  */
#ifndef RUNAWAY_H
#define RUNAWAY_H

void RunawayScreen_Draw(void);
void RunawayScreen_Update(void);   /* walking-away animation - must not block */

#endif /* RUNAWAY_H */
