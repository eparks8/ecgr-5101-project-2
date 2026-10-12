/**
  ******************************************************************************
  * @file    test_pet.c
  * @brief   TEMPORARY test pet + meters for exercising the menu and feed action.
  *
  * Replaces the weak MainScreen_Draw()/MainScreen_Update() in menu.c. Delete
  * this file (and its line in CMakeLists.txt) once the real pet and main
  * screen are in.
  *
  * The face follows Pet_GetMood(), and each mood has a small idle animation:
  *   HUNGRY  - worried brows, open mouth, blinking "FEED ME!"
  *             (a "DYING NN" countdown once hunger hits max)
  *   SAD     - worried brows, frown, a tear rolling down the cheek
  *   HAPPY   - smile, sparkles twinkling beside the head
  *   NEUTRAL - flat mouth
  * Every mood blinks. Only the parts that change are redrawn.
  ******************************************************************************
  */
#include <stdio.h>
#include "main.h"
#include "menu.h"
#include "pet.h"
#include "ili9341.h"

#define BG_COLOR    ILI9341_COLOR_BLACK
#define METER_X     90U
#define METER_W     122U   /* 1 px border on each side + 120 px fill */
#define METER_H     12U
#define HUNGER_Y    255U
#define HAPPY_Y     280U

/* Face features, relative to the shared head/mouth layout in pet.h */
#define EYE_SIZE    10U
#define EYE_L_X     (PET_HEAD_X + 25U)
#define EYE_R_X     (PET_HEAD_X + PET_HEAD_W - 35U)
#define EYE_Y       (PET_HEAD_Y + 20U)
#define FACE_X      (PET_HEAD_X + 10U)   /* area cleared when the expression changes */
#define FACE_Y      (PET_HEAD_Y + 8U)
#define FACE_W      (PET_HEAD_W - 20U)
#define FACE_H      (PET_HEAD_H - 12U)
#define TEAR_X      (EYE_L_X + 2U)
#define TEAR_TOP_Y  (EYE_Y + EYE_SIZE + 2U)
#define TEAR_END_Y  (PET_MOUTH_Y - 4U)
#define TEAR_W      4U
#define TEAR_H      6U
#define FEED_ME_X   72U                  /* "FEED ME!" at size 2 is 96 px wide */
#define FEED_ME_Y   (PET_HEAD_Y - 35U)
#define SPARKLE_L_X (PET_HEAD_X - 20U)
#define SPARKLE_R_X (PET_HEAD_X + PET_HEAD_W + 8U)
#define SPARKLE_L_Y (PET_HEAD_Y + 10U)
#define SPARKLE_R_Y (PET_HEAD_Y + 30U)

/* Animation timing, in 40 ms frames */
#define FRAME_MS            40U
#define BLINK_PERIOD        75U   /* blink every 3 s ... */
#define BLINK_CLOSED        4U    /* ... with eyes shut for 160 ms */
#define FEED_ME_TOGGLE      12U
#define SPARKLE_TOGGLE      10U

/* TEMPORARY stand-in for the time-based hunger feature so the moods can be
 * seen changing. Remove once the real timer exists. */
#ifndef TEST_PET_DRIFT_MS
#define TEST_PET_DRIFT_MS   4000U   /* 1 hunger every 4 s; 0 disables */
#endif

static PetMood_t drawn_mood;
static uint8_t   drawn_hunger;
static uint8_t   drawn_happiness;
static uint32_t  frame;
static uint32_t  last_frame;
static uint16_t  tear_y;
#if TEST_PET_DRIFT_MS > 0
static uint32_t  last_drift;
#endif

static void TestPet_DrawMeter(uint16_t y, const char *label, uint8_t value, uint16_t color)
{
  char     text[4];
  uint16_t fill = (uint16_t)(((uint32_t)value * (METER_W - 2U)) / PET_STAT_MAX);

  ILI9341_DrawString(20, y + 2U, label, ILI9341_COLOR_WHITE, BG_COLOR, 1);
  ILI9341_DrawRect(METER_X, y, METER_W, METER_H, ILI9341_COLOR_WHITE);
  ILI9341_FillRect(METER_X + 1U, y + 1U, fill, METER_H - 2U, color);
  ILI9341_FillRect(METER_X + 1U + fill, y + 1U, (uint16_t)(METER_W - 2U - fill), METER_H - 2U, BG_COLOR);

  (void)snprintf(text, sizeof(text), "%3u", value);
  ILI9341_DrawString(METER_X + METER_W + 4U, y + 2U, text, ILI9341_COLOR_WHITE, BG_COLOR, 1);
}

