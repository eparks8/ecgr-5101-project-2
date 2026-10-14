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
 * so it doesn't depend on how often the main loop runs.
 * Measured on the MIC TEST screen's PEAK: a clap ~3000, talking well below. */
#define CLAP_LEVEL          1000U   /* a block this loud can start a clap */
#define CLAP_RISE           2U      /* ...if it's also >= 2x the block before it (claps start suddenly) */
#define CLAP_DECAY          4U      /* the clap is over once it falls below its own peak / 4 */
#define CLAP_QUIET_FLOOR    250U    /* ...or below this, whichever is higher (room noise) */
#define CLAP_MAX_BLOCKS     6U      /* loud for longer than ~140 ms = not a clap (talking, shouting) */
#define CLAP_REARM_BLOCKS   2U      /* ~50 ms of quiet before the next clap can count */
#define CLAP_SEQ_GAP_BLOCKS 26U     /* ~600 ms with no new clap ends the command */
#define CLAP_MAX_COUNT      9U

void    Clap_Reset(void);
void    Clap_Feed(uint16_t block_level);   /* peak-to-peak level of one block */
uint8_t Clap_TakeCommand(void);            /* claps in the last finished sequence (0 = none); clears it */

#endif /* CLAP_H */
