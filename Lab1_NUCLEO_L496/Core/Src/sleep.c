/**
  ******************************************************************************
  * @file    sleep.c
  * @brief   Sleep action: cover the light sensor and the pet falls asleep.
  *
  *   WAIT --LDR dark--> SLEEP --LDR light--> RESULT --1.5 s--> IDLE
  *    |--10 s, still light------------------> RESULT ("NOT SLEEPY", no change)
  *    +--LEFT (cancel / wake up), any time before RESULT ---------> IDLE
  *
  * Input:        photoresistor on PC3 (A2), read on ADC2 by pet_analog.c.
  * State change: while asleep, +SLEEP_HAPPY_STEP happiness every
  *               SLEEP_TICK_MS, and the pet gets slightly hungrier
  *               (SLEEP_HUNGER_STEP every SLEEP_HUNGER_TICK_MS) so it can't
  *               just sleep forever (agreed with Elijah). Normal hunger
  *               ticking via Pet_Update() continues as usual.
  * Animation:    a repeating "z Z Z" loop beside the pet's head, one step per
  *               SLEEP_FRAME_MS; fully nonblocking (HAL_GetTick() timing).
  *
  * Integration rule: pet_analog.c must be initialised after MX_ADC1_Init().
  * Sleep_Start() initialises it lazily on first use, which always happens
  * after main()'s init code, so main.c does not need to change.
  ******************************************************************************
  */
#include "sleep.h"
#include "input.h"
#include "menu.h"
#include "pet.h"
#include "pet_art.h"
#include "pet_analog.h"
#include "main.h"
#include "ili9341.h"

#define BG_COLOR            ILI9341_COLOR_BLACK
#define Z_COLOR             ILI9341_COLOR_CYAN
#define TOP_H               70U
#define TITLE_Y             20U
#define HINT_Y              5U

#define SENSOR_MS           10U      /* sample the LDR every 10 ms (16-sample avg ~160 ms) */
#define SLEEP_FRAME_MS      400U     /* one Zzz animation step */
#define SLEEP_TICK_MS       2000U    /* how often sleeping recovers stats */
#define WAIT_TIMEOUT_MS     10000U
#define RESULT_MS           1500U

/* Sleep slowly raises happiness and, even more slowly, makes the pet
 * hungrier (agreed with Elijah, Oct 14). pet.h uses 0 = full, 100 = starving,
 * so "hungrier" is a POSITIVE Pet_AddHunger(). If the team flips hunger to
 * "higher = well-fed" (Minecraft style), negate SLEEP_HUNGER_STEP. */
#define SLEEP_HAPPY_STEP     2
#define SLEEP_HUNGER_STEP    1
#define SLEEP_HUNGER_TICK_MS 6000U   /* 3x slower than the happiness tick */

/* PLACEHOLDER - waiting on Elijah: replace with PET_MOOD_SLEEPY if he adds it */
#define SLEEP_FACE          PET_MOOD_NEUTRAL

/* Zzz letters: small -> large, drifting up and to the right of the head.
 * Built-in font assumed to be 6 x 8 px per character at size 1. */
#define CHAR_W(sz)          (6U * (sz))
#define CHAR_H(sz)          (8U * (sz))
#define Z_COUNT             3U

typedef struct { int16_t x; int16_t y; uint8_t size; } ZPos_t;

static const ZPos_t z_pos[Z_COUNT] =
{
  { (int16_t)(PET_HEAD_X + PET_HEAD_W + 4),  (int16_t)(PET_HEAD_Y + 4),   1U },
  { (int16_t)(PET_HEAD_X + PET_HEAD_W + 14), (int16_t)(PET_HEAD_Y - 14),  2U },
  { (int16_t)(PET_HEAD_X + PET_HEAD_W + 30), (int16_t)(PET_HEAD_Y - 40),  3U },
};

typedef enum { SLEEP_IDLE, SLEEP_WAIT, SLEEP_ASLEEP, SLEEP_RESULT } SleepState_t;

static SleepState_t state = SLEEP_IDLE;
static uint8_t      sensor_ready;
static uint8_t      z_step;          /* 0..Z_COUNT: how many Z's are showing */
static uint32_t     last_sensor;
static uint32_t     last_frame;
static uint32_t     last_tick;
static uint32_t     last_hunger_tick;
static uint32_t     phase_start;

/* Only draw a Z if it fits on screen and below the title strip. */
static uint8_t Z_Fits(const ZPos_t *z)
{
  return (uint8_t)((z->x >= 0) && (z->y >= (int16_t)TOP_H) &&
                   ((z->x + (int16_t)CHAR_W(z->size)) <= (int16_t)ILI9341_WIDTH));
}

static void Z_Draw(uint8_t i)
{
  if (Z_Fits(&z_pos[i]))
  {
    ILI9341_DrawChar((uint16_t)z_pos[i].x, (uint16_t)z_pos[i].y, 'Z', Z_COLOR, BG_COLOR, z_pos[i].size);
  }
}

