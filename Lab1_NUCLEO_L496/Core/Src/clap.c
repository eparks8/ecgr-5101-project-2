/**
  ******************************************************************************
  * @file    clap.c
  * @brief   Clap detection and counting, fed one ~24 ms block of sound at a time.
  *
  *   ARMED --block >= CLAP_LEVEL and >= CLAP_RISE x the previous block--> LOUD
  *           (a sudden jump: words ramp up, claps don't)
  *   LOUD  --falls below the quiet level within CLAP_MAX_BLOCKS--> clap counted, REARM
  *         --still loud after CLAP_MAX_BLOCKS--> TOO_LONG (talking: sequence cancelled)
  *   REARM / TOO_LONG --CLAP_REARM_BLOCKS below the quiet level--> ARMED
  *
  * The quiet level is relative to each sound's own peak (peak / CLAP_DECAY,
  * but never below CLAP_QUIET_FLOOR), so a loud clap's echo doesn't make it
  * look "too long", and a soft clap still ends promptly.
  *
  * Claps closer together than CLAP_SEQ_GAP_BLOCKS form one command; once that
  * gap passes, the count is handed out by Clap_TakeCommand().
  ******************************************************************************
  */
#include "clap.h"

typedef enum { CLAP_ARMED, CLAP_LOUD, CLAP_REARM, CLAP_TOO_LONG } ClapState_t;

static ClapState_t state = CLAP_ARMED;
static uint8_t     loud_blocks;
static uint8_t     quiet_blocks;
static uint8_t     count;
static uint8_t     blocks_since_clap;
static uint8_t     command;
static uint16_t    prev_level;
static uint16_t    peak;          /* loudest block of the current sound */

/* Below this, the current sound has died away */
static uint16_t Clap_QuietLevel(void)
{
  uint16_t relative = (uint16_t)(peak / CLAP_DECAY);

  return (relative > CLAP_QUIET_FLOOR) ? relative : (uint16_t)CLAP_QUIET_FLOOR;
}

void Clap_Reset(void)
{
  state = CLAP_ARMED;
  prev_level = 0U;
  peak = 0U;
  loud_blocks = 0U;
  quiet_blocks = 0U;
  count = 0U;
  blocks_since_clap = 0U;
  command = 0U;
}

void Clap_Feed(uint16_t block_level)
{
  switch (state)
  {
    case CLAP_ARMED:
      if ((block_level >= CLAP_LEVEL) && ((uint32_t)block_level >= ((uint32_t)prev_level * CLAP_RISE)))
      {
        state = CLAP_LOUD;
        loud_blocks = 1U;
        peak = block_level;
      }
      break;

    case CLAP_LOUD:
      if (block_level > peak) { peak = block_level; }
      if (block_level >= Clap_QuietLevel())
      {
        if (++loud_blocks > CLAP_MAX_BLOCKS)
        {
          state = CLAP_TOO_LONG;   /* a long sound, not a clap */
          quiet_blocks = 0U;
          count = 0U;              /* talking cancels a half-finished command */
        }
      }
      else
      {
        /* Short and sharp: that was a clap */
        if (count < CLAP_MAX_COUNT) { count++; }
        blocks_since_clap = 0U;
        state = CLAP_REARM;
        quiet_blocks = 1U;
      }
      break;

    case CLAP_REARM:
    case CLAP_TOO_LONG:
      if (block_level < Clap_QuietLevel())
      {
        if (++quiet_blocks >= CLAP_REARM_BLOCKS) { state = CLAP_ARMED; }
      }
      else
      {
        quiet_blocks = 0U;
      }
      break;
  }

  /* A long enough pause after the last clap finishes the command */
  if ((count > 0U) && (state != CLAP_LOUD))
  {
    if (++blocks_since_clap >= CLAP_SEQ_GAP_BLOCKS)
    {
      command = count;
      count = 0U;
    }
  }

  prev_level = block_level;
}

uint8_t Clap_TakeCommand(void)
{
  uint8_t result = command;

  command = 0U;
  return result;
}