static void TestPet_DrawMeters(void)
{
  TestPet_DrawMeter(HUNGER_Y, "HUNGER", g_pet.hunger, ILI9341_COLOR_ORANGE);
  TestPet_DrawMeter(HAPPY_Y, "HAPPY", g_pet.happiness, ILI9341_COLOR_YELLOW);
  drawn_hunger = g_pet.hunger;
  drawn_happiness = g_pet.happiness;
}

/* 2 px thick line, since the driver's lines are 1 px */
static void TestPet_ThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  ILI9341_DrawLine(x0, y0, x1, y1, ILI9341_COLOR_BLACK);
  ILI9341_DrawLine(x0, (int16_t)(y0 + 1), x1, (int16_t)(y1 + 1), ILI9341_COLOR_BLACK);
}

static void TestPet_DrawEyes(uint8_t open)
{
  ILI9341_FillRect(EYE_L_X, EYE_Y, EYE_SIZE, EYE_SIZE, open ? ILI9341_COLOR_BLACK : PET_FACE_COLOR);
  ILI9341_FillRect(EYE_R_X, EYE_Y, EYE_SIZE, EYE_SIZE, open ? ILI9341_COLOR_BLACK : PET_FACE_COLOR);
  if (!open)
  {
    ILI9341_FillRect(EYE_L_X, EYE_Y + (EYE_SIZE / 2U), EYE_SIZE, 2U, ILI9341_COLOR_BLACK);
    ILI9341_FillRect(EYE_R_X, EYE_Y + (EYE_SIZE / 2U), EYE_SIZE, 2U, ILI9341_COLOR_BLACK);
  }
}

static void TestPet_DrawFace(PetMood_t mood)
{
  const int16_t mx = (int16_t)PET_MOUTH_X;
  const int16_t my = (int16_t)PET_MOUTH_Y;
  const int16_t mw = (int16_t)PET_MOUTH_W;
  const int16_t mh = (int16_t)PET_MOUTH_H;

  ILI9341_FillRect(FACE_X, FACE_Y, FACE_W, FACE_H, PET_FACE_COLOR);
  TestPet_DrawEyes(1U);

  if ((mood == PET_MOOD_SAD) || (mood == PET_MOOD_HUNGRY))
  {
    /* Worried brows: inner ends raised */
    TestPet_ThickLine((int16_t)(EYE_L_X - 3U), (int16_t)(EYE_Y - 4U), (int16_t)(EYE_L_X + EYE_SIZE + 2U), (int16_t)(EYE_Y - 9U));
    TestPet_ThickLine((int16_t)(EYE_R_X - 2U), (int16_t)(EYE_Y - 9U), (int16_t)(EYE_R_X + EYE_SIZE + 3U), (int16_t)(EYE_Y - 4U));
  }

  switch (mood)
  {
    case PET_MOOD_HAPPY:    /* smile */
      TestPet_ThickLine(mx, my, (int16_t)(mx + 8), (int16_t)(my + mh - 2));
      TestPet_ThickLine((int16_t)(mx + 8), (int16_t)(my + mh - 2), (int16_t)(mx + mw - 8), (int16_t)(my + mh - 2));
      TestPet_ThickLine((int16_t)(mx + mw - 8), (int16_t)(my + mh - 2), (int16_t)(mx + mw), my);
      break;

    case PET_MOOD_SAD:      /* frown */
      TestPet_ThickLine(mx, (int16_t)(my + mh - 2), (int16_t)(mx + 8), my);
      TestPet_ThickLine((int16_t)(mx + 8), my, (int16_t)(mx + mw - 8), my);
      TestPet_ThickLine((int16_t)(mx + mw - 8), my, (int16_t)(mx + mw), (int16_t)(my + mh - 2));
      break;

    case PET_MOOD_HUNGRY:   /* open mouth, waiting for food */
      ILI9341_FillRect(PET_MOUTH_X + 8U, PET_MOUTH_Y, PET_MOUTH_W - 16U, PET_MOUTH_H, PET_MOUTH_COLOR);
      break;

    default:                /* neutral: flat mouth */
      ILI9341_FillRect(PET_MOUTH_X, PET_MOUTH_Y + (PET_MOUTH_H / 2U) - 1U, PET_MOUTH_W, 3U, PET_MOUTH_COLOR);
      break;
  }

  /* Clear the per-mood extras outside the head so the old mood's don't linger */
  ILI9341_DrawString(FEED_ME_X, FEED_ME_Y, "        ", BG_COLOR, BG_COLOR, 2);
  ILI9341_DrawString(SPARKLE_L_X, SPARKLE_L_Y, " ", BG_COLOR, BG_COLOR, 2);
  ILI9341_DrawString(SPARKLE_R_X, SPARKLE_R_Y, " ", BG_COLOR, BG_COLOR, 2);

  drawn_mood = mood;
  frame = 0;
  tear_y = TEAR_TOP_Y;
}

