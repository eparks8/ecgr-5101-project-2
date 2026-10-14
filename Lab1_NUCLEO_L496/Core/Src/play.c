/**
  ******************************************************************************
  * @file    play.c
  * @brief   Play action: shake the vibration switch (or press the play
  *          button) and the pet bounces happily.
  *
  *   WAIT --trigger--> BOUNCE --3 bounces--> RESULT --1.5 s--> IDLE
  *    |--8 s, no trigger--------------------> RESULT ("MAYBE LATER", no change)
  *    +--LEFT (cancel)-------------------------------------------------> IDLE
  *
  * Input:        vibration switch or breadboard pushbutton on a GPIO pin.
  *               Until that pin is confirmed free with the GFX01M2 attached,
  *               the joystick CENTER button stands in for it (see below).
  * State change: +PLAY_HAPPY happiness, +PLAY_HUNGER hunger (playing is tiring).
  *               Refused (no change) when the pet is too hungry, like Fetch.
  * Animation:    the pet bounces up and down over many frames using
  *               PetArt_Move(); fully nonblocking (HAL_GetTick() timing).
  ******************************************************************************
  */
#include "play.h"
#include "input.h"
#include "menu.h"
#include "pet.h"
#include "pet_art.h"
#include "main.h"
#include "ili9341.h"

/* ---- Physical trigger ---------------------------------------------------
 * Uncomment and fill in ONLY after the pin is verified to be unused by the
 * X-NUCLEO-GFX01M2 and the rest of the team's code. Reconfiguring a pin the
 * display uses would break the display.
 * While these stay commented out, joystick CENTER triggers the bounce.     */
/* #define PLAY_SENSOR_PORT          GPIOx                          */
/* #define PLAY_SENSOR_PIN           GPIO_PIN_y                     */
/* #define PLAY_SENSOR_CLK_ENABLE()  __HAL_RCC_GPIOx_CLK_ENABLE()   */
#define PLAY_SENSOR_ACTIVE        GPIO_PIN_RESET   /* switch/button to GND, internal pull-up */

#define BG_COLOR            ILI9341_COLOR_BLACK
#define TOP_H               70U
#define TITLE_Y             20U
#define HINT_Y              5U

#define FRAME_MS            40U
#define WAIT_TIMEOUT_MS     8000U
#define RESULT_MS           1500U
#define TRIGGER_LOCKOUT_MS  150U    /* ignore switch chatter after a trigger */

#define PLAY_HAPPY          15
#define PLAY_HUNGER         5

/* One bounce = one pass through this table (pet goes up and comes back).
 * Must stay within PET_ART_MIN_DY..PET_ART_MAX_DY. */
static const int8_t bounce_dy[] = { 0, -8, -16, -22, -26, -28, -26, -22, -16, -8 };
#define BOUNCE_STEPS        ((uint8_t)(sizeof(bounce_dy) / sizeof(bounce_dy[0])))
#define BOUNCE_COUNT        3U

typedef enum { PLAY_IDLE, PLAY_WAIT, PLAY_BOUNCE, PLAY_RESULT } PlayState_t;

static PlayState_t state = PLAY_IDLE;
static uint32_t    last_frame;
static uint32_t    phase_start;
static uint8_t     step;
static uint8_t     bounces;
static uint8_t     triggered;     /* latched until the WAIT state consumes it */

#ifdef PLAY_SENSOR_PIN
static uint8_t  sensor_ready;
static uint8_t  sensor_prev;
static uint32_t last_trigger;

static void Play_SensorInit(void)
{
  GPIO_InitTypeDef gpio = {0};

  PLAY_SENSOR_CLK_ENABLE();
  gpio.Pin  = PLAY_SENSOR_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PLAY_SENSOR_PORT, &gpio);
  sensor_prev  = 0U;
  sensor_ready = 1U;
}

/* Polled on every Play_Update() call, not once per frame, because
 * vibration-switch pulses are short. Rising edge of "active" = trigger. */
