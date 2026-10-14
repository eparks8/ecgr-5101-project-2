/**
  ******************************************************************************
  * @file    sleep.c
  * @brief   Sleep action: the pet naps, gets rested, and wakes on a press,
  *          on a loud noise, or by itself.
  *
  *   ASLEEP --CENTER press--------------> WAKING "GOOD MORNING!" (gentle)
  *          --loud noise or a clap-------> WAKING "WHO WOKE ME?!" (startled, -happiness)
  *          --SLEEP_MAX_MS---------------> WAKING "WELL RESTED!"
  *   WAKING --WAKE_MS--> IDLE (menu returns to the main screen)
  *
  * Input:        the joystick, or the microphone (noise / claps wake it).
  * State change: +1 happiness every SLEEP_HAPPY_MS (rested); hunger still
  *               rises, but slowly (+1 every SLEEP_HUNGER_MS).
  * Animation:    closed eyes and "Z" letters floating up from the head.
  ******************************************************************************
  */
#include <stdio.h>
#include "sleep.h"
#include "input.h"
#include "menu.h"
#include "mic.h"
#include "pet.h"
#include "pet_art.h"
#include "main.h"
#include "ili9341.h"

#define BG_COLOR         ILI9341_COLOR_BLACK
#define TOP_H            70U
#define TITLE_Y          14U
#define HINT_Y           44U
#define STATUS_Y         58U

#define SLEEP_GRACE_MS   1500U    /* ignore noise just after falling asleep (e.g. the claps that started it) */
#define SLEEP_HAPPY_MS   2000U    /* rested: +1 happiness this often */
#define SLEEP_HUNGER_MS  20000U   /* still gets hungry, just slowly */
#define SLEEP_MAX_MS     60000U   /* wakes up by itself after a minute */
#define Z_FRAME_MS       400U
#define WAKE_MS          1500U
#define STARTLE_HAPPY    5        /* being woken by a noise is annoying */

/* Z letters rising up and to the right of the head, getting bigger */
#define Z1_X  (PET_HEAD_X + PET_HEAD_W + 2U)
#define Z1_Y  (PET_HEAD_Y - 22U)
#define Z2_X  (PET_HEAD_X + PET_HEAD_W + 14U)
#define Z2_Y  (PET_HEAD_Y - 46U)
#define Z3_X  (PET_HEAD_X + PET_HEAD_W + 30U)
#define Z3_Y  (PET_HEAD_Y - 76U)

typedef enum { SLEEP_IDLE, SLEEP_ASLEEP, SLEEP_WAKING } SleepState_t;

static SleepState_t state = SLEEP_IDLE;
static uint32_t     sleep_start;
static uint32_t     last_happy;
static uint32_t     last_hunger;
static uint32_t     last_z;
static uint32_t     wake_start;
static uint8_t      z_phase;
static uint8_t      rested;    /* happiness gained this nap, shown on screen */

static void Sleep_DrawZ(uint8_t phase)
{
  /* phase 0: none, 1: small z, 2: + medium Z, 3: + big Z */
  ILI9341_DrawString(Z1_X, Z1_Y, (phase >= 1U) ? "Z" : " ", ILI9341_COLOR_CYAN, BG_COLOR, 1);
  ILI9341_DrawString(Z2_X, Z2_Y, (phase >= 2U) ? "Z" : " ", ILI9341_COLOR_CYAN, BG_COLOR, 2);
  ILI9341_DrawString(Z3_X, Z3_Y, (phase >= 3U) ? "Z" : " ", ILI9341_COLOR_CYAN, BG_COLOR, 3);
}

static void Sleep_DrawRested(void)
{
  char text[24];

  (void)snprintf(text, sizeof(text), "RESTED +%-3u", rested);
  Menu_DrawCentered(STATUS_Y, text, ILI9341_COLOR_GREEN, 1);
}

static void Sleep_Wake(const char *message, uint16_t color, PetMood_t face, uint32_t now)
{
  Sleep_DrawZ(0U);
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  Menu_DrawCentered(TITLE_Y, message, color, 2);
  PetArt_Move(0, 0, face);
  wake_start = now;
  state = SLEEP_WAKING;
}

void Sleep_Start(void)
{
  uint32_t now = HAL_GetTick();

  MainScreen_Draw();
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);

  if (g_pet.hunger >= PET_TOO_HUNGRY_TO_PLAY)
  {
    Menu_DrawCentered(TITLE_Y, "TOO HUNGRY", ILI9341_COLOR_RED, 3);
    wake_start = now;
    state = SLEEP_WAKING;   /* show the refusal, no stat change */
    return;
  }

  Menu_DrawCentered(TITLE_Y, "SLEEPING", ILI9341_COLOR_CYAN, 3);
  Menu_DrawCentered(HINT_Y, "PRESS TO WAKE  (NOISE WAKES IT TOO)", ILI9341_COLOR_GRAY, 1);
  PetArt_Move(0, 0, PET_MOOD_ASLEEP);

  rested = 0U;
  Sleep_DrawRested();
  z_phase = 0U;
  sleep_start = now;
  last_happy = now;
  last_hunger = now;
  last_z = now;
  state = SLEEP_ASLEEP;
}

void Sleep_HandleInput(uint8_t pressed)
{
  if ((state == SLEEP_ASLEEP) && (pressed & JOY_CENTER_MASK))
  {
    Sleep_Wake("GOOD MORNING!", ILI9341_COLOR_YELLOW, PET_MOOD_HAPPY, HAL_GetTick());
  }
}

/* Startled awake by a noise */
static void Sleep_Startle(uint32_t now)
{
  Pet_AddHappiness(-STARTLE_HAPPY);
  Sleep_Wake("WHO WOKE ME?!", ILI9341_COLOR_RED, PET_MOOD_SAD, now);
}

void Sleep_Update(void)
{
  uint32_t now = HAL_GetTick();

  switch (state)
  {
    case SLEEP_ASLEEP:
      if (((now - sleep_start) >= SLEEP_GRACE_MS) && (Mic_GetLoudness() == MIC_LOUD))
      {
        Sleep_Startle(now);
        break;
      }
      if ((now - sleep_start) >= SLEEP_MAX_MS)
      {
        Sleep_Wake("WELL RESTED!", ILI9341_COLOR_GREEN, PET_MOOD_HAPPY, now);
        break;
      }
      if ((now - last_happy) >= SLEEP_HAPPY_MS)
      {
        last_happy = now;
        if (g_pet.happiness < PET_STAT_MAX) { rested++; }
        Pet_AddHappiness(1);
        Sleep_DrawRested();
      }
      if ((now - last_hunger) >= SLEEP_HUNGER_MS)
      {
        last_hunger = now;
        Pet_AddHunger(1);
      }
      if ((now - last_z) >= Z_FRAME_MS)
      {
        last_z = now;
        z_phase = (uint8_t)((z_phase + 1U) % 4U);
        Sleep_DrawZ(z_phase);
      }
      break;

    case SLEEP_WAKING:
      if ((now - wake_start) >= WAKE_MS) { state = SLEEP_IDLE; }
      break;

    default:
      break;
  }
}

void Sleep_OnClap(uint8_t claps)
{
  (void)claps;
  if ((state == SLEEP_ASLEEP) && ((HAL_GetTick() - sleep_start) >= SLEEP_GRACE_MS))
  {
    Sleep_Startle(HAL_GetTick());
  }
}

void Sleep_Abort(void)
{
  state = SLEEP_IDLE;
}

uint8_t Sleep_IsActive(void)
{
  return (uint8_t)(state != SLEEP_IDLE);
}
