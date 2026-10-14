/**
  ******************************************************************************
  * @file    dance.c
  * @brief   Dance action: a rhythm game - copy the moves to make the pet dance.
  *
  *   INTRO --1.2 s--> PROMPT --right direction--> POSE (pet does the move)
  *                      |  --wrong / too slow--> POSE (sad, stays put)
  *                      +<------- next beat --------+
  *   after DANCE_BEATS beats --> RESULT --1.8 s--> IDLE
  *
  * Input:        a random move (LEFT/RIGHT/UP/DOWN) per beat, pressed before
  *               the timer bar runs out.
  * State change: happiness +HAPPY_PER_HIT per hit, hunger +DANCE_HUNGER.
  * Animation:    the pet slides, jumps or squats to match each hit.
  ******************************************************************************
  */
#include <stdio.h>
#include "dance.h"
#include "input.h"
#include "menu.h"
#include "pet.h"
#include "pet_art.h"
#include "main.h"
#include "ili9341.h"

#define BG_COLOR        ILI9341_COLOR_BLACK

#define DANCE_BEATS     8U
#define BEAT_MS         1000U   /* time allowed to press the move */
#define POSE_MS         350U    /* how long the pet holds a move */
#define INTRO_MS        1200U
#define RESULT_MS       1800U
#define FRAME_MS        40U

#define HAPPY_PER_HIT   3
#define DANCE_HUNGER    8       /* dancing is tiring */

#define TOP_H           70U     /* text strip above the pet */
#define PROMPT_Y        18U
#define TIMER_X         20U
#define TIMER_Y         48U
#define TIMER_W         200U
#define TIMER_H         4U
#define STATUS_Y        58U

typedef enum { DANCE_IDLE, DANCE_INTRO, DANCE_PROMPT, DANCE_POSE, DANCE_RESULT } DanceState_t;

typedef struct
{
  uint8_t     mask;
  const char *label;   /* 5 chars so each prompt fully overwrites the last */
  int16_t     dx;
  int16_t     dy;
} Move_t;

static const Move_t moves[] =
{
  { JOY_LEFT_MASK,  "LEFT ", -25,   0 },   /* slide left */
  { JOY_RIGHT_MASK, "RIGHT",  25,   0 },   /* slide right */
  { JOY_UP_MASK,    " UP  ",   0, -20 },   /* jump */
  { JOY_DOWN_MASK,  "DOWN ",   0,  10 },   /* squat */
};
#define MOVE_COUNT ((uint8_t)(sizeof(moves) / sizeof(moves[0])))
#define MOVE_MASKS (JOY_LEFT_MASK | JOY_RIGHT_MASK | JOY_UP_MASK | JOY_DOWN_MASK)

static DanceState_t state = DANCE_IDLE;
static uint8_t      beat;
static uint8_t      hits;
static uint8_t      move;
static uint32_t     phase_start;
static uint32_t     last_frame;
static uint32_t     rng;

static uint8_t Dance_RandomMove(void)
{
  rng = (rng * 1103515245U) + 12345U;   /* simple LCG - plenty for picking moves */
  return (uint8_t)((rng >> 16) % MOVE_COUNT);
}

static void Dance_DrawStatus(void)
{
  char text[24];

  (void)snprintf(text, sizeof(text), "BEAT %u/%u   HITS %u", (unsigned)(beat + 1U), (unsigned)DANCE_BEATS, (unsigned)hits);
  Menu_DrawCentered(STATUS_Y, text, ILI9341_COLOR_GRAY, 1);
}

static void Dance_NextBeat(uint32_t now)
{
  move = Dance_RandomMove();
  Menu_DrawCentered(PROMPT_Y, moves[move].label, ILI9341_COLOR_WHITE, 4);
  ILI9341_FillRect(TIMER_X, TIMER_Y, TIMER_W, TIMER_H, ILI9341_COLOR_WHITE);
  Dance_DrawStatus();
  phase_start = now;
  state = DANCE_PROMPT;
}

