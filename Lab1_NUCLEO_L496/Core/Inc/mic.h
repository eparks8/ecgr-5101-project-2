/**
  ******************************************************************************
  * @file    mic.h
  * @brief   MAX4466 microphone on PF3 (ADC3 channel 6), powered from PF15.
  *
  * Wiring (back of the Nucleo, left long header, outer column, rows counted
  * up from the bottom): PF15 row 6 -> mic VCC, PF3 row 7 -> mic OUT,
  * GND row 9 -> mic GND.
  ******************************************************************************
  */
#ifndef MIC_H
#define MIC_H

#include <stdint.h>

/* Loudness thresholds on the peak-to-peak level (0..4095), tuned with the MIC
 * test screen: quiet room ~122, normal talking ~320, a clap ~450 */
#define MIC_TALK_LEVEL  200U
#define MIC_LOUD_LEVEL  400U

typedef enum { MIC_QUIET, MIC_TALKING, MIC_LOUD } MicLoudness_t;

void          Mic_Init(void);        /* powers the mic and starts the ADC */
void          Mic_Update(void);      /* call every main-loop pass - never blocks */
uint8_t       Mic_IsReady(void);     /* 0 if the ADC failed to start */
uint16_t      Mic_GetRaw(void);      /* newest ADC sample, 0..4095 */
uint16_t      Mic_GetAverage(void);  /* average of the last window - about 2048 when wired correctly */
uint16_t      Mic_GetLevel(void);    /* loudness: max - min over the last ~190 ms */
MicLoudness_t Mic_GetLoudness(void); /* level sorted into quiet / talking / loud */
uint16_t      Mic_TakeBurstPeak(void);   /* loudest ~24 ms block since the last call (what claps are compared to) */
uint8_t       Mic_TakeClapCommand(void); /* claps in the last finished sequence, 0 if none; clears it */

#endif /* MIC_H */
