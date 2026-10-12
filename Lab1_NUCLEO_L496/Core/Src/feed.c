/**
  ******************************************************************************
  * @file    feed.c
  * @brief   Feed action: pick a food, update the pet, play the eating animation.
  *
  *   CHOOSE --CENTER--> DROP --food reaches pet--> CHEW --N chomps--> REACT --~1.6 s--> IDLE
  *     +--UP (cancel)--------------------------------------------------------------> IDLE
  *
  * Each food has its own effect on the pet and its own reaction animation.
  * Animation frames are paced with HAL_GetTick() so the main loop never blocks.
  ******************************************************************************
  */
#include <stdio.h>
#include "feed.h"
#include "input.h"
#include "menu.h"
#include "pet.h"
#include "main.h"
#include "ili9341.h"

#define BG_COLOR        ILI9341_COLOR_BLACK

#define FOOD_SIZE       12U
#define FOOD_X          ((ILI9341_WIDTH - FOOD_SIZE) / 2U)
#define FOOD_START_Y    70U
#define FOOD_END_Y      (PET_HEAD_Y - FOOD_SIZE)   /* stop above the head so erasing never cuts into the pet */
#define DROP_STEP_PX    5U

#define CHOOSER_H       62U    /* top strip used by the chooser and the reaction message */
#define NAME_Y          20U
#define PREVIEW_Y       50U
#define MESSAGE_Y       22U

#define FRAME_MS        40U    /* 25 fps */
#define CHOMP_FRAMES    8U     /* mouth open 4 frames, closed 4 frames */
#define REACT_FRAMES    40U    /* ~1.6 s */
#define OVERFEED_HAPPY  10U

/* Reaction features, relative to the shared head/mouth layout in pet.h */
#define CHEEK_W         12U
#define CHEEK_H         8U
#define CHEEK_L_X       (PET_HEAD_X + 8U)
#define CHEEK_R_X       (PET_HEAD_X + PET_HEAD_W - 8U - CHEEK_W)
#define CHEEK_Y         (PET_MOUTH_Y - 6U)
#define TONGUE_W        10U
#define TONGUE_H        8U
#define TONGUE_X        (PET_MOUTH_X + ((PET_MOUTH_W - TONGUE_W) / 2U))
#define TONGUE_Y        (PET_MOUTH_Y + PET_MOUTH_H - 2U)
#define SPARKLE_L_X     (PET_HEAD_X - 20U)
#define SPARKLE_R_X     (PET_HEAD_X + PET_HEAD_W + 8U)
#define SPARKLE_Y       (PET_HEAD_Y + 20U)

typedef enum { FEED_IDLE, FEED_CHOOSE, FEED_DROP, FEED_CHEW, FEED_REACT } FeedState_t;

typedef enum
{
  REACT_YUM,        /* licks lips */
  REACT_LOVE,       /* pink cheeks + sparkles */
  REACT_BURP,       /* mouth wide open, then closes */
  REACT_SPICY,      /* cheeks flash red */
  REACT_YUCK,       /* tongue out */
  REACT_TOO_FULL    /* any food when hunger is already 0 */
} Reaction_t;

typedef struct
{
  const char *name;          /* 8 chars, centred, so switching foods fully overwrites the old name */
  uint8_t     hunger_drop;
  int8_t      happy_change;
  uint16_t    color;
  uint8_t     chomps;
  Reaction_t  reaction;
  const char *message;
  uint16_t    message_color;
} Food_t;

static const Food_t foods[] =
{
  { " APPLE  ", 15U,   5, ILI9341_COLOR_RED,     3U, REACT_YUM,   "YUM!",      ILI9341_COLOR_GREEN   },
  { "  CAKE  ", 10U,  20, ILI9341_COLOR_MAGENTA, 3U, REACT_LOVE,  "SO SWEET!", ILI9341_COLOR_MAGENTA },
  { "  MEAL  ", 35U,   5, ILI9341_COLOR_YELLOW,  5U, REACT_BURP,  "BURP!",     ILI9341_COLOR_YELLOW  },
  { " PEPPER ", 10U, -10, ILI9341_COLOR_ORANGE,  2U, REACT_SPICY, "SPICY!",    ILI9341_COLOR_RED     },
  { "BROCCOLI", 20U,  -5, ILI9341_COLOR_GREEN,   4U, REACT_YUCK,  "YUCK!",     ILI9341_COLOR_GREEN   },
};
#define FOOD_COUNT (sizeof(foods) / sizeof(foods[0]))

