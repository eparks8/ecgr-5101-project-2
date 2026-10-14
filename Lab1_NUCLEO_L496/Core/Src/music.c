/**
  ******************************************************************************
  * @file    music.c
  * @brief   MUSIC action: a rhythm game played with the ultrasonic sensor.
  *
  * Notes of "Mary Had a Little Lamb" scroll from right to left in 4 lanes.
  * The hand's distance from the sensor moves the pet between the lanes
  * (farther = higher lane = higher note). A note that reaches the pet while
  * the pet is in its lane is caught, and the pet sings it on the speaker.
  *
  *   COUNT-IN (3, 2, 1, GO) --> PLAY --> RESULT (3 s) --> back to the pet
  *   LEFT quits at any time, with no stat change.
  *
  * Input:        hand distance from the HC-SR04.
  * State change: up to +MUSIC_HAPPY_MAX happiness (by notes caught) and
  *               +MUSIC_HUNGER hunger. Refused if the pet is too hungry.
  * Animation:    scrolling notes, the pet moving between lanes and opening
  *               its mouth to sing each note it catches.
  ******************************************************************************
  */
#include <stdio.h>
#include "music.h"
#include "input.h"
#include "menu.h"
#include "pet.h"
#include "sound.h"
#include "ultrasonic.h"
#include "main.h"
#include "ili9341.h"

/* ---- Game settings ------------------------------------------------------- */
#define MUSIC_BEAT_MS      700U    /* one beat of the song */
#define MUSIC_SPEED        120     /* notes move this many pixels per second */
#define MUSIC_WINDOW_MS    180     /* a note can be caught this many ms early or late */
#define MUSIC_HAPPY_MAX    20      /* happiness for catching every note */
#define MUSIC_HUNGER       5       /* singing is tiring */
#define MUSIC_COUNT_MS     700U    /* one step of the 3-2-1 count-in */
#define MUSIC_RESULT_MS    3000U   /* how long the result (or "too hungry") stays up */
#define MUSIC_FRAME_MS     25U     /* redraw at most this often */

/* Hand distance -> lane: lane 0 starts 5 cm from the sensor and each lane is
 * 8 cm deep, so the four lanes cover 5 to 37 cm */
#define MUSIC_NEAR_MM      50
#define MUSIC_LANE_MM      80
#define MUSIC_HYST_MM      15      /* the pet's lane counts as 1.5 cm wider, so it doesn't flicker at a border */

/* ---- Screen layout ------------------------------------------------------- */
#define BG_COLOR       ILI9341_COLOR_BLACK
#define LINE_COLOR     0x4208U     /* dark gray */
#define LANES          4U
#define LANE_TOP       58U         /* y of the top of the lanes */
#define LANE_H         58U         /* height of one lane */
#define TITLE_Y        8U
#define STATUS_Y       32U
#define HINT_Y         45U
#define FOOTER_Y       302U

#define PET_X          6U          /* the small singing pet */
#define PET_W          28U
#define MARK_X         36U         /* the hit line */
#define HIT_X          42          /* a note's left edge when it reaches the hit line */
#define NOTE_W         16U

#define LANE_Y(l)      ((uint16_t)(LANE_TOP + ((LANES - 1U - (l)) * LANE_H)))   /* lane 0 at the bottom */
#define PET_Y(l)       ((uint16_t)(LANE_Y(l) + ((LANE_H - PET_W) / 2U)))
#define NOTE_Y(l)      ((uint16_t)(LANE_Y(l) + ((LANE_H - NOTE_W) / 2U)))

/* ---- The song -------------------------------------------------------------- */
/* Lanes 0..3 = C, D, E, G - the only four notes "Mary Had a Little Lamb" needs */
static const uint16_t lane_freq[LANES]  = { NOTE_C7, NOTE_D7, NOTE_E7, NOTE_G7 };
static const uint16_t lane_color[LANES] = { ILI9341_COLOR_RED, ILI9341_COLOR_YELLOW,
                                            ILI9341_COLOR_CYAN, ILI9341_COLOR_MAGENTA };

typedef struct
{
  uint8_t lane;
  uint8_t beats;
} MusicNote_t;

enum { L_C = 0, L_D = 1, L_E = 2, L_G = 3 };   /* lane of each note */

/* "Mary Had a Little Lamb" (traditional): {lane, beats} */
static const MusicNote_t song[] =
{
  {L_E,1},{L_D,1},{L_C,1},{L_D,1},{L_E,1},{L_E,1},{L_E,2},
  {L_D,1},{L_D,1},{L_D,2},
  {L_E,1},{L_G,1},{L_G,2},
  {L_E,1},{L_D,1},{L_C,1},{L_D,1},{L_E,1},{L_E,1},{L_E,1},{L_E,1},
  {L_D,1},{L_D,1},{L_E,1},{L_D,1},{L_C,2},
};
#define SONG_LEN  ((uint8_t)(sizeof(song) / sizeof(song[0])))

