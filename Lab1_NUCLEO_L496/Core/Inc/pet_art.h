/**
  ******************************************************************************
  * @file    pet_art.h
  * @brief   Drawing the pet away from its home position, for actions that
  *          move it around (dance, fetch).
  *
  * Implemented by whoever draws the pet - test_pet.c for now; the real pet
  * must provide the same function. Offsets are relative to PET_HEAD_X/Y and
  * are clamped to the limits below. MainScreen_Draw() puts the pet back home.
  ******************************************************************************
  */
#ifndef PET_ART_H
#define PET_ART_H

#include <stdint.h>
#include "pet.h"

#define PET_ART_MIN_DX  (-60)   /* keeps the whole pet on the 240 px wide screen */
#define PET_ART_MAX_DX  (65)
#define PET_ART_MIN_DY  (-60)   /* keeps the ears clear of the text strip at the top */
#define PET_ART_MAX_DY  (20)    /* keeps the head clear of the meters */

/* Move the pet to (dx, dy) from home with the given expression. Only the
 * strip the pet moved off is erased, so small steps are cheap. */
void PetArt_Move(int16_t dx, int16_t dy, PetMood_t face);

#endif /* PET_ART_H */
