/**
  ******************************************************************************
  * @file    pet.c
  * @brief   Shared virtual pet state, mood and life cycle.
  *
  * Time: hunger rises by 1 every PET_HUNGER_RISE_MS and happiness falls by 1
  * every PET_HAPPY_DROP_MS (twice as fast while hungry). Paused while resting
  * (Pet_SetResting), since an action like sleep applies its own changes then.
  *
  * Death: if hunger sits at PET_STAT_MAX for PET_STARVE_MS the pet dies.
  * Any drop below max (e.g. feeding) resets the countdown.
  *
  * Runaway: if happiness sits at 0 for PET_BORED_MS the pet gets bored and
  * runs away. Any rise above 0 (e.g. playing) resets the countdown.
  ******************************************************************************
  */
#include "pet.h"
#include "main.h"

Pet_t g_pet = { PET_START_HUNGER, PET_START_HAPPINESS };

static uint8_t  starving;
static uint8_t  dead;
static uint32_t starving_since;
static uint8_t  bored;
static uint8_t  ran_away;
static uint32_t bored_since;
static uint8_t  resting;
static uint32_t last_hunger_tick;   /* when hunger last rose */
static uint32_t last_happy_tick;    /* when happiness last fell */

PetMood_t Pet_GetMood(void)
{
  if (g_pet.hunger >= PET_HUNGRY_AT)  { return PET_MOOD_HUNGRY; }
  if (g_pet.happiness <= PET_SAD_AT)  { return PET_MOOD_SAD; }
  if (g_pet.hunger <= PET_FULL_AT)    { return PET_MOOD_HAPPY; }
  return PET_MOOD_NEUTRAL;
}

static uint8_t Pet_Clamp(int16_t value)
{
  if (value < 0)                     { return 0U; }
  if (value > (int16_t)PET_STAT_MAX) { return (uint8_t)PET_STAT_MAX; }
  return (uint8_t)value;
}

void Pet_AddHunger(int16_t amount)
{
  g_pet.hunger = Pet_Clamp((int16_t)(g_pet.hunger + amount));
}

void Pet_AddHappiness(int16_t amount)
{
  g_pet.happiness = Pet_Clamp((int16_t)(g_pet.happiness + amount));
}

/* Hunger rises and happiness falls as time passes */
static void Pet_UpdateTime(uint32_t now)
{
  uint32_t happy_ms;

  if (resting)
  {
    /* Keep restarting the periods so waking up doesn't release a burst of
     * changes that built up while resting */
    last_hunger_tick = now;
    last_happy_tick = now;
    return;
  }

  /* "+= period" rather than "= now" keeps the average rate exact even when
   * the main loop comes back late (a full-screen redraw takes ~0.7 s) */
  while ((now - last_hunger_tick) >= PET_HUNGER_RISE_MS)
  {
    last_hunger_tick += PET_HUNGER_RISE_MS;
    Pet_AddHunger(1);
  }

  happy_ms = (g_pet.hunger >= PET_HUNGRY_AT) ? PET_HAPPY_DROP_HUNGRY_MS : PET_HAPPY_DROP_MS;
  while ((now - last_happy_tick) >= happy_ms)
  {
    last_happy_tick += happy_ms;
    Pet_AddHappiness(-1);
  }
}

void Pet_SetResting(uint8_t is_resting)
{
  resting = is_resting;
}

void Pet_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (dead || ran_away) { return; }

  Pet_UpdateTime(now);

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
      return;
    }
  }
  else
  {
    starving = 0U;            /* fed in time - countdown cancelled */
  }

  if (g_pet.happiness == 0U)
  {
    if (!bored)
    {
      bored = 1U;
      bored_since = now;      /* start the countdown */
    }
    else if ((now - bored_since) >= PET_BORED_MS)
    {
      ran_away = 1U;
    }
  }
  else
  {
    bored = 0U;               /* cheered up in time - countdown cancelled */
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

uint8_t Pet_IsBored(void)
{
  return (uint8_t)(bored && !ran_away && !dead);
}

uint8_t Pet_BoredSecondsLeft(void)
{
  uint32_t elapsed;

  if (!bored) { return (uint8_t)(PET_BORED_MS / 1000U); }

  elapsed = HAL_GetTick() - bored_since;
  if (elapsed >= PET_BORED_MS) { return 0U; }
  return (uint8_t)((PET_BORED_MS - elapsed + 999U) / 1000U);
}

uint8_t Pet_HasRunAway(void)
{
  return ran_away;
}

void Pet_Reset(void)
{
  g_pet.hunger = PET_START_HUNGER;
  g_pet.happiness = PET_START_HAPPINESS;
  starving = 0U;
  dead = 0U;
  bored = 0U;
  ran_away = 0U;
  resting = 0U;
  last_hunger_tick = HAL_GetTick();   /* a new pet gets a full period before its first change */
  last_happy_tick = last_hunger_tick;
}
