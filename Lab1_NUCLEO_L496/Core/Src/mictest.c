/**
  ******************************************************************************
  * @file    mictest.c
  * @brief   MIC menu item: live microphone readings for checking the wiring
  *          and tuning the loudness thresholds.
  *
  * Shows the raw ADC value and the interpreted values (average, level, peak,
  * QUIET / TALKING / LOUD), a level bar, a 10 s level history, and a one-line
  * wiring diagnosis based on the average. LEFT exits, CENTER resets the peak.
  *
  * PEAK is the loudest ~24 ms burst - the same number clap.c compares with
  * CLAP_LEVEL - and CLAPS shows each clap command heard, without running it.
  ******************************************************************************
  */
#include <stdio.h>
#include "mictest.h"
#include "mic.h"
#include "input.h"
#include "menu.h"
#include "main.h"
#include "ili9341.h"

#define BG_COLOR        ILI9341_COLOR_BLACK
#define REFRESH_MS      100U

extern uint8_t mic_block_log_hold;   /* mic.c tuning log: DOWN freezes it for reading in the debugger */

#define STATUS_Y        58U
#define VALUE_X         150U
#define RAW_Y           74U
#define AVG_Y           94U
#define LEVEL_Y         114U
#define PEAK_Y          134U
#define CLAPS_Y         154U
#define WORD_Y          178U

#define BAR_X           20U
#define BAR_Y           208U
#define BAR_W           200U
#define BAR_H           14U
#define FULL_SCALE      2048U   /* level that fills the bar / graph */

#define GRAPH_X         20U
#define GRAPH_Y         236U
#define GRAPH_W         200U
#define GRAPH_H         64U
#define GRAPH_COL_W     2U      /* 100 columns x 100 ms = 10 s of history */

/* Average outside this range means the mic isn't sitting at half supply */
#define AVG_NO_POWER    300U
#define AVG_STUCK_HIGH  3800U

static uint8_t  active;
static uint8_t  shown_status;   /* which status line is on screen, so it's only redrawn on change */
static uint16_t peak;
static uint8_t  claps_heard;
static uint8_t  claps_shown;
static uint16_t graph_col;
static uint32_t last_refresh;

static uint16_t MicTest_Color(MicLoudness_t loudness)
{
  if (loudness == MIC_LOUD)    { return ILI9341_COLOR_RED; }
  if (loudness == MIC_TALKING) { return ILI9341_COLOR_YELLOW; }
  return ILI9341_COLOR_GREEN;
}

static void MicTest_DrawValue(uint16_t y, uint16_t value)
{
  char text[8];

  (void)snprintf(text, sizeof(text), "%4u", value);
  ILI9341_DrawString(VALUE_X, y, text, ILI9341_COLOR_WHITE, BG_COLOR, 2);
}

/* One-line wiring check, padded to a constant width so it fully overwrites.
 * Text is slow to draw on this display, so it's only redrawn when it changes. */
static void MicTest_DrawStatus(uint16_t avg)
{
  const char *msg;
  uint16_t    color;
  uint8_t     status;
  char        text[40];

  if (!Mic_IsReady())             { status = 0U; msg = "ADC FAILED TO START";              color = ILI9341_COLOR_RED; }
  else if (avg < AVG_NO_POWER)    { status = 1U; msg = "NO SIGNAL: CHECK VCC WIRE (PF15)"; color = ILI9341_COLOR_RED; }
  else if (avg > AVG_STUCK_HIGH)  { status = 2U; msg = "STUCK HIGH: CHECK OUT WIRE (PF3)"; color = ILI9341_COLOR_RED; }
  else                            { status = 3U; msg = "WIRING LOOKS GOOD";                color = ILI9341_COLOR_GREEN; }

  if (status == shown_status) { return; }
  shown_status = status;
  (void)snprintf(text, sizeof(text), "%-32s", msg);
  Menu_DrawCentered(STATUS_Y, text, color, 1);
}

static void MicTest_DrawFrame(void)
{
  ILI9341_FillScreen(BG_COLOR);
  Menu_DrawCentered(10, "MIC TEST", ILI9341_COLOR_CYAN, 3);
  Menu_DrawCentered(38, "LEFT EXIT  PRESS PEAK  DOWN HOLD", ILI9341_COLOR_GRAY, 1);

  ILI9341_DrawString(10, RAW_Y,   "RAW",     ILI9341_COLOR_GRAY, BG_COLOR, 2);
  ILI9341_DrawString(10, AVG_Y,   "AVERAGE", ILI9341_COLOR_GRAY, BG_COLOR, 2);
  ILI9341_DrawString(10, LEVEL_Y, "LEVEL",   ILI9341_COLOR_GRAY, BG_COLOR, 2);
  ILI9341_DrawString(10, PEAK_Y,  "PEAK",    ILI9341_COLOR_GRAY, BG_COLOR, 2);
  ILI9341_DrawString(10, CLAPS_Y, "CLAPS",   ILI9341_COLOR_GRAY, BG_COLOR, 2);

  ILI9341_DrawRect(BAR_X - 1U, BAR_Y - 1U, BAR_W + 2U, BAR_H + 2U, ILI9341_COLOR_WHITE);
  ILI9341_DrawRect(GRAPH_X - 1U, GRAPH_Y - 1U, GRAPH_W + 2U, GRAPH_H + 2U, ILI9341_COLOR_GRAY);
}

