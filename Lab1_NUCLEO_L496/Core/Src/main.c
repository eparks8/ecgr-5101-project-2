/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : X-NUCLEO-GFX01M2 LCD demo for the NUCLEO-L496ZG-P
  *
  * This demo drives the 2.2" ILI9341 QVGA display on the X-NUCLEO-GFX01M2
  * expansion board over SPI1 and cycles through a few simple graphics demos:
  * color bars, filled/outlined shapes, text, and a live joystick (B1)
  * readout. See Drivers/BSP/GFX01M2 for the small display driver used here.
  *
  * WIRING - the GFX01M2 plugs directly onto the top of this board's CN11/CN12
  * ST Morpho headers. NOTE: ST's own datasheet (DB4236) only lists Nucleo-64
  * boards as compatible with the GFX01M2 - the NUCLEO-L496ZG-P (Nucleo-144)
  * is not officially validated, so UM2750 has no pin table for this exact
  * combination. Pins were instead cross-referenced against this board's own
  * schematic (mb1312-l4xxzx-a03): RESET=PA1, CS=PA9, DC=PB10, SPI1 SCK/MISO/
  * MOSI=PA5/PA6/PA7. See Drivers/BSP/GFX01M2/gfx01m2_conf.h for the full
  * derivation and the PA9/USB_VBUS_Pin sharing note.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ili9341.h"
#include "gfx01m2_conf.h"
#include "pumpkin_sprite.h"

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
typedef enum {
  BLACK,
  FIELD,
  HOUSE
} Background;

typedef enum {
  IDLE, // Passive hunger and happiness drain
  FEED,
  ACT1,
  ACT2,
  BORED, // End condition
  HUNGRY // End condition
} State;

typedef struct {
    uint16_t width;
    uint16_t height;
    const uint16_t *data;
} Sprite;

typedef struct {
  uint8_t frame;
  int16_t happiness;
  int16_t hunger;

} Pet;

Sprite idle = {
  IDLE_SPRITE_WIDTH,
  IDLE_SPRITE_HEIGHT,
  idle_sprite
};

Sprite bored = {
  BORED_SPRITE_WIDTH,
  BORED_SPRITE_HEIGHT,
  bored_sprite
};

Sprite hungry = {
  HUNGRY_SPRITE_WIDTH,
  HUNGRY_SPRITE_HEIGHT,
  hungry_sprite
};

#define IDLE_HAPPY_DRAIN 10
#define IDLE_HUNGER_DRAIN 5
#define MAX_HAPPY 1000
#define MAX_HUNGER 1000

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
// Drawing functions
static void Draw_Background(Background background);
static void Draw_Sprite(uint16_t start_x, uint16_t start_y, Sprite *s);
static void Draw_Happiness(Pet *p);
static void Draw_Hunger(Pet *p);
// Stat getters
static int16_t Pet_Happiness(Pet *p);
static int16_t Pet_Hunger(Pet *p);
static uint8_t Pet_Frame(Pet *p);
// Stat setters
static State IDLE_Update(Pet *p);
static State FEED_Update(Pet *p);
static State ACT1_Update(Pet *p);
static State ACT2_Update(Pet *p);
static State BORED_Update(Pet *p);
static State HUNGRY_Update(Pet *p);

/* Joystick (B1) bit flags returned by Joystick_Read() */
#define JOY_LEFT_MASK   (1U << 0)
#define JOY_CENTER_MASK (1U << 1)
#define JOY_DOWN_MASK   (1U << 2)
#define JOY_RIGHT_MASK  (1U << 3)
#define JOY_UP_MASK     (1U << 4)

