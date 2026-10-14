/**
  ******************************************************************************
  * @file    pet.h
  * @brief   Shared virtual pet state and on-screen pet layout.
  *
  * g_pet is read and written by every action (feed, play, timers) and shown
  * by the main screen meters - agree on the meaning of each field as a team.
  ******************************************************************************
  */
#ifndef PET_H
#define PET_H

#include <stdint.h>
#include "ili9341.h"

#define PET_STAT_MAX        100U
#define PET_START_HUNGER    50U
#define PET_START_HAPPINESS 50U
#define PET_STARVE_MS       20000U   /* hunger stuck at max this long -> the pet dies */
#define PET_BORED_MS        20000U   /* happiness stuck at 0 this long -> the pet runs away */
#define PET_TOO_HUNGRY_TO_PLAY 90U   /* hunger >= this -> refuses to dance / fetch */

typedef struct
{
  uint8_t hunger;     /* 0 = full, 100 = starving */
  uint8_t happiness;  /* 0 = miserable, 100 = ecstatic */
} Pet_t;

extern Pet_t g_pet;

typedef enum
{
  PET_MOOD_NEUTRAL,
  PET_MOOD_HAPPY,
  PET_MOOD_SAD,
  PET_MOOD_HUNGRY,
  PET_MOOD_ASLEEP   /* a face only (used by SLEEP) - Pet_GetMood() never returns it */
} PetMood_t;

/* Mood thresholds - checked in this order: hungry, sad, happy, else neutral */
#define PET_HUNGRY_AT   70U   /* hunger >= this    -> hungry */
#define PET_SAD_AT      30U   /* happiness <= this -> sad */
#define PET_FULL_AT     30U   /* hunger <= this    -> happy */

PetMood_t Pet_GetMood(void);

/* Change a stat by a signed amount, clamped to 0..PET_STAT_MAX */
void Pet_AddHunger(int16_t amount);
void Pet_AddHappiness(int16_t amount);

/* Life cycle */
void    Pet_Update(void);              /* call every main-loop pass - runs the starvation timer */
uint8_t Pet_IsStarving(void);          /* hunger is at max and the death countdown is running */
uint8_t Pet_StarveSecondsLeft(void);   /* countdown, rounded up */
uint8_t Pet_IsDead(void);
uint8_t Pet_IsBored(void);             /* happiness is at 0 and the runaway countdown is running */
uint8_t Pet_BoredSecondsLeft(void);    /* countdown, rounded up */
uint8_t Pet_HasRunAway(void);
void    Pet_Reset(void);               /* new pet: starting stats, alive and home */

/* Pet layout - shared by the main screen drawing and the feed animation.
 * Whoever draws the final pet should update these to match. */
#define PET_HEAD_X      70U
#define PET_HEAD_Y      150U
#define PET_HEAD_W      100U
#define PET_HEAD_H      80U
#define PET_FACE_COLOR  ILI9341_COLOR_GREEN

#define PET_MOUTH_X     105U
#define PET_MOUTH_Y     200U
#define PET_MOUTH_W     30U
#define PET_MOUTH_H     12U
#define PET_MOUTH_COLOR ILI9341_COLOR_BLACK

#endif /* PET_H */
