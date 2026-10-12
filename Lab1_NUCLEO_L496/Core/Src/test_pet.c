/**
  ******************************************************************************
  * @file    test_pet.c
  * @brief   TEMPORARY test pet + meters for exercising the menu and feed action.
  *
  * Replaces the weak MainScreen_Draw() in menu.c. Delete this file (and its
  * line in CMakeLists.txt) once the real pet and main screen are in.
  ******************************************************************************
  */
#include <stdio.h>
#include "menu.h"
#include "pet.h"
#include "ili9341.h"

#define BG_COLOR    ILI9341_COLOR_BLACK
#define METER_X     90U
#define METER_W     122U   /* 1 px border on each side + 120 px fill */
#define METER_H     12U

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

void MainScreen_Draw(void)
{
  ILI9341_FillScreen(BG_COLOR);
  ILI9341_DrawString(10, 10, "TEST PET", ILI9341_COLOR_WHITE, BG_COLOR, 2);
  ILI9341_DrawString(10, 35, "PRESS FOR MENU", ILI9341_COLOR_GRAY, BG_COLOR, 1);

  /* Ears, head, eyes, closed mouth */
  ILI9341_FillRect(PET_HEAD_X + 10U, PET_HEAD_Y - 15U, 15U, 15U, PET_FACE_COLOR);
  ILI9341_FillRect(PET_HEAD_X + PET_HEAD_W - 25U, PET_HEAD_Y - 15U, 15U, 15U, PET_FACE_COLOR);
  ILI9341_FillRect(PET_HEAD_X, PET_HEAD_Y, PET_HEAD_W, PET_HEAD_H, PET_FACE_COLOR);
  ILI9341_FillRect(PET_HEAD_X + 25U, PET_HEAD_Y + 20U, 10U, 10U, ILI9341_COLOR_BLACK);
  ILI9341_FillRect(PET_HEAD_X + PET_HEAD_W - 35U, PET_HEAD_Y + 20U, 10U, 10U, ILI9341_COLOR_BLACK);
  ILI9341_FillRect(PET_MOUTH_X, PET_MOUTH_Y + (PET_MOUTH_H / 2U) - 1U, PET_MOUTH_W, 3U, PET_MOUTH_COLOR);

  TestPet_DrawMeter(255U, "HUNGER", g_pet.hunger, ILI9341_COLOR_ORANGE);
  TestPet_DrawMeter(280U, "HAPPY", g_pet.happiness, ILI9341_COLOR_YELLOW);
}