static FeedState_t state = FEED_IDLE;
static uint8_t     food_idx;
static uint16_t    food_y;
static uint8_t     frame;
static uint32_t    last_frame;
static Reaction_t  reaction;

static void Feed_DrawChoice(void)
{
  const Food_t *f = &foods[food_idx];
  char preview[24];

  ILI9341_DrawString(10, NAME_Y, "<", ILI9341_COLOR_WHITE, BG_COLOR, 3);
  ILI9341_DrawString(48, NAME_Y, f->name, f->color, BG_COLOR, 3);
  ILI9341_DrawString(212, NAME_Y, ">", ILI9341_COLOR_WHITE, BG_COLOR, 3);

  /* Constant-width text (21 chars) so it always overwrites the previous food's preview */
  (void)snprintf(preview, sizeof(preview), "HUNGER -%2u  HAPPY %+3d", f->hunger_drop, f->happy_change);
  ILI9341_DrawString(57, PREVIEW_Y, preview, ILI9341_COLOR_GRAY, BG_COLOR, 1);
}

static void Feed_DrawMouth(uint8_t open)
{
  ILI9341_FillRect(PET_MOUTH_X, PET_MOUTH_Y, PET_MOUTH_W, PET_MOUTH_H, open ? PET_MOUTH_COLOR : PET_FACE_COLOR);
  if (!open)
  {
    ILI9341_FillRect(PET_MOUTH_X, PET_MOUTH_Y + (PET_MOUTH_H / 2U) - 1U, PET_MOUTH_W, 3U, PET_MOUTH_COLOR);
  }
}

static void Feed_DrawCheeks(uint16_t color)
{
  ILI9341_FillRect(CHEEK_L_X, CHEEK_Y, CHEEK_W, CHEEK_H, color);
  ILI9341_FillRect(CHEEK_R_X, CHEEK_Y, CHEEK_W, CHEEK_H, color);
}

static void Feed_DrawTongue(uint8_t out)
{
  ILI9341_FillRect(TONGUE_X, TONGUE_Y, TONGUE_W, TONGUE_H, out ? ILI9341_COLOR_MAGENTA : PET_FACE_COLOR);
}

static void Feed_DrawMessage(const char *msg, uint16_t color)
{
  Menu_DrawCentered(MESSAGE_Y, msg, color, 3);
}

/* The state change - returns the reaction to play */
static Reaction_t Feed_ApplyToPet(void)
{
  const Food_t *f = &foods[food_idx];

  if (g_pet.hunger == 0U)
  {
    /* Overfeeding makes the pet grumpy, whatever the food */
    Pet_AddHappiness(-(int16_t)OVERFEED_HAPPY);
    return REACT_TOO_FULL;
  }

  Pet_AddHunger(-(int16_t)f->hunger_drop);
  Pet_AddHappiness(f->happy_change);
  return f->reaction;
}

