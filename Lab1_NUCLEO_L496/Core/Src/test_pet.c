/**
  ******************************************************************************
  * @file    test_pet.c
  * @brief   Main screen: the pet, its hunger and happiness meters, and its
  *          idle animations (MainScreen_* in menu.h).
  *
  * The face follows Pet_GetMood(), and each mood has a small idle animation:
  *   HUNGRY  - worried brows, open mouth, blinking "FEED ME!"
  *             (a "DYING NN" countdown once hunger hits max)
  *   SAD     - worried brows, frown, a tear rolling down the cheek
  *   HAPPY   - smile, sparkles twinkling beside the head
  *   NEUTRAL - flat mouth
  * Every mood blinks. Only the parts that change are redrawn.
  *
  * Also implements PetArt_Move() (pet_art.h) so actions can move the pet.
  ******************************************************************************
  */
#include <stdio.h>
#include "main.h"
#include "menu.h"
#include "pet.h"
#include "pet_art.h"
#include "ili9341.h"

#define BG_COLOR    ILI9341_COLOR_BLACK
#define METER_X     90U
#define METER_W     122U   /* 1 px border on each side + 120 px fill */
#define METER_H     12U
#define HUNGER_Y    255U
#define HAPPY_Y     280U

/* Where the pet currently is, relative to the shared layout in pet.h.
 * (ox, oy) is the offset set by PetArt_Move(); 0,0 on the main screen. */
static int16_t ox;
static int16_t oy;

#define HEAD_X      ((uint16_t)((int16_t)PET_HEAD_X + ox))
#define HEAD_Y      ((uint16_t)((int16_t)PET_HEAD_Y + oy))
#define MOUTH_X     ((uint16_t)((int16_t)PET_MOUTH_X + ox))
#define MOUTH_Y     ((uint16_t)((int16_t)PET_MOUTH_Y + oy))

/* Features, relative to the head */
#define EAR_SIZE    15U
#define EAR_L_X     (HEAD_X + 10U)
#define EAR_R_X     (HEAD_X + PET_HEAD_W - 25U)
#define EAR_Y       (HEAD_Y - EAR_SIZE)
#define EYE_SIZE    10U
#define EYE_L_X     (HEAD_X + 25U)
#define EYE_R_X     (HEAD_X + PET_HEAD_W - 35U)
#define EYE_Y       (HEAD_Y + 20U)
#define FACE_X      (HEAD_X + 10U)   /* area cleared when the expression changes */
#define FACE_Y      (HEAD_Y + 8U)
#define FACE_W      (PET_HEAD_W - 20U)
#define FACE_H      (PET_HEAD_H - 12U)
#define TEAR_X      (EYE_L_X + 2U)
#define TEAR_TOP_Y  (EYE_Y + EYE_SIZE + 2U)
#define TEAR_END_Y  (MOUTH_Y - 4U)
#define TEAR_W      4U
#define TEAR_H      6U

/* Main-screen extras around the pet - only drawn at the home position */
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
#define NOTICE_MS           1200U   /* how long the "HI!" reaction to a clap lasts */
#define NOTICE_HAPPY        2       /* attention makes the pet a little happier */

