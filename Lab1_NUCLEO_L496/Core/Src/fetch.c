/**
  ******************************************************************************
  * @file    fetch.c
  * @brief   Fetch action: time the throw, then the pet runs to get the ball.
  *
  *   AIM --CENTER--> FLY --ball lands--> RUN --reaches ball--> BACK --home--> RESULT --1.8 s--> IDLE
  *    +--LEFT (cancel)-------------------------------------------------------------------------> IDLE
  *
  * Input:        a power bar sweeps 0..100..0; press CENTER to throw.
  * State change: depends on the power - see Fetch_Finish(). Always costs
  *               FETCH_HUNGER (running is tiring).
  * Animation:    the ball flies in an arc, the pet runs over to it and
  *               carries it back in its mouth (or comes back sad if lost).
  ******************************************************************************
  */
#include "fetch.h"
#include "input.h"
#include "menu.h"
#include "pet.h"
#include "pet_art.h"
#include "main.h"
#include "ili9341.h"

#define BG_COLOR        ILI9341_COLOR_BLACK
#define BALL_COLOR      ILI9341_COLOR_YELLOW

#define TOP_H           70U
#define TITLE_Y         20U
#define HINT_Y          5U

/* Power bar: power 0..100 maps to 0..200 px */
#define POWER_X         20U
#define POWER_Y         50U
#define POWER_W         200U
#define POWER_H         8U
#define POWER_STEP      4U      /* per frame -> one sweep takes ~1 s */
#define POWER_GREAT     70U     /* 70..94: great throw (green zone) */
#define POWER_LOST      95U     /* 95+: too strong, ball flies off-screen */
#define POWER_OK        40U     /* 40..69: fine; below 40: short */

/* Ball flight: from just right of the pet to the ground, in an arc */
#define BALL_SIZE       8U
#define BALL_START_X    (PET_HEAD_X + PET_HEAD_W + 2U)
#define BALL_START_Y    (PET_HEAD_Y + 20U)
#define BALL_GROUND_Y   (PET_HEAD_Y + PET_HEAD_H - BALL_SIZE)
#define LAND_MIN_X      180U
#define LAND_RANGE_X    45U     /* power 100 would land at 225 */
#define LAND_LOST_X     280     /* off the right edge */
#define FLY_FRAMES      20U

#define RUN_STEP_PX     6
#define FRAME_MS        40U
#define RESULT_MS       1800U
#define FETCH_HUNGER    10

typedef enum { FETCH_IDLE, FETCH_AIM, FETCH_FLY, FETCH_RUN, FETCH_BACK, FETCH_RESULT } FetchState_t;

static FetchState_t state = FETCH_IDLE;
static uint8_t      power;
static int8_t       power_dir;
static uint8_t      lost;
static int16_t      land_x;
static int16_t      peak;
static uint8_t      fly_t;
static int16_t      ball_x;
static int16_t      ball_y;
static uint8_t      ball_drawn;
static int16_t      pet_dx;
static int16_t      target_dx;
static uint32_t     last_frame;
static uint32_t     phase_start;

static void Fetch_DrawPower(void)
{
  uint16_t fill = (uint16_t)(power * 2U);
  uint16_t color = (power >= POWER_LOST) ? ILI9341_COLOR_RED
                 : (power >= POWER_GREAT) ? ILI9341_COLOR_GREEN : ILI9341_COLOR_YELLOW;

  ILI9341_FillRect(POWER_X, POWER_Y, fill, POWER_H, color);
  ILI9341_FillRect(POWER_X + fill, POWER_Y, (uint16_t)(POWER_W - fill), POWER_H, BG_COLOR);
}

static void Fetch_Ball(uint8_t visible)
{
  ILI9341_FillRect((uint16_t)ball_x, (uint16_t)ball_y, BALL_SIZE, BALL_SIZE, visible ? BALL_COLOR : BG_COLOR);
  ball_drawn = visible;
}

/* The state change - result depends on how hard the ball was thrown */
static void Fetch_Finish(uint32_t now)
{
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  Pet_AddHunger(FETCH_HUNGER);

  if (lost)
  {
    Pet_AddHappiness(-5);
    Menu_DrawCentered(TITLE_Y, "LOST IT!", ILI9341_COLOR_RED, 3);
  }
  else if (power >= POWER_GREAT)
  {
    Pet_AddHappiness(15);
    Menu_DrawCentered(TITLE_Y, "GREAT THROW!", ILI9341_COLOR_GREEN, 3);
  }
  else if (power >= POWER_OK)
  {
    Pet_AddHappiness(10);
    Menu_DrawCentered(TITLE_Y, "FETCHED!", ILI9341_COLOR_YELLOW, 3);
  }
  else
  {
    Pet_AddHappiness(5);
    Menu_DrawCentered(TITLE_Y, "SHORT ONE", ILI9341_COLOR_CYAN, 3);
  }

  phase_start = now;
  state = FETCH_RESULT;
}

static void Fetch_Throw(void)
{
  lost = (uint8_t)(power >= POWER_LOST);
  land_x = lost ? (int16_t)LAND_LOST_X : (int16_t)(LAND_MIN_X + ((power * LAND_RANGE_X) / 100U));
  peak = (int16_t)(30 + ((power * 7) / 10));   /* harder throw, higher arc */
  fly_t = 0;
  ball_x = (int16_t)BALL_START_X;
  ball_y = (int16_t)BALL_START_Y;
  Fetch_Ball(1U);
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, POWER_Y - 6U, BG_COLOR);   /* clear hint + title, keep the bar */
  Menu_DrawCentered(TITLE_Y, "GO GET IT!", ILI9341_COLOR_WHITE, 2);
  state = FETCH_FLY;
}