/* One frame of the reaction animation; frame 0 draws the message */
static void Feed_DrawReaction(void)
{
  if (frame == 0U)
  {
    if (reaction == REACT_TOO_FULL) { Feed_DrawMessage("TOO FULL!", ILI9341_COLOR_RED); }
    else                            { Feed_DrawMessage(foods[food_idx].message, foods[food_idx].message_color); }
  }

  switch (reaction)
  {
    case REACT_YUM:        /* lick: tongue pops out twice */
      if ((frame % 10U) == 0U) { Feed_DrawTongue(((frame / 10U) % 2U) == 0U); }
      break;

    case REACT_LOVE:
      if (frame == 0U) { Feed_DrawCheeks(ILI9341_COLOR_MAGENTA); }
      if ((frame % 5U) == 0U)
      {
        uint8_t left = ((frame / 5U) % 2U) == 0U;
        ILI9341_DrawString(SPARKLE_L_X, SPARKLE_Y, left ? "*" : " ", ILI9341_COLOR_MAGENTA, BG_COLOR, 2);
        ILI9341_DrawString(SPARKLE_R_X, SPARKLE_Y, left ? " " : "*", ILI9341_COLOR_MAGENTA, BG_COLOR, 2);
      }
      break;

    case REACT_BURP:
      if (frame == 0U)  { Feed_DrawMouth(1U); }
      if (frame == 20U) { Feed_DrawMouth(0U); }
      break;

    case REACT_SPICY:
      if ((frame % 4U) == 0U)
      {
        Feed_DrawCheeks((((frame / 4U) % 2U) == 0U) ? ILI9341_COLOR_RED : PET_FACE_COLOR);
      }
      break;

    case REACT_YUCK:
      if (frame == 0U) { Feed_DrawTongue(1U); }
      break;

    case REACT_TOO_FULL:
    default:
      break;
  }
}

void Feed_Start(void)
{
  MainScreen_Draw();        /* the pet must be visible for the animation */
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, CHOOSER_H, BG_COLOR);
  ILI9341_DrawString(10, 5, "L/R PICK  OK FEED  UP CANCEL", ILI9341_COLOR_GRAY, BG_COLOR, 1);
  food_idx = 0;
  state = FEED_CHOOSE;
  Feed_DrawChoice();
}

void Feed_HandleInput(uint8_t pressed)
{
  if (state != FEED_CHOOSE) { return; }   /* ignore buttons mid-animation */

  if (pressed & JOY_LEFT_MASK)
  {
    food_idx = (uint8_t)((food_idx + FOOD_COUNT - 1U) % FOOD_COUNT);
    Feed_DrawChoice();
  }
  else if (pressed & JOY_RIGHT_MASK)
  {
    food_idx = (uint8_t)((food_idx + 1U) % FOOD_COUNT);
    Feed_DrawChoice();
  }
  else if (pressed & JOY_UP_MASK)
  {
    state = FEED_IDLE;   /* cancel - Menu_Update sees this and returns to main */
  }
  else if (pressed & JOY_CENTER_MASK)
  {
    ILI9341_FillRect(0, 0, ILI9341_WIDTH, CHOOSER_H, BG_COLOR);   /* clear the chooser */
    food_y = FOOD_START_Y;
    last_frame = HAL_GetTick();
    state = FEED_DROP;
  }
}

void Feed_Update(void)
{
  uint32_t now = HAL_GetTick();

  if ((state == FEED_IDLE) || (state == FEED_CHOOSE)) { return; }
  if ((now - last_frame) < FRAME_MS) { return; }   /* not time for the next frame yet */
  last_frame = now;

  switch (state)
  {
    case FEED_DROP:
      ILI9341_FillRect(FOOD_X, food_y, FOOD_SIZE, FOOD_SIZE, BG_COLOR);   /* erase old position */
      food_y += DROP_STEP_PX;
      if (food_y >= FOOD_END_Y)
      {
        frame = 0;
        state = FEED_CHEW;   /* food "disappears" into the pet */
      }
      else
      {
        ILI9341_FillRect(FOOD_X, food_y, FOOD_SIZE, FOOD_SIZE, foods[food_idx].color);
      }
      break;

    case FEED_CHEW:
      if ((frame % (CHOMP_FRAMES / 2U)) == 0U)
      {
        Feed_DrawMouth(((frame / (CHOMP_FRAMES / 2U)) % 2U) == 0U);
      }
      if (++frame >= (foods[food_idx].chomps * CHOMP_FRAMES))
      {
        reaction = Feed_ApplyToPet();
        frame = 0;
        state = FEED_REACT;
      }
      break;

    case FEED_REACT:
      Feed_DrawReaction();
      if (++frame >= REACT_FRAMES) { state = FEED_IDLE; }
      break;

    default:
      break;
  }
}

void Feed_Abort(void)
{
  state = FEED_IDLE;   /* drop any animation in progress; the caller redraws the screen */
}

uint8_t Feed_IsActive(void)
{
  return (uint8_t)(state != FEED_IDLE);
}