static void Play_SensorPoll(uint32_t now)
{
  uint8_t active = (uint8_t)(HAL_GPIO_ReadPin(PLAY_SENSOR_PORT, PLAY_SENSOR_PIN) == PLAY_SENSOR_ACTIVE);

  if (active && !sensor_prev && ((now - last_trigger) >= TRIGGER_LOCKOUT_MS))
  {
    triggered    = 1U;
    last_trigger = now;
  }
  sensor_prev = active;
}
#endif

static void Play_ShowTitle(const char *text, uint16_t color, uint8_t size)
{
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  Menu_DrawCentered(TITLE_Y, text, color, size);
}

void Play_Start(void)
{
  uint32_t now = HAL_GetTick();

#ifdef PLAY_SENSOR_PIN
  if (!sensor_ready) { Play_SensorInit(); }   /* lazy init: no main.c change needed */
#endif

  MainScreen_Draw();
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  last_frame  = now;
  phase_start = now;
  triggered   = 0U;

  if (g_pet.hunger >= PET_TOO_HUNGRY_TO_PLAY)
  {
    Menu_DrawCentered(TITLE_Y, "TOO HUNGRY", ILI9341_COLOR_RED, 3);
    state = PLAY_RESULT;   /* show the refusal, no stat change */
    return;
  }

#ifdef PLAY_SENSOR_PIN
  Menu_DrawCentered(HINT_Y, "SHAKE TO PLAY  LEFT CANCEL", ILI9341_COLOR_GRAY, 1);
#else
  Menu_DrawCentered(HINT_Y, "PRESS TO PLAY  LEFT CANCEL", ILI9341_COLOR_GRAY, 1);
#endif
  Menu_DrawCentered(TITLE_Y, "PLAY WITH ME!", ILI9341_COLOR_WHITE, 2);
  PetArt_Move(0, 0, PET_MOOD_HAPPY);
  state = PLAY_WAIT;
}

void Play_HandleInput(uint8_t pressed)
{
  if (state != PLAY_WAIT) { return; }   /* ignore buttons once bouncing */

  if (pressed & JOY_LEFT_MASK) { state = PLAY_IDLE; return; }   /* cancel */
#ifndef PLAY_SENSOR_PIN
  if (pressed & JOY_CENTER_MASK) { triggered = 1U; }           /* stand-in trigger */
#endif
}

void Play_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (state == PLAY_IDLE) { return; }

#ifdef PLAY_SENSOR_PIN
  if (state == PLAY_WAIT) { Play_SensorPoll(now); }
#endif

  if (state == PLAY_RESULT)
  {
    if ((now - phase_start) >= RESULT_MS) { state = PLAY_IDLE; }
    return;
  }

  if (state == PLAY_WAIT)
  {
    if (triggered)
    {
      triggered  = 0U;
      step       = 0U;
      bounces    = 0U;
      last_frame = now;
      Play_ShowTitle("WHEE!", ILI9341_COLOR_YELLOW, 3);
      state = PLAY_BOUNCE;
    }
    else if ((now - phase_start) >= WAIT_TIMEOUT_MS)
    {
      Play_ShowTitle("MAYBE LATER", ILI9341_COLOR_CYAN, 2);
      phase_start = now;
      state = PLAY_RESULT;
    }
    return;
  }

  /* PLAY_BOUNCE: one animation frame per FRAME_MS, never blocks */
  if ((now - last_frame) < FRAME_MS) { return; }
  last_frame = now;

  PetArt_Move(0, bounce_dy[step], PET_MOOD_HAPPY);

  if (++step >= BOUNCE_STEPS)
  {
    step = 0U;
    if (++bounces >= BOUNCE_COUNT)
    {
      PetArt_Move(0, 0, PET_MOOD_HAPPY);   /* land back home */
      Pet_AddHappiness(PLAY_HAPPY);
      Pet_AddHunger(PLAY_HUNGER);
      Play_ShowTitle("SO FUN!", ILI9341_COLOR_GREEN, 3);
      phase_start = now;
      state = PLAY_RESULT;
    }
  }
}

void Play_Abort(void)
{
  state     = PLAY_IDLE;
  triggered = 0U;
}

uint8_t Play_IsActive(void)
{
  return (uint8_t)(state != PLAY_IDLE);
}
