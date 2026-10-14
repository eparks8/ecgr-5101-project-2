/**
  ******************************************************************************
  * @file    clap.h
  * @brief   Clap detection and counting, fed one ~24 ms block of sound at a time.
  *
  * No hardware code here, so the logic can be tested on a PC.
  ******************************************************************************
  */
#ifndef CLAP_H
#define CLAP_H

#include <stdint.h>

/* One block = 64 mic samples at ~2.7 kHz = ~24 ms. All timing is in blocks,
 * so it doesn't depend on how often the main loop runs. */
#define CLAP_LEVEL          380U                /* a block this loud starts a clap - tune with MIC TEST's PEAK */
#define CLAP_QUIET          (CLAP_LEVEL / 2U)   /* below this the sound has died away */
#define CLAP_RISE           2U                  /* a clap starts suddenly: >= 2x the block before it */
#define CLAP_MAX_BLOCKS     5U                  /* loud for longer than ~120 ms = not a clap (talking, shouting) */
#define CLAP_REARM_BLOCKS   2U                  /* ~50 ms of quiet before the next clap can count */
#define CLAP_SEQ_GAP_BLOCKS 26U                 /* ~600 ms with no new clap ends the command */
#define CLAP_MAX_COUNT      9U

void    Clap_Reset(void);
void    Clap_Feed(uint16_t block_level);   /* peak-to-peak level of one block */
uint8_t Clap_TakeCommand(void);            /* claps in the last finished sequence (0 = none); clears it */

#endif /* CLAP_H */
