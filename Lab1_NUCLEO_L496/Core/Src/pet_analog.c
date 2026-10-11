/**
 * pet_analog.c - see pet_analog.h
 *
 * Pin:  PA3 = ADC1_IN8 (Arduino A0)
 * ADC:  ADC1, 12-bit, right aligned, single conversion, software trigger.
 * Clock: synchronous HCLK/4 (CKMODE = 11), so the ADC clock stays well under
 *        its 80 MHz limit whatever HCLK is set to.
 */
#include "pet_analog.h"
#include "stm32l4xx_hal.h"
#include "ili9341.h"
#include <stdio.h>

#define PET_ADC_CHANNEL     8U            /* PA3 */
#define PET_ADC_TIMEOUT     100000U       /* loop-count guard, avoids a hang */

static uint16_t s_samples[PET_ANALOG_AVG_SAMPLES];
static uint32_t s_sum;
static uint8_t  s_idx;
static bool     s_dark;

/* Wait until (reg & mask) != 0. Returns false on timeout. */
static bool wait_set(volatile uint32_t *reg, uint32_t mask)
{
  uint32_t n = PET_ADC_TIMEOUT;
  while (((*reg) & mask) == 0U) {
    if (n-- == 0U) { return false; }
  }
  return true;
}

/* Wait until (reg & mask) == 0. Returns false on timeout. */
static bool wait_clear(volatile uint32_t *reg, uint32_t mask)
{
  uint32_t n = PET_ADC_TIMEOUT;
  while (((*reg) & mask) != 0U) {
    if (n-- == 0U) { return false; }
  }
  return true;
}

void PetAnalog_Init(void)
{
  /* 1. Pin PA3 as analog input with the ADC analog switch connected. */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin  = GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* 2. ADC clock on, synchronous HCLK/4. */
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  (void)RCC->AHB2ENR;                                   /* read-back delay */
  ADC123_COMMON->CCR = (ADC123_COMMON->CCR & ~ADC_CCR_CKMODE)
                     | ADC_CCR_CKMODE_0 | ADC_CCR_CKMODE_1;

  /* 3. Leave deep power-down, enable the voltage regulator (~20 us). */
  ADC1->CR &= ~ADC_CR_DEEPPWD;
  ADC1->CR |= ADC_CR_ADVREGEN;
  HAL_Delay(1);

  /* 4. Calibrate (single-ended). */
  ADC1->CR &= ~ADC_CR_ADCALDIF;
  ADC1->CR |= ADC_CR_ADCAL;
  (void)wait_clear(&ADC1->CR, ADC_CR_ADCAL);

  /* 5. Enable and wait for ready. */
  ADC1->ISR |= ADC_ISR_ADRDY;                           /* clear flag */
  ADC1->CR  |= ADC_CR_ADEN;
  (void)wait_set(&ADC1->ISR, ADC_ISR_ADRDY);

  /* 6. 12-bit, right aligned, single conversion, software trigger. */
  ADC1->CFGR = 0U;
  ADC1->CFGR2 = 0U;

  /* Channel 8, 247.5 cycles sampling (SMP = 110) for a ~10k source. */
  ADC1->SMPR1 = (ADC1->SMPR1 & ~ADC_SMPR1_SMP8)
              | (6UL << ADC_SMPR1_SMP8_Pos);
  /* Sequence length 1 (L = 0), first conversion = channel 8. */
  ADC1->SQR1 = (PET_ADC_CHANNEL << ADC_SQR1_SQ1_Pos);

  /* 7. Prime the moving average so the first reads are sensible. */
  uint16_t first = PetAnalog_ReadRaw();
  s_sum = 0U;
  for (uint32_t i = 0; i < PET_ANALOG_AVG_SAMPLES; i++) {
    s_samples[i] = first;
    s_sum += first;
  }
  s_idx  = 0U;
  s_dark = (PetAnalog_GetPercent() < PET_ANALOG_DARK_ENTER_PCT);
}

uint16_t PetAnalog_ReadRaw(void)
{
  ADC1->ISR |= ADC_ISR_EOC;                             /* clear stale flag */
  ADC1->CR  |= ADC_CR_ADSTART;
  if (!wait_set(&ADC1->ISR, ADC_ISR_EOC)) {
    return 0U;                                          /* timeout */
  }
  return (uint16_t)(ADC1->DR & 0x0FFFU);                /* reading DR clears EOC */
}

void PetAnalog_Update(void)
{
  uint16_t v = PetAnalog_ReadRaw();
  s_sum -= s_samples[s_idx];
  s_samples[s_idx] = v;
  s_sum += v;
  s_idx = (uint8_t)((s_idx + 1U) & (PET_ANALOG_AVG_SAMPLES - 1U));

  uint8_t pct = PetAnalog_GetPercent();
  if (s_dark) {
    if (pct > PET_ANALOG_DARK_EXIT_PCT)  { s_dark = false; }
  } else {
    if (pct < PET_ANALOG_DARK_ENTER_PCT) { s_dark = true; }
  }
}

uint16_t PetAnalog_GetRaw(void)
{
  return (uint16_t)(s_sum / PET_ANALOG_AVG_SAMPLES);
}

uint16_t PetAnalog_GetMillivolts(void)
{
  return (uint16_t)(((uint32_t)PetAnalog_GetRaw() * PET_ANALOG_VREF_MV)
                    / PET_ANALOG_MAX_RAW);
}

uint8_t PetAnalog_GetPercent(void)
{
  return (uint8_t)(((uint32_t)PetAnalog_GetRaw() * 100U) / PET_ANALOG_MAX_RAW);
}

bool PetAnalog_IsDark(void)
{
  return s_dark;
}

void PetAnalog_DrawDebug(uint16_t x, uint16_t y)
{
  char line[32];
  snprintf(line, sizeof(line), "RAW %4u  %4umV ",
           (unsigned)PetAnalog_GetRaw(), (unsigned)PetAnalog_GetMillivolts());
  ILI9341_DrawString(x, y, line, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK, 2);
  snprintf(line, sizeof(line), "LIGHT %3u%% %s ",
           (unsigned)PetAnalog_GetPercent(), s_dark ? "DARK " : "LIGHT");
  ILI9341_DrawString(x, (uint16_t)(y + 20U), line,
                     ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK, 2);
}