static uint8_t Joystick_Read(void)
{
  uint8_t state = 0;

  /* All five directions are active low (see gfx01m2_conf.h) */
  if (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET)     { state |= JOY_LEFT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_CENTER_GPIO_Port, JOY_CENTER_Pin) == GPIO_PIN_RESET) { state |= JOY_CENTER_MASK; }
  if (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET)     { state |= JOY_DOWN_MASK; }
  if (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET)   { state |= JOY_RIGHT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET)         { state |= JOY_UP_MASK; }

  return state;
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();

  /* Initialize the LCD and show a splash screen */
  ILI9341_Init(&hspi1);
  HAL_Delay(2000);

  /* Initialize the pet and program values. */
  Pet myPet = {
    0, MAX_HAPPY, MAX_HUNGER,
  };

  Background bg = FIELD;
  State status = IDLE;
  State nextStatus = IDLE;

  Draw_Background(bg);
  Draw_Sprite(56,96,&idle);

  while (1) 
  {
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    
    switch (status)
    {
      case IDLE:
        nextStatus = IDLE_Update(&myPet);
        break;
      case FEED:
        nextStatus = FEED_Update(&myPet);
        break;
      case ACT1:
        nextStatus = ACT1_Update(&myPet);
        break;
      case ACT2:
        nextStatus = ACT2_Update(&myPet);
        break;
      case BORED:
        nextStatus = BORED_Update(&myPet);
        break;
      case HUNGRY:
        nextStatus = HUNGRY_Update(&myPet);
        break;
      default:
        break;
    }

    if (status != nextStatus)
    {
      switch (nextStatus)
      {
        case IDLE:
          Draw_Sprite(56, 96, &idle);
          break;
        case BORED:
          Draw_Sprite(56, 96, &bored);
          break;
        case HUNGRY:
          Draw_Sprite(56, 96, &hungry);
          break;
        default:
          break;
      }
      status = nextStatus;
    }

    Draw_Happiness(&myPet);
    Draw_Hunger(&myPet);

    HAL_Delay(10);
  }
}

/**
 * @brief Draws the specified background.
 *        Should always be called before any other action.
 * @retval None
 */
static void Draw_Background(Background background) {
  switch (background)
  {
  case BLACK:
    ILI9341_FillScreen(ILI9341_COLOR_BLACK);
    break;
  case FIELD:
    ILI9341_FillScreen(0x867d); // Sky
    break;
  case HOUSE:

    break;
  default:
    break;
  }
}

/**
 * @brief Draws neutral sprite position.
 * @retval None
 */
void Draw_Sprite(uint16_t start_x, uint16_t start_y, Sprite *s) {
    uint32_t i = 0;
    for (uint16_t y = 0; y < s->height; y++) {
        for (uint16_t x = 0; x < s->width; x++) {
            uint16_t color = s->data[i++];
            if (color != 0xFFFF) ILI9341_DrawPixel(start_x + x, start_y + y, color);
        }
    }
}

/**
 * @brief Draws horizontal happiness bar without double-filling the background.
 * @retval None
 */
