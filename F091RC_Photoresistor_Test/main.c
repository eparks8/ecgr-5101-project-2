#include "stm32f0xx_hal.h"
#include <stdint.h>

/* Photoresistor divider output -> PA3 (ADC1_IN3). Use a 10 kOhm resistor
   from PA3 to GND and the LDR from 3V3 to PA3, as in the existing project. */
#define PHOTO_ADC_CHANNEL ADC_CHANNEL_0
#define DARK_ENTER_RAW   ((4095U * 25U) / 100U)
#define DARK_EXIT_RAW    ((4095U * 35U) / 100U)
#define AVG_COUNT        16U

static ADC_HandleTypeDef hadc;
void Error_Handler(void);
static volatile uint16_t latest_raw;
static volatile uint8_t dark_state;

void SystemClock_Config(void)
{
  /* Use the internal 8 MHz HSI clock; no PLL required for this ADC test. */
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState = RCC_PLL_OFF;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = GPIO_PIN_0;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* NUCLEO-F091RC onboard LD2 is PA5. LED on means the reading is dark. */
  gpio.Pin = GPIO_PIN_5;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

static void ADC_Init(void)
{
  __HAL_RCC_ADC1_CLK_ENABLE();
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc.Init.SamplingTimeCommon = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_Init(&hadc) != HAL_OK) Error_Handler();
  ADC_ChannelConfTypeDef channel = {0};
  channel.Channel = PHOTO_ADC_CHANNEL;
  channel.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc, &channel) != HAL_OK) Error_Handler();
  HAL_StatusTypeDef cal_status = HAL_ADCEx_Calibration_Start(&hadc);

  if (cal_status != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adc)
{
  if (adc->Instance == ADC1) {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    for (volatile uint32_t i = 0; i < 30000U; ++i) { }
  }
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  GPIO_Init();
  ADC_Init();

   while (1)
 {
    uint32_t sum = 0;
    for (uint32_t n = 0; n < AVG_COUNT; ++n)
    {
      HAL_ADC_Start(&hadc);
      if (HAL_ADC_PollForConversion(&hadc, 10) != HAL_OK)
      {
        Error_Handler();
      }
      sum += HAL_ADC_GetValue(&hadc);
      HAL_ADC_Stop(&hadc);
    }
    latest_raw = (uint16_t)(sum / AVG_COUNT);

    if (!dark_state && latest_raw < DARK_ENTER_RAW)
    {
      dark_state = 1;
    }
    else if (dark_state && latest_raw > DARK_EXIT_RAW)
    {
      dark_state = 0;
    }

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5,
                      dark_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_Delay(100);
 }
}
