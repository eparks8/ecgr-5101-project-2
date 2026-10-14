/**
 * pet_analog.c - see pet_analog.h
 *
 * Pin:  PC3 = ADC123_IN4 (Arduino A2, Zio CN9 pin 5)
 * ADC:  ADC2 (ADC1 belongs to the potentiometer on PA3/A0, via HAL),
 *       12-bit, right aligned, single conversion, software trigger.
 *
 * Shared-hardware rules (ADC1 and ADC2 share ADC12_COMMON->CCR and the
 * RCC ADC clock):
 *   - Call PetAnalog_Init() AFTER MX_ADC1_Init(). HAL_ADC_Init() only writes
 *     the common clock settings while no ADC is enabled, so if ADC2 were
 *     enabled first, the pot's clock configuration would be silently skipped.
 *   - This module never overwrites a clock someone else configured. It only
 *     sets CKMODE (synchronous HCLK/4) as a fallback when no ADC clock has
 *     been configured at all and ADC1 is not enabled - e.g. standalone tests.
 *   - Only ADC2 registers are written otherwise. ADC1 is never touched.
 */
#include "pet_analog.h"
#include "stm32l4xx_hal.h"
#include "ili9341.h"
#include <stdio.h>

#define PET_ADC             ADC2
#define PET_ADC_CHANNEL     4U            /* PC3 = ADC123_IN4 */
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
  /* 1. Pin PC3 as analog input with the ADC analog switch connected. */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin  = GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &gpio);

  /* 2. ADC bus clock on (shared bit - setting it again is harmless). */
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  (void)RCC->AHB2ENR;                                   /* read-back delay */

  /* Fallback only: if nobody has configured an ADC clock (no synchronous
   * CKMODE and no asynchronous ADCSEL source) and ADC1 is not enabled,
   * select synchronous HCLK/4. Otherwise keep the existing configuration. */
  if (((ADC123_COMMON->CCR & ADC_CCR_CKMODE) == 0U) &&
      ((RCC->CCIPR & RCC_CCIPR_ADCSEL) == 0U) &&
      ((ADC1->CR & ADC_CR_ADEN) == 0U))
  {
    ADC123_COMMON->CCR = (ADC123_COMMON->CCR & ~ADC_CCR_CKMODE)
                       | ADC_CCR_CKMODE_0 | ADC_CCR_CKMODE_1;
  }

  /* 3. Leave deep power-down, enable the voltage regulator (~20 us). */
  PET_ADC->CR &= ~ADC_CR_DEEPPWD;
  PET_ADC->CR |= ADC_CR_ADVREGEN;
  HAL_Delay(1);

  /* 4. Calibrate (single-ended). */
  PET_ADC->CR &= ~ADC_CR_ADCALDIF;
  PET_ADC->CR |= ADC_CR_ADCAL;
  (void)wait_clear(&PET_ADC->CR, ADC_CR_ADCAL);

  /* 5. Enable and wait for ready. */
  PET_ADC->ISR |= ADC_ISR_ADRDY;                        /* clear flag */
  PET_ADC->CR  |= ADC_CR_ADEN;
  (void)wait_set(&PET_ADC->ISR, ADC_ISR_ADRDY);

  /* 6. 12-bit, right aligned, single conversion, software trigger. */
  PET_ADC->CFGR = 0U;
  PET_ADC->CFGR2 = 0U;

  /* Channel 4, 247.5 cycles sampling (SMP = 110) for a ~10k source. */
  PET_ADC->SMPR1 = (PET_ADC->SMPR1 & ~ADC_SMPR1_SMP4)
                 | (6UL << ADC_SMPR1_SMP4_Pos);
  /* Sequence length 1 (L = 0), first conversion = channel 4. */
  PET_ADC->SQR1 = (PET_ADC_CHANNEL << ADC_SQR1_SQ1_Pos);

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
  PET_ADC->ISR |= ADC_ISR_EOC;                          /* clear stale flag */
  PET_ADC->CR  |= ADC_CR_ADSTART;
  if (!wait_set(&PET_ADC->ISR, ADC_ISR_EOC)) {
    return 0U;                                          /* timeout */
  }
  return (uint16_t)(PET_ADC->DR & 0x0FFFU);             /* reading DR clears EOC */
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