static void Draw_Happiness(Pet *p) {
  // Bounding brackets
  ILI9341_FillRect(8,8,2,24,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(10,10,8,2,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(10,28,8,2,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(230,8,2,24,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(222,10,8,2,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(222,28,8,2,ILI9341_COLOR_BLACK);

  // Calculate active bar length (max 220px)
  uint16_t filled_width = (220 * Pet_Happiness(p)) / MAX_HAPPY;
  if (filled_width > 220) filled_width = 220;

  // 1. Draw colored active bar portion
  if (filled_width > 0) {
    ILI9341_FillRect(10, 12, filled_width, 16, ILI9341_COLOR_YELLOW);
  }

  // 2. Fill the remaining empty portion with white
  if (filled_width < 220) {
    ILI9341_FillRect(10 + filled_width, 12, 220 - filled_width, 16, ILI9341_COLOR_WHITE);
  }
}

/**
 * @brief Draws horizontal hunger bar without double-filling the background.
 * @retval None
 */
static void Draw_Hunger(Pet *p) {
  // Bounding brackets
  ILI9341_FillRect(8,288,2,24,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(10,290,8,2,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(10,308,8,2,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(230,288,2,24,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(222,290,8,2,ILI9341_COLOR_BLACK);
  ILI9341_FillRect(222,308,8,2,ILI9341_COLOR_BLACK);

  // Calculate active bar length (max 220px)
  uint16_t filled_width = (220 * Pet_Hunger(p)) / MAX_HUNGER;
  if (filled_width > 220) filled_width = 220;

  // 1. Draw colored active bar portion
  if (filled_width > 0) {
    ILI9341_FillRect(10, 292, filled_width, 16, ILI9341_COLOR_RED);
  }

  // 2. Fill the remaining empty portion with white
  if (filled_width < 220) {
    ILI9341_FillRect(10 + filled_width, 292, 220 - filled_width, 16, ILI9341_COLOR_WHITE);
  }
}

/**
 * @brief Returns pet happiness.
 * @retval int8_t happiness
 */
static int16_t Pet_Happiness(Pet *p) {
  return (p->happiness <= 0) ? 0 : p->happiness;
}

/**
 * @brief Returns pet hunger.
 * @retval int8_t hunger
 */
static int16_t Pet_Hunger(Pet *p) {
  return (p->hunger <= 0) ? 0 : p->hunger;
}

/**
 * @brief Returns animation frame.
 * @retval uint8_t frame
 */
static uint8_t Pet_Frame(Pet *p) {
  return p->frame;
}

/**
 * @brief Updates status values for value drain.
 * @retval None
 */
static State IDLE_Update(Pet *p) {
  p->happiness -= IDLE_HAPPY_DRAIN;
  p->hunger -= IDLE_HUNGER_DRAIN;

  if (!Pet_Happiness(p)) return BORED;
  if (!Pet_Hunger(p)) return HUNGRY;
  return IDLE;
}

/**
 * @brief Updates status values for feed action.
 * @retval None
 */
static State FEED_Update(Pet *p) {
  return IDLE;
}

/**
 * @brief Updates status values for ACT1 action.
 * @retval None
 */
static State ACT1_Update(Pet *p) {
  return IDLE;
}

/**
 * @brief Updates status values for ACT2 action.
 * @retval None
 */
static State ACT2_Update(Pet *p) {
  return IDLE;
}

/**
 * @brief Resets status values and sets background for BORED end condition.
 * @retval None
 */
static State BORED_Update(Pet *p) {
  HAL_Delay(3000); // TODO: Replace with input for continue.
  p->happiness = MAX_HAPPY;
  p->hunger = MAX_HUNGER;
  return IDLE;
}

/**
 * @brief Resets status values and sets background for HUNGRY end condition.
 * @retval None
 */
static State HUNGRY_Update(Pet *p) {
  HAL_Delay(3000); // TODO: Replace with input for continue.
  p->happiness = MAX_HAPPY;
  p->hunger = MAX_HUNGER;
  return IDLE;
}

/**
  * @brief SPI1 Initialization Function
  *        SCK = PA5, MISO = PA6, MOSI = PA7 - connects to the GFX01M2 LCD.
  * @retval None
  */
static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  /* PCLK2 is now ~56.8 MHz (HCLK was previously quartered by a stray AHB
   * DIV4 - see SystemClock_Config). /32 keeps the same ~1.775 MHz target
   * that gave comfortable margin over the ILI9341's 10 MHz write ceiling
   * on this jumper-wired, unsupported-board setup. */
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_9;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 71;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  /* DIV4 here previously left HCLK at 56.8/4 = 14.2 MHz despite the PLL
   * being configured for 56.8 MHz - DIV1 makes HCLK match the PLL output. */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /* HCLK is now ~56.8 MHz - VOS Range 1 requires 3 wait states for
   * 48-64 MHz (RM0351), not the 0 WS that was only valid up to 16 MHz. */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin, GPIO_PIN_RESET);

  /* LCD CS/RESET idle high, DC state does not matter until the first transfer */
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OverCurrent_Pin SMPS_PG_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin|SMPS_PG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_PowerSwitchOn_Pin SMPS_V1_Pin SMPS_EN_Pin SMPS_SW_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_CS_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

  /* LCD_DC is on a different port than LCD_CS (GPIOB vs GPIOA) - must be a separate HAL_GPIO_Init call */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RESET_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_LEFT_Pin JOY_DOWN_Pin JOY_RIGHT_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_LEFT_Pin|JOY_DOWN_Pin|JOY_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_CENTER_Pin JOY_UP_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_CENTER_Pin|JOY_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
