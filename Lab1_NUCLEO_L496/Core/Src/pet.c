/**
  ******************************************************************************
  * @file    pet.c
  * @brief   Shared virtual pet state.
  ******************************************************************************
  */
#include "pet.h"

Pet_t g_pet = { 50U, 50U };

PetMood_t Pet_GetMood(void)
{
  if (g_pet.hunger >= PET_HUNGRY_AT)  { return PET_MOOD_HUNGRY; }
  if (g_pet.happiness <= PET_SAD_AT)  { return PET_MOOD_SAD; }
  if (g_pet.hunger <= PET_FULL_AT)    { return PET_MOOD_HAPPY; }
  return PET_MOOD_NEUTRAL;
}
