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

#define PET_STAT_MAX 100U

typedef struct
{
  uint8_t hunger;     /* 0 = full, 100 = starving */
  uint8_t happiness;  /* 0 = miserable, 100 = ecstatic */
} Pet_t;

extern Pet_t g_pet;

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
