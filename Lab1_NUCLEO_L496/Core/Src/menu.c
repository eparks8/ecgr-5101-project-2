/**
  ******************************************************************************
  * @file    menu.c
  * @brief   Screen state machine and joystick menu navigation.
  *
  *   MAIN --CENTER--> MENU --CENTER on an item--> ACTION --(done/cancel)--> MAIN
  *    ^                 |  UP/DOWN move within a column, LEFT/RIGHT switch columns
  *    +--LEFT (from the left column)
  *
  *   The menu has two columns: the first MENU_LEFT_COUNT actions on the left
  *   (FEED, DANCE), the rest on the right (FETCH, MIC, SLEEP).
  *
  *   (any screen) --pet dies--> DEAD --CENTER--> MAIN with a new pet
  *
  *   Clap commands on MAIN or MENU: 1 clap = pet notices you, 2 = DANCE,
  *   3 = FETCH, 4 = FEED (food chooser), 5 = a random food. While an action
  *   runs, claps go to its on_clap (if it has one).
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
#include "mictest.h"
#include "sleep.h"
#include "play.h"
#include "mic.h"
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
  void    (*on_clap)(uint8_t claps);      /* optional: claps heard while running, NULL = ignore */
} Action_t;

/* Must match the order of actions[] - used by the clap commands */
enum { ACTION_FEED, ACTION_DANCE, ACTION_FETCH, ACTION_MIC, ACTION_SLEEP };

static const Action_t actions[] =
{
  { "FEED ", Feed_Start,    Feed_HandleInput,    Feed_Update,    Feed_IsActive,    Feed_Abort,    NULL           },
  { "DANCE", Dance_Start,   Dance_HandleInput,   Dance_Update,   Dance_IsActive,   Dance_Abort,   NULL           },
  { "FETCH", Fetch_Start,   Fetch_HandleInput,   Fetch_Update,   Fetch_IsActive,   Fetch_Abort,   NULL           },
  { "MIC  ", MicTest_Start, MicTest_HandleInput, MicTest_Update, MicTest_IsActive, MicTest_Abort, MicTest_OnClap },
  { "SLEEP", Sleep_Start,   Sleep_HandleInput,   Sleep_Update,   Sleep_IsActive,   Sleep_Abort,   Sleep_OnClap   },
  { "PLAY ", Play_Start,    Play_HandleInput,    Play_Update,    Play_IsActive,    Play_Abort,    NULL           },
};
#define ACTION_COUNT ((uint8_t)(sizeof(actions) / sizeof(actions[0])))

#define MENU_LEFT_COUNT 2U      /* actions[0..1] in the left column, the rest in the right */
#define MENU_LEFT_X     30U
#define MENU_RIGHT_X    150U
#define MENU_Y          70U
#define MENU_ROW_H      30U

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

/* One clap on the main screen - nothing by default, a non-weak version overrides this */
__weak void MainScreen_Notice(void)
{
}

void Menu_DrawCentered(uint16_t y, const char *text, uint16_t color, uint8_t size)
{
  uint16_t w = (uint16_t)(strlen(text) * 6U * size);   /* 5x7 font + 1 px spacing */

  ILI9341_DrawString((uint16_t)((ILI9341_WIDTH - w) / 2U), y, text, color, ILI9341_COLOR_BLACK, size);
}

/* Column layout: which column an item is in, and its row within that column */
static uint8_t Menu_IsRight(uint8_t i) { return (uint8_t)(i >= MENU_LEFT_COUNT); }
static uint8_t Menu_Row(uint8_t i)     { return Menu_IsRight(i) ? (uint8_t)(i - MENU_LEFT_COUNT) : i; }
static uint8_t Menu_ColumnStart(uint8_t right) { return right ? (uint8_t)MENU_LEFT_COUNT : 0U; }
static uint8_t Menu_ColumnSize(uint8_t right)  { return right ? (uint8_t)(ACTION_COUNT - MENU_LEFT_COUNT) : (uint8_t)MENU_LEFT_COUNT; }