static void Z_EraseAll(void)
{
  for (uint8_t i = 0U; i < Z_COUNT; i++)
  {
    if (Z_Fits(&z_pos[i]))
    {
      ILI9341_FillRect((uint16_t)z_pos[i].x, (uint16_t)z_pos[i].y,
                       CHAR_W(z_pos[i].size), CHAR_H(z_pos[i].size), BG_COLOR);
    }
  }
}

static void Sleep_ShowTitle(const char *text, uint16_t color, uint8_t size)
{
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  Menu_DrawCentered(TITLE_Y, text, color, size);
}

static void Sleep_FallAsleep(uint32_t now)
{
  Sleep_ShowTitle("ZZZ...", Z_COLOR, 3);
  Menu_DrawCentered(HINT_Y, "UNCOVER TO WAKE  LEFT WAKE", ILI9341_COLOR_GRAY, 1);
  PetArt_Move(0, 0, SLEEP_FACE);
  z_step     = 0U;
  last_frame       = now;
  last_tick        = now;
  last_hunger_tick = now;
  state = SLEEP_ASLEEP;
}

static void Sleep_WakeUp(uint32_t now)
{
  Z_EraseAll();
  PetArt_Move(0, 0, PET_MOOD_HAPPY);
  Sleep_ShowTitle("GOOD MORNING!", ILI9341_COLOR_YELLOW, 2);
  phase_start = now;
  state = SLEEP_RESULT;
}

void Sleep_Start(void)
{
  uint32_t now = HAL_GetTick();

  if (!sensor_ready)
  {
    PetAnalog_Init();          /* one-time ~1 ms setup; after MX_ADC1_Init() by construction */
    sensor_ready = 1U;
  }
  /* Refresh the moving average so a stale reading from last time can't
   * trigger an instant sleep or wake (16 quick conversions, well under 1 ms). */
  for (uint8_t i = 0U; i < PET_ANALOG_AVG_SAMPLES; i++) { PetAnalog_Update(); }

  MainScreen_Draw();
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  last_sensor = now;
  phase_start = now;

  if (PetAnalog_IsDark())
  {
    Sleep_FallAsleep(now);     /* already dark - go straight to sleep */
    return;
  }

  Menu_DrawCentered(HINT_Y, "COVER THE LIGHT  LEFT CANCEL", ILI9341_COLOR_GRAY, 1);
  Menu_DrawCentered(TITLE_Y, "SLEEPY TIME?", ILI9341_COLOR_WHITE, 2);
  PetArt_Move(0, 0, PET_MOOD_NEUTRAL);
  state = SLEEP_WAIT;
}

void Sleep_HandleInput(uint8_t pressed)
{
  if (!(pressed & JOY_LEFT_MASK)) { return; }

  if (state == SLEEP_WAIT)        { state = SLEEP_IDLE; }            /* cancel */
  else if (state == SLEEP_ASLEEP) { Sleep_WakeUp(HAL_GetTick()); }   /* wake early */
}

void Sleep_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (state == SLEEP_IDLE) { return; }

  if (state == SLEEP_RESULT)
  {
    if ((now - phase_start) >= RESULT_MS) { state = SLEEP_IDLE; }
    return;
  }

  /* Sample the LDR at a fixed rate instead of every loop pass. */
  if ((now - last_sensor) >= SENSOR_MS)
  {
    last_sensor = now;
    PetAnalog_Update();
  }

  if (state == SLEEP_WAIT)
  {
    if (PetAnalog_IsDark())
    {
      Sleep_FallAsleep(now);
    }
    else if ((now - phase_start) >= WAIT_TIMEOUT_MS)
    {
      Sleep_ShowTitle("NOT SLEEPY", ILI9341_COLOR_CYAN, 2);
      phase_start = now;
      state = SLEEP_RESULT;
    }
    return;
  }

  /* SLEEP_ASLEEP */
  if (!PetAnalog_IsDark())
  {
    Sleep_WakeUp(now);
    return;
  }

  if ((now - last_tick) >= SLEEP_TICK_MS)
  {
    last_tick = now;
    Pet_AddHappiness(SLEEP_HAPPY_STEP);
  }

  if ((now - last_hunger_tick) >= SLEEP_HUNGER_TICK_MS)
  {
    last_hunger_tick = now;
    Pet_AddHunger(SLEEP_HUNGER_STEP);
  }

  if ((now - last_frame) >= SLEEP_FRAME_MS)
  {
    last_frame = now;
    if (z_step < Z_COUNT)
    {
      Z_Draw(z_step);          /* add the next, bigger Z */
      z_step++;
    }
    else
    {
      Z_EraseAll();            /* clear and start the loop again */
      z_step = 0U;
    }
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