static PetMood_t drawn_mood;
static uint8_t   drawn_hunger;
static uint8_t   drawn_happiness;
static uint32_t  frame;
static uint32_t  last_frame;
static uint16_t  tear_y;
static uint8_t   noticing;
static uint32_t  notice_start;

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
  const int16_t mx = (int16_t)MOUTH_X;
  const int16_t my = (int16_t)MOUTH_Y;
  const int16_t mw = (int16_t)PET_MOUTH_W;
  const int16_t mh = (int16_t)PET_MOUTH_H;

  ILI9341_FillRect(FACE_X, FACE_Y, FACE_W, FACE_H, PET_FACE_COLOR);
  TestPet_DrawEyes((uint8_t)(mood != PET_MOOD_ASLEEP));

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
      ILI9341_FillRect(MOUTH_X + 8U, MOUTH_Y, PET_MOUTH_W - 16U, PET_MOUTH_H, PET_MOUTH_COLOR);
      break;

    case PET_MOOD_ASLEEP:   /* eyes closed (above), small round snoring mouth */
      ILI9341_FillRect(MOUTH_X + 11U, MOUTH_Y + 3U, PET_MOUTH_W - 22U, PET_MOUTH_H - 6U, PET_MOUTH_COLOR);
      break;

    default:                /* neutral: flat mouth */
      ILI9341_FillRect(MOUTH_X, MOUTH_Y + (PET_MOUTH_H / 2U) - 1U, PET_MOUTH_W, 3U, PET_MOUTH_COLOR);
      break;
  }

  drawn_mood = mood;
  frame = 0;
  tear_y = TEAR_TOP_Y;
}

/* Ears and head at the current offset; the face is drawn on top */
static void TestPet_DrawPet(PetMood_t mood)
{
  ILI9341_FillRect(EAR_L_X, EAR_Y, EAR_SIZE, EAR_SIZE, PET_FACE_COLOR);
  ILI9341_FillRect(EAR_R_X, EAR_Y, EAR_SIZE, EAR_SIZE, PET_FACE_COLOR);
  ILI9341_FillRect(HEAD_X, HEAD_Y, PET_HEAD_W, PET_HEAD_H, PET_FACE_COLOR);
  TestPet_DrawFace(mood);
}

static int16_t TestPet_Limit(int16_t value, int16_t lo, int16_t hi)
{
  if (value < lo) { return lo; }
  if (value > hi) { return hi; }
  return value;
}

void PetArt_Move(int16_t dx, int16_t dy, PetMood_t face)
{
  int16_t  old_x = (int16_t)HEAD_X;
  int16_t  old_y = (int16_t)HEAD_Y;
  int16_t  step_x;
  int16_t  step_y;
  uint16_t strip;

  dx = TestPet_Limit(dx, PET_ART_MIN_DX, PET_ART_MAX_DX);
  dy = TestPet_Limit(dy, PET_ART_MIN_DY, PET_ART_MAX_DY);
  step_x = (int16_t)(dx - ox);
  step_y = (int16_t)(dy - oy);

  if ((step_x == 0) && (step_y == 0))
  {
    TestPet_DrawFace(face);   /* same place - only the expression changes */
    return;
  }

  /* Erase what the pet at its new position won't cover: the old ears, then
   * the strips of the old head it moved off. Done before drawing so the
   * erase can never clip the newly drawn pet. */
  ILI9341_FillRect(EAR_L_X, EAR_Y, EAR_SIZE, EAR_SIZE, BG_COLOR);
  ILI9341_FillRect(EAR_R_X, EAR_Y, EAR_SIZE, EAR_SIZE, BG_COLOR);
  if (step_x > 0)
  {
    strip = (uint16_t)((step_x < (int16_t)PET_HEAD_W) ? step_x : (int16_t)PET_HEAD_W);
    ILI9341_FillRect((uint16_t)old_x, (uint16_t)old_y, strip, PET_HEAD_H, BG_COLOR);
  }
  else if (step_x < 0)
  {
    strip = (uint16_t)((-step_x < (int16_t)PET_HEAD_W) ? -step_x : (int16_t)PET_HEAD_W);
    ILI9341_FillRect((uint16_t)(old_x + (int16_t)PET_HEAD_W - (int16_t)strip), (uint16_t)old_y, strip, PET_HEAD_H, BG_COLOR);
  }
  if (step_y > 0)
  {
    strip = (uint16_t)((step_y < (int16_t)PET_HEAD_H) ? step_y : (int16_t)PET_HEAD_H);
    ILI9341_FillRect((uint16_t)old_x, (uint16_t)old_y, PET_HEAD_W, strip, BG_COLOR);
  }
  else if (step_y < 0)
  {
    strip = (uint16_t)((-step_y < (int16_t)PET_HEAD_H) ? -step_y : (int16_t)PET_HEAD_H);
    ILI9341_FillRect((uint16_t)old_x, (uint16_t)(old_y + (int16_t)PET_HEAD_H - (int16_t)strip), PET_HEAD_W, strip, BG_COLOR);
  }

  ox = dx;
  oy = dy;
  TestPet_DrawPet(face);
}

