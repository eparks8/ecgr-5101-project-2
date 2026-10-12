/**
  ******************************************************************************
  * @file    death.c
  * @brief   Tombstone screen shown when the pet dies.
  *
  * A rounded headstone with a cross and "RIP" on a strip of grass, while a
  * small ghost floats up beside it. Restarting is handled by menu.c.
  ******************************************************************************
  */
#include "death.h"
#include "main.h"
#include "ili9341.h"

#define BG_COLOR        ILI9341_COLOR_BLACK
#define STONE_COLOR     ILI9341_COLOR_GRAY
#define CARVE_COLOR     ILI9341_COLOR_BLACK

/* Headstone: a rectangle with a half-circle on top */
#define STONE_R         50U
#define STONE_X         70U
#define STONE_W         (2U * STONE_R)
#define STONE_CX        (STONE_X + STONE_R)
#define STONE_BODY_Y    140U                  /* top of the rectangle = centre of the arc */
#define GROUND_Y        240U
#define GROUND_H        8U

/* Ghost floats up on the right, clear of the stone and the text */
#define GHOST_X         192U
#define GHOST_W         14U
#define GHOST_H         16U
#define GHOST_START_Y   (GROUND_Y - GHOST_H - 2U)
#define GHOST_END_Y     60U
#define GHOST_STEP_PX   2U
#define FRAME_MS        80U

static uint16_t ghost_y;
static uint32_t last_frame;

static void Death_DrawStone(void)
{
  uint16_t dy;
  uint16_t half = STONE_R;

  /* Rounded top, one horizontal line per row: half-width shrinks so that
   * half^2 + dy^2 <= r^2 (integer only, no sqrt) */
  for (dy = 0; dy <= STONE_R; dy++)
  {
    uint32_t limit = (uint32_t)(STONE_R * STONE_R) - ((uint32_t)dy * dy);

    while (((uint32_t)half * half) > limit) { half--; }
    if (half > 0U)
    {
      ILI9341_DrawHLine((uint16_t)(STONE_CX - half), (uint16_t)(STONE_BODY_Y - dy), (uint16_t)(2U * half), STONE_COLOR);
    }
  }
  ILI9341_FillRect(STONE_X, STONE_BODY_Y, STONE_W, GROUND_Y - STONE_BODY_Y, STONE_COLOR);

  /* Cross carved in the arc, text carved in the body */
  ILI9341_FillRect(STONE_CX - 3U, 100U, 6U, 32U, CARVE_COLOR);
  ILI9341_FillRect(STONE_CX - 13U, 108U, 26U, 6U, CARVE_COLOR);
  ILI9341_DrawString(STONE_CX - 27U, 152U, "RIP", CARVE_COLOR, STONE_COLOR, 3);
  ILI9341_DrawString(STONE_CX - 21U, 190U, "STARVED", CARVE_COLOR, STONE_COLOR, 1);
}

static void Death_DrawGhost(uint16_t y, uint8_t visible)
{
  if (!visible)
  {
    ILI9341_FillRect(GHOST_X, y, GHOST_W, GHOST_H, BG_COLOR);
    return;
  }
  ILI9341_FillRect(GHOST_X, y, GHOST_W, GHOST_H, ILI9341_COLOR_WHITE);
  ILI9341_FillRect(GHOST_X + 3U, y + 4U, 2U, 3U, BG_COLOR);             /* eyes */
  ILI9341_FillRect(GHOST_X + 9U, y + 4U, 2U, 3U, BG_COLOR);
  ILI9341_FillRect(GHOST_X + 3U, y + GHOST_H - 3U, 2U, 3U, BG_COLOR);   /* wavy hem */
  ILI9341_FillRect(GHOST_X + 9U, y + GHOST_H - 3U, 2U, 3U, BG_COLOR);
}

void DeathScreen_Draw(void)
{
  ILI9341_FillScreen(BG_COLOR);
  ILI9341_DrawString(42, 20, "YOUR PET DIED", ILI9341_COLOR_WHITE, BG_COLOR, 2);
  Death_DrawStone();
  ILI9341_FillRect(0, GROUND_Y, ILI9341_WIDTH, GROUND_H, ILI9341_COLOR_GREEN);
  ILI9341_DrawString(72, 290, "PRESS TO RESTART", ILI9341_COLOR_WHITE, BG_COLOR, 1);

  ghost_y = GHOST_START_Y;
  Death_DrawGhost(ghost_y, 1U);
  last_frame = HAL_GetTick();
}

void DeathScreen_Update(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - last_frame) < FRAME_MS) { return; }   /* not time for the next frame yet */
  last_frame = now;

  Death_DrawGhost(ghost_y, 0U);                     /* erase old position */
  ghost_y = (uint16_t)(ghost_y - GHOST_STEP_PX);
  if (ghost_y <= GHOST_END_Y) { ghost_y = GHOST_START_Y; }   /* float up, then start over */
  Death_DrawGhost(ghost_y, 1U);
}