void MainScreen_Draw(void)
{
  ILI9341_FillScreen(BG_COLOR);
  ILI9341_DrawString(10, 10, "TEST PET", ILI9341_COLOR_WHITE, BG_COLOR, 2);
  ILI9341_DrawString(10, 35, "PRESS FOR MENU", ILI9341_COLOR_GRAY, BG_COLOR, 1);

  /* Ears and head; the face is drawn on top */
  ILI9341_FillRect(PET_HEAD_X + 10U, PET_HEAD_Y - 15U, 15U, 15U, PET_FACE_COLOR);
  ILI9341_FillRect(PET_HEAD_X + PET_HEAD_W - 25U, PET_HEAD_Y - 15U, 15U, 15U, PET_FACE_COLOR);
  ILI9341_FillRect(PET_HEAD_X, PET_HEAD_Y, PET_HEAD_W, PET_HEAD_H, PET_FACE_COLOR);
  TestPet_DrawFace(Pet_GetMood());

  TestPet_DrawMeters();
  last_frame = HAL_GetTick();
}

void MainScreen_Update(void)
{
  uint32_t  now = HAL_GetTick();
  PetMood_t mood;

#if TEST_PET_DRIFT_MS > 0
  /* TEMPORARY: hunger rises over time, and a hungry pet gets unhappy */
  if ((now - last_drift) >= TEST_PET_DRIFT_MS)
  {
    last_drift = now;
    if (g_pet.hunger < PET_STAT_MAX) { g_pet.hunger++; }
    if ((g_pet.hunger >= PET_HUNGRY_AT) && (g_pet.happiness > 0U)) { g_pet.happiness--; }
  }
#endif

  if ((now - last_frame) < FRAME_MS) { return; }   /* not time for the next frame yet */
  last_frame = now;

  /* Live updates when anything else changes the stats */
  if ((g_pet.hunger != drawn_hunger) || (g_pet.happiness != drawn_happiness))
  {
    TestPet_DrawMeters();
  }
  mood = Pet_GetMood();
  if (mood != drawn_mood)
  {
    TestPet_DrawFace(mood);   /* resets frame to 0 */
  }

  /* Blink - every mood */
  if ((frame % BLINK_PERIOD) == (BLINK_PERIOD - BLINK_CLOSED)) { TestPet_DrawEyes(0U); }
  else if ((frame % BLINK_PERIOD) == 0U)                       { TestPet_DrawEyes(1U); }

  switch (drawn_mood)
  {
    case PET_MOOD_HUNGRY:
      if ((frame % FEED_ME_TOGGLE) == 0U)
      {
        if (Pet_IsStarving())
        {
          /* Steady countdown to death - same 8-char width as "FEED ME!" */
          char text[12];   /* room for any uint8_t; the countdown is at most 2 digits */
          (void)snprintf(text, sizeof(text), "DYING %2u", Pet_StarveSecondsLeft());
          ILI9341_DrawString(FEED_ME_X, FEED_ME_Y, text, ILI9341_COLOR_RED, BG_COLOR, 2);
        }
        else
        {
          uint8_t show = ((frame / FEED_ME_TOGGLE) % 2U) == 0U;
          ILI9341_DrawString(FEED_ME_X, FEED_ME_Y, show ? "FEED ME!" : "        ", ILI9341_COLOR_RED, BG_COLOR, 2);
        }
      }
      break;

    case PET_MOOD_SAD:
      if ((frame % 2U) == 0U)
      {
        ILI9341_FillRect(TEAR_X, tear_y, TEAR_W, TEAR_H, PET_FACE_COLOR);   /* erase old position */
        tear_y = (uint16_t)(tear_y + 2U);
        if ((tear_y + TEAR_H) > TEAR_END_Y) { tear_y = TEAR_TOP_Y; }
        ILI9341_FillRect(TEAR_X, tear_y, TEAR_W, TEAR_H, ILI9341_COLOR_CYAN);
      }
      break;

    case PET_MOOD_HAPPY:
      if ((frame % SPARKLE_TOGGLE) == 0U)
      {
        uint8_t left = ((frame / SPARKLE_TOGGLE) % 2U) == 0U;
        ILI9341_DrawString(SPARKLE_L_X, SPARKLE_L_Y, left ? "*" : " ", ILI9341_COLOR_YELLOW, BG_COLOR, 2);
        ILI9341_DrawString(SPARKLE_R_X, SPARKLE_R_Y, left ? " " : "*", ILI9341_COLOR_YELLOW, BG_COLOR, 2);
      }
      break;

    default:
      break;
  }

  frame++;
}