void MainScreen_Draw(void)
{
  ILI9341_FillScreen(BG_COLOR);
  ILI9341_DrawString(10, 10, "VIRTUAL PET", ILI9341_COLOR_WHITE, BG_COLOR, 2);
  ILI9341_DrawString(10, 35, "PRESS FOR MENU", ILI9341_COLOR_GRAY, BG_COLOR, 1);

  ox = 0;   /* back home */
  oy = 0;
  noticing = 0U;
  TestPet_DrawPet(Pet_GetMood());

  TestPet_DrawMeters();
  last_frame = HAL_GetTick();
}

void MainScreen_Update(void)
{
  uint32_t  now = HAL_GetTick();
  PetMood_t mood;

  if ((now - last_frame) < FRAME_MS) { return; }   /* not time for the next frame yet */
  last_frame = now;

  /* Live updates when anything else changes the stats */
  if ((g_pet.hunger != drawn_hunger) || (g_pet.happiness != drawn_happiness))
  {
    TestPet_DrawMeters();
  }

  /* Hold the "HI!" reaction to a clap, then go back to the normal face */
  if (noticing)
  {
    if ((now - notice_start) < NOTICE_MS) { return; }
    noticing = 0U;
    ILI9341_DrawString(FEED_ME_X, FEED_ME_Y, "        ", BG_COLOR, BG_COLOR, 2);
    TestPet_DrawFace(Pet_GetMood());
  }
  mood = Pet_GetMood();
  if (mood != drawn_mood)
  {
    /* Clear the old mood's extras around the head so they don't linger */
    ILI9341_DrawString(FEED_ME_X, FEED_ME_Y, "        ", BG_COLOR, BG_COLOR, 2);
    ILI9341_DrawString(SPARKLE_L_X, SPARKLE_L_Y, " ", BG_COLOR, BG_COLOR, 2);
    ILI9341_DrawString(SPARKLE_R_X, SPARKLE_R_Y, " ", BG_COLOR, BG_COLOR, 2);
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
      if ((frame % FEED_ME_TOGGLE) == 0U)
      {
        /* Steady countdown to running away - same 8-char width as "FEED ME!";
         * blanked again once the pet cheers up above 0 */
        char text[12] = "        ";
        if (Pet_IsBored()) { (void)snprintf(text, sizeof(text), "BYE IN%2u", Pet_BoredSecondsLeft()); }
        ILI9341_DrawString(FEED_ME_X, FEED_ME_Y, text, ILI9341_COLOR_CYAN, BG_COLOR, 2);
      }
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

/* One clap: the pet perks up and says hi */
void MainScreen_Notice(void)
{
  /* Attention alone can't stop a bored pet running away - it needs real play */
  if (g_pet.happiness > 0U) { Pet_AddHappiness(NOTICE_HAPPY); }
  ILI9341_DrawString(SPARKLE_L_X, SPARKLE_L_Y, " ", BG_COLOR, BG_COLOR, 2);
  ILI9341_DrawString(SPARKLE_R_X, SPARKLE_R_Y, " ", BG_COLOR, BG_COLOR, 2);
  ILI9341_DrawString(FEED_ME_X, FEED_ME_Y, "  HI!   ", ILI9341_COLOR_CYAN, BG_COLOR, 2);
  TestPet_DrawFace(PET_MOOD_HAPPY);
  noticing = 1U;
  notice_start = HAL_GetTick();
}