/* Hit: pet strikes the pose. Miss: sad face, stays put. */
static void Dance_Pose(uint8_t hit, uint32_t now)
{
  if (hit)
  {
    hits++;
    Menu_DrawCentered(PROMPT_Y, moves[move].label, ILI9341_COLOR_GREEN, 4);
    PetArt_Move(moves[move].dx, moves[move].dy, PET_MOOD_HAPPY);
  }
  else
  {
    Menu_DrawCentered(PROMPT_Y, moves[move].label, ILI9341_COLOR_RED, 4);
    PetArt_Move(0, 0, PET_MOOD_SAD);
  }
  ILI9341_FillRect(TIMER_X, TIMER_Y, TIMER_W, TIMER_H, BG_COLOR);
  Dance_DrawStatus();
  phase_start = now;
  state = DANCE_POSE;
}

static void Dance_Finish(uint32_t now)
{
  Pet_AddHappiness((int16_t)(hits * HAPPY_PER_HIT));
  Pet_AddHunger(DANCE_HUNGER);

  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  if (hits >= 7U)      { Menu_DrawCentered(PROMPT_Y, "AMAZING!", ILI9341_COLOR_YELLOW, 3); }
  else if (hits >= 4U) { Menu_DrawCentered(PROMPT_Y, "NICE MOVES!", ILI9341_COLOR_GREEN, 3); }
  else                 { Menu_DrawCentered(PROMPT_Y, "OOPS!", ILI9341_COLOR_RED, 3); }
  PetArt_Move(0, 0, (hits >= 4U) ? PET_MOOD_HAPPY : PET_MOOD_SAD);

  phase_start = now;
  state = DANCE_RESULT;
}

void Dance_Start(void)
{
  uint32_t now = HAL_GetTick();

  MainScreen_Draw();
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
  phase_start = now;
  last_frame = now;

  if (g_pet.hunger >= PET_TOO_HUNGRY_TO_PLAY)
  {
    Menu_DrawCentered(PROMPT_Y, "TOO HUNGRY", ILI9341_COLOR_RED, 3);
    state = DANCE_RESULT;   /* show the refusal, no stat change */
    return;
  }

  Menu_DrawCentered(PROMPT_Y, "LET'S DANCE!", ILI9341_COLOR_MAGENTA, 3);
  Menu_DrawCentered(STATUS_Y, "COPY THE MOVES", ILI9341_COLOR_GRAY, 1);
  PetArt_Move(0, 0, PET_MOOD_HAPPY);
  rng = now | 1U;
  beat = 0;
  hits = 0;
  state = DANCE_INTRO;
}

void Dance_HandleInput(uint8_t pressed)
{
  uint8_t mask;

  if (state != DANCE_PROMPT) { return; }   /* ignore buttons between beats */

  pressed &= MOVE_MASKS;                    /* CENTER does nothing here */
  if (pressed == 0U) { return; }

  mask = moves[move].mask;
  Dance_Pose((uint8_t)(pressed == mask), HAL_GetTick());
}

void Dance_Update(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t elapsed = now - phase_start;

  switch (state)
  {
    case DANCE_INTRO:
      if (elapsed >= INTRO_MS)
      {
        ILI9341_FillRect(0, 0, ILI9341_WIDTH, TOP_H, BG_COLOR);
        Dance_NextBeat(now);
      }
      break;

    case DANCE_PROMPT:
      if (elapsed >= BEAT_MS)
      {
        Dance_Pose(0U, now);   /* too slow */
      }
      else if ((now - last_frame) >= FRAME_MS)
      {
        /* Shrink the timer bar from the right */
        uint16_t left = (uint16_t)((TIMER_W * (BEAT_MS - elapsed)) / BEAT_MS);
        last_frame = now;
        ILI9341_FillRect(TIMER_X + left, TIMER_Y, (uint16_t)(TIMER_W - left), TIMER_H, BG_COLOR);
      }
      break;

    case DANCE_POSE:
      if (elapsed >= POSE_MS)
      {
        PetArt_Move(0, 0, PET_MOOD_HAPPY);   /* back to the middle */
        beat++;
        if (beat >= DANCE_BEATS) { Dance_Finish(now); }
        else                     { Dance_NextBeat(now); }
      }
      break;

    case DANCE_RESULT:
      if (elapsed >= RESULT_MS) { state = DANCE_IDLE; }
      break;

    default:
      break;
  }
}

void Dance_Abort(void)
{
  state = DANCE_IDLE;
}

uint8_t Dance_IsActive(void)
{
  return (uint8_t)(state != DANCE_IDLE);
}