static void Menu_DrawItem(uint8_t i)
{
  uint16_t x   = Menu_IsRight(i) ? MENU_RIGHT_X : MENU_LEFT_X;
  uint16_t y   = (uint16_t)(MENU_Y + (Menu_Row(i) * MENU_ROW_H));
  uint8_t  sel = (i == selected);

  ILI9341_DrawString(x - 20U, y, sel ? ">" : " ", ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
  ILI9341_DrawString(x, y, actions[i].name,
                     sel ? ILI9341_COLOR_BLACK : ILI9341_COLOR_WHITE,
                     sel ? ILI9341_COLOR_YELLOW : ILI9341_COLOR_BLACK, 2);
}

static void Menu_DrawAll(void)
{
  uint8_t i;

  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_DrawString(MENU_LEFT_X, 20U, "MENU", ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK, 3);
  for (i = 0; i < ACTION_COUNT; i++)
  {
    Menu_DrawItem(i);
  }
  Menu_DrawCentered(300, "ARROWS MOVE  PRESS OK  LEFT BACK", ILI9341_COLOR_GRAY, 1);
}

static void Menu_Select(uint8_t index)
{
  uint8_t old = selected;

  selected = index;
  Menu_DrawItem(old);        /* only redraw the two items that changed */
  Menu_DrawItem(selected);
}

/* UP/DOWN: move within the current column, wrapping around */
static void Menu_MoveVertical(int8_t delta)
{
  uint8_t right = Menu_IsRight(selected);
  uint8_t size  = Menu_ColumnSize(right);
  uint8_t row   = (uint8_t)((Menu_Row(selected) + size + delta) % size);

  Menu_Select((uint8_t)(Menu_ColumnStart(right) + row));
}

/* LEFT/RIGHT: jump to the same row in the other column (or its last row if shorter) */
static void Menu_SwitchColumn(uint8_t to_right)
{
  uint8_t row  = Menu_Row(selected);
  uint8_t size = Menu_ColumnSize(to_right);

  if (row >= size) { row = (uint8_t)(size - 1U); }
  Menu_Select((uint8_t)(Menu_ColumnStart(to_right) + row));
}

static void Menu_StartAction(uint8_t index)
{
  selected = index;
  screen = SCREEN_ACTION;
  actions[index].start();
}

/* Carry out a clap command */
static void Menu_OnClaps(uint8_t claps)
{
  switch (screen)
  {
    case SCREEN_MAIN:
    case SCREEN_MENU:
      if (claps == 1U)
      {
        if (screen == SCREEN_MAIN) { MainScreen_Notice(); }   /* in the menu, one clap does nothing */
      }
      else if (claps == 2U) { Menu_StartAction(ACTION_DANCE); }
      else if (claps == 3U) { Menu_StartAction(ACTION_FETCH); }
      else if (claps == 4U) { Menu_StartAction(ACTION_FEED); }
      else if (claps == 5U)
      {
        /* Random food: runs as the FEED action, just skipping the chooser */
        selected = ACTION_FEED;
        screen = SCREEN_ACTION;
        Feed_StartRandom();
      }
      /* 6 or more (applause) is ignored */
      break;

    case SCREEN_ACTION:
      if (actions[selected].on_clap != NULL) { actions[selected].on_clap(claps); }
      break;

    default:
      break;   /* tombstone: claps can't bring the pet back */
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
      if (pressed & JOY_UP_MASK)          { Menu_MoveVertical(-1); }
      else if (pressed & JOY_DOWN_MASK)   { Menu_MoveVertical(+1); }
      else if (pressed & JOY_RIGHT_MASK)  { if (!Menu_IsRight(selected)) { Menu_SwitchColumn(1U); } }
      else if (pressed & JOY_LEFT_MASK)
      {
        if (Menu_IsRight(selected)) { Menu_SwitchColumn(0U); }                 /* right column -> left column */
        else                        { screen = SCREEN_MAIN; MainScreen_Draw(); } /* left column -> back to the pet */
      }
      else if (pressed & JOY_CENTER_MASK) { Menu_StartAction(selected); }
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
  uint8_t claps;

  /* Death interrupts whatever screen is showing */
  if (Pet_IsDead() && (screen != SCREEN_DEAD))
  {
    if (screen == SCREEN_ACTION) { actions[selected].abort(); }
    screen = SCREEN_DEAD;
    DeathScreen_Draw();
    return;
  }

  claps = Mic_TakeClapCommand();
  if (claps != 0U) { Menu_OnClaps(claps); }

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