void Fetch_Start(void)
{
  uint32_t now = HAL_GetTick();

  MainScreen_Draw();
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  last_frame = now;
  phase_start = now;

  if (g_pet.hunger >= PET_TOO_HUNGRY_TO_PLAY)
  {
    Menu_DrawCentered(TITLE_Y, "TOO HUNGRY", ILI9341_COLOR_RED, 3);
    lost = 0U;
    state = FETCH_RESULT;   /* show the refusal, no stat change */
    return;
  }

  Menu_DrawCentered(HINT_Y, "PRESS TO THROW  LEFT CANCEL", ILI9341_COLOR_GRAY, 1);
  Menu_DrawCentered(TITLE_Y, "AIM FOR GREEN", ILI9341_COLOR_WHITE, 2);
  ILI9341_DrawRect(POWER_X - 1U, POWER_Y - 1U, POWER_W + 2U, POWER_H + 2U, ILI9341_COLOR_WHITE);
  /* Mark the green zone above the bar */
  ILI9341_FillRect(POWER_X + (POWER_GREAT * 2U), POWER_Y - 5U, (POWER_LOST - POWER_GREAT) * 2U, 3U, ILI9341_COLOR_GREEN);
  PetArt_Move(0, 0, PET_MOOD_HAPPY);

  power = 0;
  power_dir = 1;
  pet_dx = 0;
  state = FETCH_AIM;
}

void Fetch_HandleInput(uint8_t pressed)
{
  if (state != FETCH_AIM) { return; }   /* ignore buttons once the ball is thrown */

  if (pressed & JOY_CENTER_MASK)    { Fetch_Throw(); }
  else if (pressed & JOY_LEFT_MASK) { state = FETCH_IDLE; }   /* cancel */
}

void Fetch_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (state == FETCH_IDLE) { return; }
  if (state == FETCH_RESULT)
  {
    if ((now - phase_start) >= RESULT_MS) { state = FETCH_IDLE; }
    return;
  }
  if ((now - last_frame) < FRAME_MS) { return; }   /* not time for the next frame yet */
  last_frame = now;

  switch (state)
  {
    case FETCH_AIM:
      /* Sweep the power up and down until the player throws */
      if (power_dir > 0)
      {
        power = (uint8_t)(power + POWER_STEP);
        if (power >= 100U) { power = 100U; power_dir = -1; }
      }
      else
      {
        power = (power > POWER_STEP) ? (uint8_t)(power - POWER_STEP) : 0U;
        if (power == 0U) { power_dir = 1; }
      }
      Fetch_DrawPower();
      break;

    case FETCH_FLY:
    {
      /* Straight line from start to landing, minus a parabola for the arc */
      int32_t t = (int32_t)++fly_t;
      int32_t n = (int32_t)FLY_FRAMES;

      if (ball_drawn) { Fetch_Ball(0U); }
      ball_x = (int16_t)(BALL_START_X + (((land_x - (int32_t)BALL_START_X) * t) / n));
      ball_y = (int16_t)(BALL_START_Y + (((int32_t)(BALL_GROUND_Y - BALL_START_Y) * t) / n)
                         - ((4 * peak * t * (n - t)) / (n * n)));
      if ((ball_x + (int16_t)BALL_SIZE) <= (int16_t)ILI9341_WIDTH) { Fetch_Ball(1U); }   /* off-screen: not drawn */

      if (fly_t >= FLY_FRAMES)
      {
        /* Run until the pet's right edge reaches the far side of the ball */
        target_dx = lost ? PET_ART_MAX_DX
                         : (int16_t)(land_x + (int16_t)BALL_SIZE - (int16_t)(PET_HEAD_X + PET_HEAD_W));
        state = FETCH_RUN;
      }
      break;
    }

    case FETCH_RUN:
      pet_dx = (int16_t)(pet_dx + RUN_STEP_PX);
      if (pet_dx >= target_dx) { pet_dx = target_dx; }
      PetArt_Move(pet_dx, 0, (lost && (pet_dx == target_dx)) ? PET_MOOD_SAD : PET_MOOD_HAPPY);
      if (pet_dx == target_dx) { state = FETCH_BACK; }   /* the pet now covers the ball - picked up */
      break;

    case FETCH_BACK:
      pet_dx = (int16_t)(pet_dx - RUN_STEP_PX);
      if (pet_dx < 0) { pet_dx = 0; }
      PetArt_Move(pet_dx, 0, lost ? PET_MOOD_SAD : PET_MOOD_HAPPY);
      if (!lost)
      {
        /* Ball carried in the mouth */
        ILI9341_FillRect((uint16_t)(PET_MOUTH_X + pet_dx + ((PET_MOUTH_W - BALL_SIZE) / 2U)), PET_MOUTH_Y + 2U,
                         BALL_SIZE, BALL_SIZE, BALL_COLOR);
      }
      if (pet_dx == 0) { Fetch_Finish(now); }
      break;

    default:
      break;
  }
}

void Fetch_Abort(void)
{
  state = FETCH_IDLE;
}

uint8_t Fetch_IsActive(void)
{
  return (uint8_t)(state != FETCH_IDLE);
}
