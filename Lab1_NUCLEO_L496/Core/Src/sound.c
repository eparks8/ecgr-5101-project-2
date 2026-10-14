/**
  ******************************************************************************
  * @file    sound.c
  * @brief   Notes and short tunes on the speaker (TIM16 PWM on PE0).
  *
  * A note is a square wave: the pin switches HIGH/LOW freq_hz times a second.
  * TIM16 does the switching by itself:
  *   - it counts 800,000 times a second (56.8 MHz / 71),
  *   - it starts over every 800000 / freq_hz counts (one wave),
  *   - the pin is HIGH for the first half of each wave (PWM, 50 % duty).
  * Silence = HIGH for 0 counts, so the pin stays LOW.
  *
  * Sound_Update() only has to notice when a note's time is up and start the
  * next one, so nothing ever waits.
  ******************************************************************************
  */
#include <stddef.h>
#include "sound.h"
#include "main.h"

#define SOUND_TIMER_HZ  800000U   /* 56.8 MHz / (70 + 1) */

static TIM_HandleTypeDef  htim16;
static const SoundNote_t *tune;         /* the tune playing, NULL = silent */
static uint8_t            tune_len;
static uint8_t            tune_pos;     /* which note of the tune */
static uint32_t           note_start;   /* when that note started */
static uint32_t           stopped_at;   /* when the last tune ended */
static SoundNote_t        single;       /* Sound_Tone() plays this one-note tune */

static void Sound_SetFreq(uint16_t freq_hz)
{
  uint32_t period;

  if (freq_hz == NOTE_REST)
  {
    __HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, 0U);           /* HIGH for 0 counts = silent */
    return;
  }

  period = SOUND_TIMER_HZ / freq_hz;                             /* counts in one wave */
  __HAL_TIM_SET_AUTORELOAD(&htim16, period - 1U);                /* start over after one wave */
  __HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, period / 2U);    /* HIGH for the first half */
}

void Sound_Init(void)
{
  GPIO_InitTypeDef   gpio = {0};
  TIM_OC_InitTypeDef oc = {0};

  /* PE0 is switched by TIM16 (alternate function 14) instead of by our code */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_0;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF14_TIM16;
  HAL_GPIO_Init(GPIOE, &gpio);

  __HAL_RCC_TIM16_CLK_ENABLE();
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 70U;                                   /* 56.8 MHz / 71 = 0.8 MHz */
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 799U;                                     /* any value - it starts silent */
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; /* a new note starts at the end of a wave */
  HAL_TIM_PWM_Init(&htim16);

  oc.OCMode = TIM_OCMODE_PWM1;            /* pin HIGH while the count is below the compare value */
  oc.Pulse = 0U;                          /* silent */
  oc.OCPolarity = TIM_OCPOLARITY_HIGH;
  HAL_TIM_PWM_ConfigChannel(&htim16, &oc, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);   /* also switches on TIM16's output (MOE) */
}

void Sound_Play(const SoundNote_t *notes, uint8_t count)
{
  if (count == 0U) { return; }

  tune = notes;
  tune_len = count;
  tune_pos = 0U;
  note_start = HAL_GetTick();
  Sound_SetFreq(tune[0].freq_hz);
}

void Sound_Tone(uint16_t freq_hz, uint16_t duration_ms)
{
  single.freq_hz = freq_hz;
  single.duration_ms = duration_ms;
  Sound_Play(&single, 1U);
}

void Sound_Stop(void)
{
  if (tune != NULL) { stopped_at = HAL_GetTick(); }
  tune = NULL;
  Sound_SetFreq(NOTE_REST);
}

void Sound_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (tune == NULL) { return; }                                       /* nothing playing */
  if ((now - note_start) < tune[tune_pos].duration_ms) { return; }    /* note not over yet */

  tune_pos++;
  if (tune_pos >= tune_len)
  {
    Sound_Stop();                                                     /* tune finished */
    return;
  }

  note_start = now;
  Sound_SetFreq(tune[tune_pos].freq_hz);                              /* next note */
}

uint8_t Sound_IsPlaying(void)
{
  return (uint8_t)(tune != NULL);
}

uint8_t Sound_IsBusy(void)
{
  return (uint8_t)((tune != NULL) || ((HAL_GetTick() - stopped_at) < SOUND_MIC_GUARD_MS));
}
