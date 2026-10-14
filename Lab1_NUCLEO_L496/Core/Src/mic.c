/**
  ******************************************************************************
  * @file    mic.c
  * @brief   MAX4466 microphone on PF3 (ADC3 channel 6), powered from PF15.
  *
  * Sampling runs entirely in hardware: the ADC converts continuously and DMA
  * copies every result into a circular buffer, so it keeps recording even
  * while the main loop is busy drawing (one line of text can take ~100 ms on
  * this display). Every MIC_WINDOW_MS, Mic_Update() looks at the whole buffer
  * (the last ~190 ms of sound):
  *   - average: the mic's resting voltage, ~2048 (half of 3.3 V) when wired
  *   - level:   max - min, i.e. how far the signal swung = loudness
  ******************************************************************************
  */
#include "mic.h"
#include "main.h"

#define MIC_POWER_PORT  GPIOF
#define MIC_POWER_PIN   GPIO_PIN_15   /* MAX4466 draws ~25 uA, well within a GPIO's limit */
#define MIC_OUT_PORT    GPIOF
#define MIC_OUT_PIN     GPIO_PIN_3
#define MIC_ADC_CHANNEL ADC_CHANNEL_6 /* PF3 = ADC3_IN6 (STM32CubeMX pin database) */
#define MIC_DMA_CHANNEL DMA1_Channel3 /* ADC3 = DMA1 channel 3, request 0 (STM32CubeMX DMA database) */
#define MIC_WINDOW_MS   50U

/* ADC clock = HCLK/4 = ~14.2 MHz; 640.5 + 12.5 cycles per conversion, averaged
 * 8 at a time -> ~2.7 k samples/s, so 512 samples hold ~190 ms of sound */
#define MIC_BUFFER_LEN  512U
#define MIC_MIDSCALE    2048U

static ADC_HandleTypeDef hadc3;
static DMA_HandleTypeDef hdma_adc3;
static uint16_t buffer[MIC_BUFFER_LEN];
static uint8_t  ready;
static uint16_t raw;
static uint16_t average;
static uint16_t level;
static uint32_t last_update;

void Mic_Init(void)
{
  GPIO_InitTypeDef       gpio = {0};
  ADC_ChannelConfTypeDef channel = {0};
  uint16_t               i;

  __HAL_RCC_GPIOF_CLK_ENABLE();

  /* Power the mic from PF15 */
  HAL_GPIO_WritePin(MIC_POWER_PORT, MIC_POWER_PIN, GPIO_PIN_SET);
  gpio.Pin = MIC_POWER_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MIC_POWER_PORT, &gpio);

  /* Mic output into the ADC */
  gpio.Pin = MIC_OUT_PIN;
  gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MIC_OUT_PORT, &gpio);

  /* Start the buffer at the resting level so the first readings aren't a fake spike */
  for (i = 0; i < MIC_BUFFER_LEN; i++) { buffer[i] = MIC_MIDSCALE; }

  /* DMA: copy each ADC result into buffer[], wrapping around forever */
  __HAL_RCC_DMA1_CLK_ENABLE();
  hdma_adc3.Instance = MIC_DMA_CHANNEL;
  hdma_adc3.Init.Request = DMA_REQUEST_0;
  hdma_adc3.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc3.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc3.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc3.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc3.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_adc3.Init.Mode = DMA_CIRCULAR;
  hdma_adc3.Init.Priority = DMA_PRIORITY_LOW;
  if (HAL_DMA_Init(&hdma_adc3) != HAL_OK) { return; }
  __HAL_LINKDMA(&hadc3, DMA_Handle, hdma_adc3);

  __HAL_RCC_ADC_CLK_ENABLE();
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;    /* HCLK / 4 = ~14 MHz, no extra clock setup needed */
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;              /* 0..4095 */
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;              /* one channel only */
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = ENABLE;                  /* keeps converting by itself */
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.DMAContinuousRequests = ENABLE;               /* required for circular DMA */
  hadc3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc3.Init.OversamplingMode = ENABLE;                    /* average 8 conversions per result */
  hadc3.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_8;
  hadc3.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_3;   /* divide the sum by 8 */
  hadc3.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc3.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc3) != HAL_OK) { return; }

  channel.Channel = MIC_ADC_CHANNEL;
  channel.Rank = ADC_REGULAR_RANK_1;
  channel.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
  channel.SingleDiff = ADC_SINGLE_ENDED;
  channel.OffsetNumber = ADC_OFFSET_NONE;
  channel.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &channel) != HAL_OK) { return; }

  if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK) { return; }

  /* No DMA/ADC interrupts are enabled in the NVIC: the hardware just keeps
   * filling the buffer and Mic_Update() reads it when it gets a chance */
  if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)buffer, MIC_BUFFER_LEN) != HAL_OK) { return; }

  last_update = HAL_GetTick();
  ready = 1U;
}

void Mic_Update(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t sum = 0U;
  uint16_t lo = 0xFFFFU;
  uint16_t hi = 0U;
  uint16_t pos;
  uint16_t i;

  if (!ready) { return; }
  if ((now - last_update) < MIC_WINDOW_MS) { return; }
  last_update = now;

  for (i = 0; i < MIC_BUFFER_LEN; i++)
  {
    uint16_t s = buffer[i];
    if (s < lo) { lo = s; }
    if (s > hi) { hi = s; }
    sum += s;
  }
  average = (uint16_t)(sum / MIC_BUFFER_LEN);
  level = (uint16_t)(hi - lo);

  /* Newest sample sits just before where the DMA will write next */
  pos = (uint16_t)(MIC_BUFFER_LEN - __HAL_DMA_GET_COUNTER(&hdma_adc3));
  raw = buffer[(pos + MIC_BUFFER_LEN - 1U) % MIC_BUFFER_LEN];
}

uint8_t Mic_IsReady(void)
{
  return ready;
}

uint16_t Mic_GetRaw(void)
{
  return raw;
}

uint16_t Mic_GetAverage(void)
{
  return average;
}

uint16_t Mic_GetLevel(void)
{
  return level;
}

MicLoudness_t Mic_GetLoudness(void)
{
  if (level >= MIC_LOUD_LEVEL) { return MIC_LOUD; }
  if (level >= MIC_TALK_LEVEL) { return MIC_TALKING; }
  return MIC_QUIET;
}
