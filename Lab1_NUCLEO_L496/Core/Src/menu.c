/**
  ******************************************************************************
  * @file    menu.c
  * @brief   Screen state machine and joystick menu navigation.
  *
  *   MAIN --CENTER--> MENU --CENTER on an item--> ACTION --(done/cancel)--> MAIN
  *    ^                 |  UP/DOWN moves the cursor
  *    +------LEFT-------+
  *
  *   (any screen) --pet dies--> DEAD --CENTER--> MAIN with a new pet
  *
  * Every menu item is an action (feed, dance, fetch, ...) described by a row
  * in actions[] - to add one, write its five functions and add a row.
  ******************************************************************************
  */
#include <string.h>
#include "menu.h"
#include "input.h"
#include "feed.h"
#include "dance.h"
#include "fetch.h"
#include "death.h"
#include "pet.h"
#include "main.h"
#include "ili9341.h"

typedef enum { SCREEN_MAIN, SCREEN_MENU, SCREEN_ACTION, SCREEN_DEAD } Screen_t;

typedef struct
{
  const char *name;                       /* 5 chars so a redraw fully overwrites the previous text */
  void    (*start)(void);
  void    (*handle_input)(uint8_t pressed);
  void    (*update)(void);
  uint8_t (*is_active)(void);             /* 0 once finished or cancelled */
  void    (*abort)(void);                 /* stop immediately, e.g. the pet died */
} Action_t;

static const Action_t actions[] =
{
  { "FEED ", Feed_Start,  Feed_HandleInput,  Feed_Update,  Feed_IsActive,  Feed_Abort  },
  { "DANCE", Dance_Start, Dance_HandleInput, Dance_Update, Dance_IsActive, Dance_Abort },
  { "FETCH", Fetch_Start, Fetch_HandleInput, Fetch_Update, Fetch_IsActive, Fetch_Abort },
};
#define ACTION_COUNT ((uint8_t)(sizeof(actions) / sizeof(actions[0])))

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

/* No idle animation by default - a non-weak version overrides this */
__weak void MainScreen_Update(void)
{
}

void Menu_DrawCentered(uint16_t y, const char *text, uint16_t color, uint8_t size)
{
  uint16_t w = (uint16_t)(strlen(text) * 6U * size);   /* 5x7 font + 1 px spacing */

  ILI9341_DrawString((uint16_t)((ILI9341_WIDTH - w) / 2U), y, text, color, ILI9341_COLOR_BLACK, size);
}

static void Menu_DrawItem(uint8_t i)
{
  uint16_t y   = (uint16_t)(MENU_Y + (i * MENU_ROW_H));
  uint8_t  sel = (i == selected);

  ILI9341_DrawString(MENU_X - 20U, y, sel ? ">" : " ", ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(MENU_X, y, actions[i].name,
                     sel ? ILI9341_COLOR_BLACK : ILI9341_COLOR_WHITE,
                     sel ? ILI9341_COLOR_YELLOW : ILI9341_COLOR_BLACK, 2);
}

static void Menu_DrawAll(void)
{
  uint8_t i;

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(MENU_X, 20U, "MENU", ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 3);
  for (i = 0; i < ACTION_COUNT; i++)
  {
    Menu_DrawItem(i);
  }
  ILI9341_DrawString(10, 300, "UP/DN MOVE  PRESS OK  LEFT BACK", ILI9341_COLOR_GRAY, ILI9341_COLOR_BLACK, 1);
}

static void Menu_Move(int8_t delta)
{
  uint8_t old = selected;

  selected = (uint8_t)((selected + ACTION_COUNT + delta) % ACTION_COUNT);  /* wraps around */
  Menu_DrawItem(old);        /* only redraw the two rows that changed */
  Menu_DrawItem(selected);
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
      else if (pressed & JOY_CENTER_MASK) { screen = SCREEN_ACTION; actions[selected].start(); }
      break;

    case SCREEN_ACTION:
      actions[selected].handle_input(pressed);
      break;

    case SCREEN_DEAD:
      if (pressed & JOY_CENTER_MASK)
      {
        Pet_Reset();
        screen = SCREEN_MAIN;
        MainScreen_Draw();
      }
      break;
  }
}

void Menu_Update(void)
{
  /* Death interrupts whatever screen is showing */
  if (Pet_IsDead() && (screen != SCREEN_DEAD))
  {
    if (screen == SCREEN_ACTION) { actions[selected].abort(); }
    screen = SCREEN_DEAD;
    DeathScreen_Draw();
    return;
  }

  if (screen == SCREEN_DEAD)
  {
    DeathScreen_Update();
  }
  else if (screen == SCREEN_MAIN)
  {
    MainScreen_Update();
  }
  else if (screen == SCREEN_ACTION)
  {
    actions[selected].update();
    if (!actions[selected].is_active())
    {
      screen = SCREEN_MAIN;
      MainScreen_Draw();   /* meters redraw with the new hunger/happiness */
    }
  }
}