static const SoundNote_t tune_win[]  = { {NOTE_C7,120}, {NOTE_E7,120}, {NOTE_G7,120}, {NOTE_C8,300} };
static const SoundNote_t tune_lose[] = { {NOTE_G7,180}, {NOTE_E7,180}, {NOTE_C7,400} };

/* ---- Game state ------------------------------------------------------------ */
typedef enum { MUSIC_IDLE, MUSIC_COUNTIN, MUSIC_PLAY, MUSIC_RESULT } MusicState_t;

static MusicState_t state = MUSIC_IDLE;
static uint32_t     state_start;            /* when the current state began */
static uint32_t     last_frame;
static uint32_t     song_start;             /* when note 0 reaches the hit line */
static uint32_t     hit_time[SONG_LEN];     /* when each note reaches it, ms after song_start */
static int16_t      drawn_x[SONG_LEN];      /* where each note is drawn, -1 = not on screen */
static uint8_t      next_note;              /* first note not yet caught or missed */
static uint8_t      caught;
static uint8_t      lane;                   /* the pet's lane */
static uint8_t      count_step;             /* which step of 3-2-1-GO is showing */
static uint8_t      singing;                /* mouth open? */
static uint32_t     sing_until;
static uint16_t     shown_mm = 1U;          /* what the HAND readout shows (1 = nothing yet) */

/* ---- Drawing --------------------------------------------------------------- */

static void Music_DrawPet(uint8_t l, uint8_t open_mouth)
{
  uint16_t y = PET_Y(l);

  ILI9341_FillRect(PET_X, y, PET_W, PET_W, PET_FACE_COLOR);
  ILI9341_FillRect(PET_X + 7U, y + 8U, 4U, 4U, ILI9341_COLOR_BLACK);    /* eyes */
  ILI9341_FillRect(PET_X + 17U, y + 8U, 4U, 4U, ILI9341_COLOR_BLACK);
  if (open_mouth) { ILI9341_FillRect(PET_X + 10U, y + 16U, 8U, 8U, ILI9341_COLOR_BLACK); }   /* "O" - singing */
  else            { ILI9341_FillRect(PET_X + 8U, y + 19U, 12U, 3U, ILI9341_COLOR_BLACK); }   /* resting */
}

static void Music_EraseNote(uint8_t i)
{
  if (drawn_x[i] < 0) { return; }
  ILI9341_FillRect((uint16_t)drawn_x[i], NOTE_Y(song[i].lane), NOTE_W, NOTE_W, BG_COLOR);
  drawn_x[i] = -1;
}

/* Draw note i at x, and erase only the strip it moved away from */
static void Music_MoveNote(uint8_t i, int16_t x)
{
  int16_t  old = drawn_x[i];
  uint16_t y = NOTE_Y(song[i].lane);

  if (x == old) { return; }
  ILI9341_FillRect((uint16_t)x, y, NOTE_W, NOTE_W, lane_color[song[i].lane]);
  if (old > x)
  {
    ILI9341_FillRect((uint16_t)(x + (int16_t)NOTE_W), y, (uint16_t)(old - x), NOTE_W, BG_COLOR);
  }
  drawn_x[i] = x;
}

static void Music_DrawScore(void)
{
  char text[16];

  (void)snprintf(text, sizeof(text), "SCORE %2u/%2u", (unsigned)caught, (unsigned)SONG_LEN);
  ILI9341_DrawString(8, STATUS_Y, text, ILI9341_COLOR_WHITE, BG_COLOR, 1);
}

/* Live sensor reading - only redrawn when it changes, because text is slow to draw */
static void Music_DrawHand(uint16_t mm)
{
  char text[16];

  if (mm == shown_mm) { return; }
  shown_mm = mm;
  if (mm == ULTRASONIC_NONE) { (void)snprintf(text, sizeof(text), "HAND  --CM"); }
  else                       { (void)snprintf(text, sizeof(text), "HAND %3uCM", (unsigned)((mm + 5U) / 10U)); }
  ILI9341_DrawString(160, STATUS_Y, text, ILI9341_COLOR_GRAY, BG_COLOR, 1);
}

