/**
  ******************************************************************************
  * @file    runaway.c
  * @brief   Screen shown when the pet gets bored and runs away.
  *
  * An empty doghouse on a strip of grass, while a small pet hops off to the
  * right, over and over. Restarting is handled by menu.c.
  ******************************************************************************
  */
#include "runaway.h"
#include "main.h"
#include "pet.h"
#include "ili9341.h"

#define BG_COLOR        ILI9341_COLOR_BLACK
#define HOUSE_COLOR     ILI9341_COLOR_ORANGE
#define ROOF_COLOR      ILI9341_COLOR_RED

/* Doghouse: a box with a triangular roof and a dark doorway */
#define HOUSE_X         20U
#define HOUSE_W         90U
#define HOUSE_CX        (HOUSE_X + (HOUSE_W / 2U))
#define HOUSE_BODY_Y    170U
#define ROOF_H          45U
#define ROOF_OVERHANG   8U
#define DOOR_W          34U
#define DOOR_H          44U
#define GROUND_Y        240U
#define GROUND_H        8U

/* Little pet walking away along the grass */
#define WALKER_W        24U
#define WALKER_H        18U
#define WALKER_START_X  (HOUSE_X + HOUSE_W + 6U)
#define WALKER_END_X    (ILI9341_WIDTH - WALKER_W)
#define WALKER_Y        (GROUND_Y - WALKER_H - 4U)
#define WALKER_HOP_PX   4U
#define WALKER_STEP_PX  3U
#define FRAME_MS        80U

static uint16_t walker_x;
static uint8_t  hop;
static uint32_t last_frame;

static void Runaway_DrawHouse(void)
{
  uint16_t dy;
  uint16_t half_base = (HOUSE_W / 2U) + ROOF_OVERHANG;

  /* Roof: one horizontal line per row, widening from the peak down */
  for (dy = 0; dy < ROOF_H; dy++)
  {
    uint16_t half = (uint16_t)(((uint32_t)half_base * (dy + 1U)) / ROOF_H);
    ILI9341_DrawHLine((uint16_t)(HOUSE_CX - half), (uint16_t)(HOUSE_BODY_Y - ROOF_H + dy), (uint16_t)(2U * half), ROOF_COLOR);
  }
  ILI9341_FillRect(HOUSE_X, HOUSE_BODY_Y, HOUSE_W, GROUND_Y - HOUSE_BODY_Y, HOUSE_COLOR);
  ILI9341_FillRect(HOUSE_CX - (DOOR_W / 2U), GROUND_Y - DOOR_H, DOOR_W, DOOR_H, BG_COLOR);   /* nobody home */
}

static void Runaway_DrawWalker(uint16_t x, uint16_t y, uint8_t visible)
{
  if (!visible)
  {
    ILI9341_FillRect(x, y, WALKER_W, WALKER_H, BG_COLOR);
    return;
  }
  ILI9341_FillRect(x, y, WALKER_W, WALKER_H, PET_FACE_COLOR);
  ILI9341_FillRect(x + 13U, y + 4U, 3U, 3U, BG_COLOR);   /* eyes, looking away to the right */
  ILI9341_FillRect(x + 19U, y + 4U, 3U, 3U, BG_COLOR);
}

void RunawayScreen_Draw(void)
{
  ILI9341_FillScreen(BG_COLOR);
  ILI9341_DrawString(18, 20, "YOUR PET RAN AWAY", ILI9341_COLOR_WHITE, BG_COLOR, 2);
  ILI9341_DrawString(72, 50, "IT GOT TOO BORED", ILI9341_COLOR_GRAY, BG_COLOR, 1);
  Runaway_DrawHouse();
  ILI9341_FillRect(0, GROUND_Y, ILI9341_WIDTH, GROUND_H, ILI9341_COLOR_GREEN);
  ILI9341_DrawString(72, 290, "PRESS TO RESTART", ILI9341_COLOR_WHITE, BG_COLOR, 1);

  walker_x = WALKER_START_X;
  hop = 0U;
  Runaway_DrawWalker(walker_x, WALKER_Y, 1U);
  last_frame = HAL_GetTick();
}

void RunawayScreen_Update(void)
{
  uint32_t now = HAL_GetTick();
  uint16_t y = (uint16_t)(WALKER_Y - (hop ? WALKER_HOP_PX : 0U));

  if ((now - last_frame) < FRAME_MS) { return; }   /* not time for the next frame yet */
  last_frame = now;

  Runaway_DrawWalker(walker_x, y, 0U);              /* erase old position */
  walker_x = (uint16_t)(walker_x + WALKER_STEP_PX);
  if (walker_x >= WALKER_END_X) { walker_x = WALKER_START_X; }   /* walk off, then start over */
  hop = (uint8_t)!hop;
  y = (uint16_t)(WALKER_Y - (hop ? WALKER_HOP_PX : 0U));
  Runaway_DrawWalker(walker_x, y, 1U);
}
