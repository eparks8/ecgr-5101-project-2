/**
  ******************************************************************************
  * @file    sound.h
  * @brief   Notes and short tunes on the CVS-1508 speaker.
  *
  * Wiring: PE0 (Zio CN10 pin 33, D34) -> 220 ohm -> speaker -> GND.
  * Sounds play in the background: start a note or tune, and Sound_Update()
  * (called every main-loop pass) moves on to the next note when it's time.
  ******************************************************************************
  */
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/* Note frequencies in Hz. This small speaker is almost silent below ~1.3 kHz,
 * so the notes come from the 7th octave. */
#define NOTE_REST  0U
#define NOTE_C7    2093U
#define NOTE_D7    2349U
#define NOTE_E7    2637U
#define NOTE_G7    3136U
#define NOTE_C8    4186U

/* The mic ignores sound until this long after the speaker stops, so the
 * pet's own beeps never count as claps */
#define SOUND_MIC_GUARD_MS  300U

typedef struct
{
  uint16_t freq_hz;       /* NOTE_REST = silence */
  uint16_t duration_ms;
} SoundNote_t;

void    Sound_Init(void);
void    Sound_Update(void);                                  /* call every main-loop pass */
void    Sound_Tone(uint16_t freq_hz, uint16_t duration_ms);  /* play one note */
void    Sound_Play(const SoundNote_t *notes, uint8_t count); /* play a tune (array must be static const) */
void    Sound_Stop(void);
uint8_t Sound_IsPlaying(void);
uint8_t Sound_IsBusy(void);   /* playing, or stopped less than SOUND_MIC_GUARD_MS ago */

#endif /* SOUND_H */