static uint16_t MicTest_Scale(uint16_t value, uint16_t full)
{
  if (value >= FULL_SCALE) { return full; }
  return (uint16_t)(((uint32_t)value * full) / FULL_SCALE);
}

static void MicTest_Refresh(void)
{
  static const char *const words[] = { " QUIET ", "TALKING", " LOUD! " };
  uint16_t      level = Mic_GetLevel();
  uint16_t      avg = Mic_GetAverage();
  MicLoudness_t loudness = Mic_GetLoudness();
  uint16_t      color = MicTest_Color(loudness);
  uint16_t      fill;
  uint16_t      x;
  uint16_t      h;

  uint16_t      burst = Mic_TakeBurstPeak();

  if (burst > peak) { peak = burst; }

  MicTest_DrawStatus(avg);
  MicTest_DrawValue(RAW_Y, Mic_GetRaw());
  MicTest_DrawValue(AVG_Y, avg);
  MicTest_DrawValue(LEVEL_Y, level);
  MicTest_DrawValue(PEAK_Y, peak);
  if (claps_heard != claps_shown)
  {
    claps_shown = claps_heard;
    MicTest_DrawValue(CLAPS_Y, claps_heard);
  }
  Menu_DrawCentered(WORD_Y, words[loudness], color, 3);

  /* Level bar */
  fill = MicTest_Scale(level, BAR_W);
  ILI9341_FillRect(BAR_X, BAR_Y, fill, BAR_H, color);
  ILI9341_FillRect(BAR_X + fill, BAR_Y, (uint16_t)(BAR_W - fill), BAR_H, BG_COLOR);

  /* History graph: one column per refresh, with a gray cursor in front of it */
  x = (uint16_t)(GRAPH_X + (graph_col * GRAPH_COL_W));
  h = MicTest_Scale(level, GRAPH_H);
  ILI9341_FillRect(x, GRAPH_Y, GRAPH_COL_W, GRAPH_H, BG_COLOR);
  if (h > 0U) { ILI9341_FillRect(x, (uint16_t)(GRAPH_Y + GRAPH_H - h), GRAPH_COL_W, h, color); }
  graph_col = (uint16_t)((graph_col + 1U) % (GRAPH_W / GRAPH_COL_W));
  ILI9341_FillRect((uint16_t)(GRAPH_X + (graph_col * GRAPH_COL_W)), GRAPH_Y, GRAPH_COL_W, GRAPH_H, ILI9341_COLOR_GRAY);
}

void MicTest_Start(void)
{
  MicTest_DrawFrame();
  shown_status = 0xFFU;   /* force the first status draw */
  peak = 0U;
  (void)Mic_TakeBurstPeak();   /* start PEAK fresh */
  claps_heard = 0U;
  claps_shown = 0xFFU;    /* force the first CLAPS draw */
  graph_col = 0U;
  last_refresh = HAL_GetTick();
  active = 1U;
  MicTest_Refresh();
}

static void MicTest_DrawHold(void)
{
  ILI9341_DrawString(200, 14, mic_block_log_hold ? "HELD" : "    ", ILI9341_COLOR_RED, BG_COLOR, 1);
}

void MicTest_HandleInput(uint8_t pressed)
{
  if (pressed & JOY_LEFT_MASK)        { active = 0U; mic_block_log_hold = 0U; }   /* menu returns to the main screen */
  else if (pressed & JOY_CENTER_MASK) { peak = 0U; }
  else if (pressed & JOY_DOWN_MASK)
  {
    mic_block_log_hold = (uint8_t)!mic_block_log_hold;   /* freeze / resume the tuning log */
    MicTest_DrawHold();
  }
}

void MicTest_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (!active) { return; }
  if ((now - last_refresh) < REFRESH_MS) { return; }
  last_refresh = now;
  MicTest_Refresh();
}

void MicTest_Abort(void)
{
  active = 0U;
}

uint8_t MicTest_IsActive(void)
{
  return active;
}

void MicTest_OnClap(uint8_t claps)
{
  claps_heard = claps;
}
