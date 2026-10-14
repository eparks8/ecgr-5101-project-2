/**
  ******************************************************************************
  * @file    death.h
  * @brief   Tombstone screen shown when the pet dies.
  ******************************************************************************
  */
#ifndef DEATH_H
#define DEATH_H

void DeathScreen_Draw(void);
void DeathScreen_Update(void);   /* ghost animation - must not block */

#endif /* DEATH_H */