static void Music_DrawBoard(void)
{
  uint8_t l;

  ILI9341_FillScreen(BG_COLOR);
  ILI9341_DrawString(8, TITLE_Y, "MUSIC", ILI9341_COLOR_CYAN, BG_COLOR, 2);
  Menu_DrawCentered(HINT_Y, "HAND FAR = HIGH   NEAR = LOW", ILI9341_COLOR_GRAY, 1);
  for (l = 0; l <= LANES; l++)
  {
    ILI9341_DrawHLine(0, (uint16_t)(LANE_TOP + (l * LANE_H)), ILI9341_WIDTH, LINE_COLOR);   /* lane borders */
  }
  ILI9341_FillRect(MARK_X, LANE_TOP, 3U, LANES * LANE_H, LINE_COLOR);                         /* hit line */
  Menu_DrawCentered(FOOTER_Y, "LEFT = QUIT", ILI9341_COLOR_GRAY, 1);
  Music_DrawScore();
  shown_mm = 1U;
  Music_DrawHand(Ultrasonic_GetMm());
  Music_DrawPet(lane, 0U);
}

/* ---- Game logic ------------------------------------------------------------ */

/* Which lane the hand is over. The pet's current lane counts as 1.5 cm wider
 * on each side, so a hand resting on a border doesn't make the pet flicker. */
static uint8_t Music_LaneFor(uint16_t mm, uint8_t current)
{
  int32_t pos = (int32_t)mm - MUSIC_NEAR_MM;            /* mm past the start of lane 0 */
  int32_t lo  = ((int32_t)current * MUSIC_LANE_MM) - MUSIC_HYST_MM;
  int32_t hi  = (((int32_t)current + 1) * MUSIC_LANE_MM) + MUSIC_HYST_MM;
  int32_t l;

  if ((pos >= lo) && (pos < hi)) { return current; }   /* still in (the widened) current lane */

  l = pos / MUSIC_LANE_MM;
  if (l < 0)                  { l = 0; }
  if (l > (int32_t)LANES - 1) { l = (int32_t)LANES - 1; }
  return (uint8_t)l;
}

static void Music_FollowHand(void)
{
  uint16_t mm = Ultrasonic_GetMm();
  uint8_t  l;

  Music_DrawHand(mm);
  if (mm == ULTRASONIC_NONE) { return; }   /* no hand: the pet stays where it is */

  l = Music_LaneFor(mm, lane);
  if (l != lane)
  {
    ILI9341_FillRect(PET_X, PET_Y(lane), PET_W, PET_W, BG_COLOR);   /* erase from the old lane */
    lane = l;
    Music_DrawPet(lane, singing);
  }
}

static void Music_Finish(uint32_t now)
{
  char    text[24];
  int16_t happy = (int16_t)(((caught * MUSIC_HAPPY_MAX) + (SONG_LEN / 2U)) / SONG_LEN);   /* rounded */
  uint8_t good  = (uint8_t)((caught * 10U) >= (SONG_LEN * 7U));                          /* 70 % or better */

  Pet_AddHappiness(happy);
  Pet_AddHunger(MUSIC_HUNGER);

  ILI9341_FillRect(20, 110, 200, 100, BG_COLOR);
  ILI9341_DrawRect(20, 110, 200, 100, good ? ILI9341_COLOR_GREEN : ILI9341_COLOR_ORANGE);
  Menu_DrawCentered(122, good ? "BRAVO!" : "NICE TRY!", good ? ILI9341_COLOR_GREEN : ILI9341_COLOR_ORANGE, 3);
  (void)snprintf(text, sizeof(text), "%u OF %u NOTES", (unsigned)caught, (unsigned)SONG_LEN);
  Menu_DrawCentered(160, text, ILI9341_COLOR_WHITE, 1);
  (void)snprintf(text, sizeof(text), "+%d HAPPY", happy);
  Menu_DrawCentered(180, text, ILI9341_COLOR_GREEN, 2);

  if (good) { Sound_Play(tune_win, (uint8_t)(sizeof(tune_win) / sizeof(tune_win[0]))); }
  else      { Sound_Play(tune_lose, (uint8_t)(sizeof(tune_lose) / sizeof(tune_lose[0]))); }

  state = MUSIC_RESULT;
  state_start = now;
}

