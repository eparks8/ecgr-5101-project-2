/**
  ******************************************************************************
  * @file    pet.c
  * @brief   Shared virtual pet state, mood and life cycle.
  *
  * Death: if hunger sits at PET_STAT_MAX for PET_STARVE_MS the pet dies.
  * Any drop below max (e.g. feeding) resets the countdown.
  ******************************************************************************
  */
#include "pet.h"
#include "main.h"

Pet_t g_pet = { PET_START_HUNGER, PET_START_HAPPINESS };

static uint8_t  starving;
static uint8_t  dead;
static uint32_t starving_since;

PetMood_t Pet_GetMood(void)
{
  if (g_pet.hunger >= PET_HUNGRY_AT)  { return PET_MOOD_HUNGRY; }
  if (g_pet.happiness <= PET_SAD_AT)  { return PET_MOOD_SAD; }
  if (g_pet.hunger <= PET_FULL_AT)    { return PET_MOOD_HAPPY; }
  return PET_MOOD_NEUTRAL;
}

void Pet_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (dead) { return; }

  if (g_pet.hunger >= PET_STAT_MAX)
  {
    if (!starving)
    {
      starving = 1U;
      starving_since = now;   /* start the countdown */
    }
    else if ((now - starving_since) >= PET_STARVE_MS)
    {
      dead = 1U;
    }
  }
  else
  {
    starving = 0U;            /* fed in time - countdown cancelled */
  }
}

uint8_t Pet_IsStarving(void)
{
  return (uint8_t)(starving && !dead);
}

uint8_t Pet_StarveSecondsLeft(void)
{
  uint32_t elapsed;

  if (!starving) { return (uint8_t)(PET_STARVE_MS / 1000U); }

  elapsed = HAL_GetTick() - starving_since;
  if (elapsed >= PET_STARVE_MS) { return 0U; }
  return (uint8_t)((PET_STARVE_MS - elapsed + 999U) / 1000U);
}

uint8_t Pet_IsDead(void)
{
  return dead;
}

void Pet_Reset(void)
{
  g_pet.hunger = PET_START_HUNGER;
  g_pet.happiness = PET_START_HAPPINESS;
  starving = 0U;
  dead = 0U;
}
