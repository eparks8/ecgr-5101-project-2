/**
  ******************************************************************************
  * @file    feed.c
  * @brief   Feed action: pick a food, update the pet, play the eating animation.
  *
  *   CHOOSE --CENTER--> DROP --food reaches pet--> CHEW --3 chomps--> RESULT --1 s--> IDLE
  *     +--UP (cancel)----------------------------------------------------------------> IDLE
  *
  * Animation frames are paced with HAL_GetTick() so the main loop never blocks.
  ******************************************************************************
  */
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

#define FRAME_MS        40U    /* 25 fps */
#define CHEW_FRAMES     24U    /* mouth toggles every 4 frames -> 3 chomps */
#define RESULT_FRAMES   25U    /* ~1 s */

typedef enum { FEED_IDLE, FEED_CHOOSE, FEED_DROP, FEED_CHEW, FEED_RESULT } FeedState_t;

typedef struct
{
  const char *name;
  uint8_t     hunger_drop;
  uint8_t     happy_gain;
  uint16_t    color;
} Food_t;

/* Names are the same length so switching foods fully overwrites the old one */
static const Food_t foods[] =
{
  { "SNACK", 10U, 10U, ILI9341_COLOR_YELLOW },
  { "MEAL ", 35U,  5U, ILI9341_COLOR_RED    },
};
#define FOOD_COUNT (sizeof(foods) / sizeof(foods[0]))

static FeedState_t state = FEED_IDLE;
static uint8_t     food_idx;
static uint16_t    food_y;
static uint8_t     frame;
static uint32_t    last_frame;

static void Feed_DrawChoice(void)
{
  ILI9341_DrawString(40, 30, "<", ILI9341_COLOR_WHITE, BG_COLOR, 3);
  ILI9341_DrawString(70, 30, foods[food_idx].name, foods[food_idx].color, BG_COLOR, 3);
  ILI9341_DrawString(180, 30, ">", ILI9341_COLOR_WHITE, BG_COLOR, 3);
}

static void Feed_DrawMouth(uint8_t open)
{
  ILI9341_FillRect(PET_MOUTH_X, PET_MOUTH_Y, PET_MOUTH_W, PET_MOUTH_H, open ? PET_MOUTH_COLOR : PET_FACE_COLOR);
  if (!open)
  {
    ILI9341_FillRect(PET_MOUTH_X, PET_MOUTH_Y + (PET_MOUTH_H / 2U) - 1U, PET_MOUTH_W, 3U, PET_MOUTH_COLOR);
  }
}

/* The state change - returns 0 if the pet was too full to eat */
static uint8_t Feed_ApplyToPet(void)
{
  const Food_t *f = &foods[food_idx];

  if (g_pet.hunger == 0U)
  {
    /* Overfeeding makes the pet grumpy */
    g_pet.happiness = (g_pet.happiness > 10U) ? (uint8_t)(g_pet.happiness - 10U) : 0U;
    return 0U;
  }

  g_pet.hunger = (g_pet.hunger > f->hunger_drop) ? (uint8_t)(g_pet.hunger - f->hunger_drop) : 0U;
  g_pet.happiness = ((g_pet.happiness + f->happy_gain) > PET_STAT_MAX)
                    ? PET_STAT_MAX : (uint8_t)(g_pet.happiness + f->happy_gain);
  return 1U;
}

void Feed_Start(void)
{
  MainScreen_Draw();        /* the pet must be visible for the animation */
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, 60, BG_COLOR);
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
    ILI9341_FillRect(0, 0, ILI9341_WIDTH, 60, BG_COLOR);   /* clear the chooser */
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
      if ((frame % 4U) == 0U)
      {
        Feed_DrawMouth(((frame / 4U) % 2U) == 0U);
      }
      if (++frame >= CHEW_FRAMES)
      {
        if (Feed_ApplyToPet())
        {
          ILI9341_DrawString(84, 30, "YUM!", ILI9341_COLOR_GREEN, BG_COLOR, 3);
        }
        else
        {
          ILI9341_DrawString(39, 30, "TOO FULL!", ILI9341_COLOR_RED, BG_COLOR, 3);
        }
        frame = 0;
        state = FEED_RESULT;
      }
      break;

    case FEED_RESULT:
      if (++frame >= RESULT_FRAMES) { state = FEED_IDLE; }
      break;

    default:
      break;
  }
}

uint8_t Feed_IsActive(void)
{
  return (uint8_t)(state != FEED_IDLE);
}
