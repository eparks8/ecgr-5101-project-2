/**
  ******************************************************************************
  * @file    menu.c
  * @brief   Screen state machine and joystick menu navigation.
  *
  *   MAIN --CENTER--> MENU --CENTER on FEED--> FEED --(done/cancel)--> MAIN
  *    ^                 |  UP/DOWN moves the cursor
  *    +------LEFT-------+
  ******************************************************************************
  */
#include "menu.h"
#include "input.h"
#include "feed.h"
#include "main.h"
#include "ili9341.h"

typedef enum { SCREEN_MAIN, SCREEN_MENU, SCREEN_FEED } Screen_t;
typedef enum { ITEM_FEED, ITEM_PLAY, ITEM_STATS, ITEM_COUNT } MenuItem_t;

/* Same length on purpose so a redraw fully overwrites the previous text */
static const char *const item_names[ITEM_COUNT] = { "FEED ", "PLAY ", "STATS" };

#define MENU_X      40U
#define MENU_Y      70U
#define MENU_ROW_H  30U

static Screen_t screen = SCREEN_MAIN;
static uint8_t  selected;

/* Placeholder until the real main screen exists - a non-weak version overrides this */
__weak void MainScreen_Draw(void)
{
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(10, 10, "MAIN SCREEN", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(10, 40, "PRESS FOR MENU", ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
}

static void Menu_DrawItem(uint8_t i)
{
  uint16_t y   = (uint16_t)(MENU_Y + (i * MENU_ROW_H));
  uint8_t  sel = (i == selected);

  ILI9341_DrawString(MENU_X - 20U, y, sel ? ">" : " ", ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(MENU_X, y, item_names[i],
                     sel ? ILI9341_COLOR_BLACK : ILI9341_COLOR_WHITE,
                     sel ? ILI9341_COLOR_YELLOW : ILI9341_COLOR_BLACK, 2);
}

static void Menu_DrawAll(void)
{
  uint8_t i;

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(MENU_X, 20U, "MENU", ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 3);
  for (i = 0; i < ITEM_COUNT; i++)
  {
    Menu_DrawItem(i);
  }
  ILI9341_DrawString(10, 300, "UP/DN MOVE  PRESS OK  LEFT BACK", ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
}

static void Menu_Move(int8_t delta)
{
  uint8_t old = selected;

  selected = (uint8_t)((selected + ITEM_COUNT + delta) % ITEM_COUNT);  /* wraps around */
  Menu_DrawItem(old);        /* only redraw the two rows that changed */
  Menu_DrawItem(selected);
}

static void Menu_Select(void)
{
  switch ((MenuItem_t)selected)
  {
    case ITEM_FEED:
      screen = SCREEN_FEED;
      Feed_Start();
      break;
    default:
      /* PLAY / STATS belong to teammates - hook their Start functions in here */
      break;
  }
}

void Menu_Init(void)
{
  screen = SCREEN_MAIN;
  MainScreen_Draw();
}

void Menu_HandleInput(uint8_t pressed)
{
  if (pressed == 0U) { return; }

  switch (screen)
  {
    case SCREEN_MAIN:
      if (pressed & JOY_CENTER_MASK)
      {
        screen = SCREEN_MENU;
        selected = 0;
        Menu_DrawAll();
      }
      break;

    case SCREEN_MENU:
      if (pressed & JOY_UP_MASK)          { Menu_Move(-1); }
      else if (pressed & JOY_DOWN_MASK)   { Menu_Move(+1); }
      else if (pressed & JOY_LEFT_MASK)   { screen = SCREEN_MAIN; MainScreen_Draw(); }
      else if (pressed & JOY_CENTER_MASK) { Menu_Select(); }
      break;

    case SCREEN_FEED:
      Feed_HandleInput(pressed);
      break;
  }
}

void Menu_Update(void)
{
  if (screen == SCREEN_FEED)
  {
    Feed_Update();
    if (!Feed_IsActive())
    {
      screen = SCREEN_MAIN;
      MainScreen_Draw();   /* meters redraw with the new hunger/happiness */
    }
  }
}