static void Music_PlayFrame(uint32_t now)
{
  int32_t until;
  int16_t x;
  uint8_t i;

  /* 1. Catch or miss the next note */
  if (next_note < SONG_LEN)
  {
    until = (int32_t)(song_start + hit_time[next_note]) - (int32_t)now;   /* ms until it reaches the hit line */

    if (until < -MUSIC_WINDOW_MS)
    {
      Music_EraseNote(next_note);                                          /* too late - missed */
      next_note++;
    }
    else if ((until <= MUSIC_WINDOW_MS) && (song[next_note].lane == lane))
    {
      Music_EraseNote(next_note);                                          /* caught - sing it! */
      Sound_Tone(lane_freq[lane], (uint16_t)((MUSIC_BEAT_MS * song[next_note].beats) - 120U));
      caught++;
      Music_DrawScore();
      singing = 1U;
      sing_until = now + 250U;
      Music_DrawPet(lane, 1U);
      next_note++;
    }
  }

  /* 2. Move the notes: each note's position comes from the clock, so they
   *    keep the same speed however long a frame takes */
  for (i = next_note; i < SONG_LEN; i++)
  {
    until = (int32_t)(song_start + hit_time[i]) - (int32_t)now;
    x = (int16_t)(HIT_X + ((until * MUSIC_SPEED) / 1000));
    if (x >= (int16_t)ILI9341_WIDTH) { break; }   /* this note and all later ones are still off screen */
    if (x < HIT_X) { x = HIT_X; }                 /* waits at the hit line until caught or missed */
    Music_MoveNote(i, x);
  }

  /* 3. Close the mouth after singing */
  if (singing && ((int32_t)(now - sing_until) >= 0))
  {
    singing = 0U;
    Music_DrawPet(lane, 0U);
  }

  /* 4. All notes done: show the result once the last one has finished sounding */
  if ((next_note >= SONG_LEN) && !Sound_IsPlaying()) { Music_Finish(now); }
}

static void Music_CountIn(uint32_t now)
{
  static const char *const steps[] = { "3  ", "2  ", "1  ", "GO!" };
  uint8_t step = (uint8_t)((now - state_start) / MUSIC_COUNT_MS);

  if (step >= 4U)
  {
    /* Start the song: the first note enters at the right edge of the screen */
    state = MUSIC_PLAY;
    song_start = now + ((uint32_t)(ILI9341_WIDTH - HIT_X) * 1000U / MUSIC_SPEED);
    ILI9341_DrawString(170, TITLE_Y, "   ", BG_COLOR, BG_COLOR, 2);
    return;
  }
  if (step != count_step)
  {
    count_step = step;
    ILI9341_DrawString(170, TITLE_Y, steps[step], ILI9341_COLOR_YELLOW, BG_COLOR, 2);
    Sound_Tone((step == 3U) ? NOTE_C8 : NOTE_C7, 120U);
  }
}

/* ---- The five functions the menu calls ------------------------------------- */

void Music_Start(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t beat = 0U;
  uint8_t  i;

  state_start = now;
  last_frame = now;

  if (g_pet.hunger >= PET_TOO_HUNGRY_TO_PLAY)
  {
    ILI9341_FillScreen(BG_COLOR);
    Menu_DrawCentered(120, "TOO HUNGRY", ILI9341_COLOR_RED, 3);
    Menu_DrawCentered(160, "TO SING", ILI9341_COLOR_RED, 2);
    state = MUSIC_RESULT;               /* show the refusal, no stat change */
    return;
  }

  /* When each note reaches the hit line: add up the beats before it */
  for (i = 0; i < SONG_LEN; i++)
  {
    hit_time[i] = beat * MUSIC_BEAT_MS;
    beat += song[i].beats;
    drawn_x[i] = -1;
  }
  next_note = 0U;
  caught = 0U;
  lane = 0U;
  singing = 0U;
  count_step = 0xFFU;                   /* nothing shown yet */

  Music_DrawBoard();
  state = MUSIC_COUNTIN;
}

void Music_HandleInput(uint8_t pressed)
{
  if (pressed & JOY_LEFT_MASK)
  {
    Music_Abort();                      /* quit: no stat change; the menu returns to the pet */
  }
  else if ((state == MUSIC_RESULT) && (pressed & JOY_CENTER_MASK))
  {
    state = MUSIC_IDLE;                 /* skip the rest of the result screen */
  }
}

void Music_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (state == MUSIC_IDLE) { return; }

  if (state == MUSIC_RESULT)
  {
    if ((now - state_start) >= MUSIC_RESULT_MS) { state = MUSIC_IDLE; }
    return;
  }

  if ((now - last_frame) < MUSIC_FRAME_MS) { return; }
  last_frame = now;

  Music_FollowHand();
  if (state == MUSIC_COUNTIN) { Music_CountIn(now); }
  else                        { Music_PlayFrame(now); }
}

void Music_Abort(void)
{
  Sound_Stop();
  state = MUSIC_IDLE;
}

uint8_t Music_IsActive(void)
{
  return (uint8_t)(state != MUSIC_IDLE);
}
