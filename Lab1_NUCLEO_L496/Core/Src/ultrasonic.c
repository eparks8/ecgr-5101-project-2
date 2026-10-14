/**
  ******************************************************************************
  * @file    ultrasonic.c
  * @brief   HC-SR04 ultrasonic distance sensor, timed by TIM1.
  *
  * TIM1 counts 800,000 times a second (1.25 us per count) and starts over
  * every 60 ms (48000 counts). Its channels do the whole measurement:
  *   - CH3 (PE13 -> Trig): HIGH for the first 12 counts (15 us) of every
  *     60 ms, which makes the sensor send one ultrasonic ping.
  *   - CH1 (PE9 <- Echo): when Echo goes HIGH, the timer saves its count.
  *   - CH2 (same pin):    when Echo goes LOW, the timer saves its count.
  * Echo stays HIGH while the sound travels to the hand and back, so
  *   distance = counts x 1.25 us x 0.343 mm/us / 2 = counts x 343 / 1600 mm
  *
  * Ultrasonic_Update() just picks up each new pair of counts - no waiting.
  * The middle of the last 3 readings is used, so one bad echo can't jump it.
  ******************************************************************************
  */
#include "ultrasonic.h"
#include "main.h"

#define US_STALE_MS  300U      /* no reading for this long = nothing there */

static TIM_HandleTypeDef htim1;
static uint16_t last3[3];       /* the last three readings, for the median */
static uint8_t  last3_idx;
static uint16_t distance_mm = ULTRASONIC_NONE;
static uint32_t last_reading;   /* when the last good reading arrived */

/* The middle value of three */
static uint16_t Ultrasonic_Median3(uint16_t a, uint16_t b, uint16_t c)
{
  uint16_t t;

  if (a > b) { t = a; a = b; b = t; }   /* now a <= b */
  if (b > c) { b = c; }                 /* b = the smaller of b and c */
  return (a > b) ? a : b;               /* the larger of a and that */
}

void Ultrasonic_Init(void)
{
  GPIO_InitTypeDef   gpio = {0};
  TIM_OC_InitTypeDef oc = {0};
  TIM_IC_InitTypeDef ic = {0};

  /* PE13 (Trig) and PE9 (Echo) are run by TIM1 (alternate function 1) */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_13 | GPIO_PIN_9;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLDOWN;              /* Echo reads LOW if the sensor is unplugged */
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOE, &gpio);

  __HAL_RCC_TIM1_CLK_ENABLE();
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 70U;             /* 56.8 MHz / 71 = 0.8 MHz */
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 47999U;             /* 48000 counts = 60 ms between pings */
  HAL_TIM_PWM_Init(&htim1);

  /* CH3: the trigger pulse */
  oc.OCMode = TIM_OCMODE_PWM1;
  oc.Pulse = 12U;                         /* 12 counts = 15 us (the sensor needs >= 10 us) */
  oc.OCPolarity = TIM_OCPOLARITY_HIGH;
  HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_3);

  /* CH1: save the count when Echo rises */
  ic.ICPolarity = TIM_ICPOLARITY_RISING;
  ic.ICSelection = TIM_ICSELECTION_DIRECTTI;      /* CH1's own pin, PE9 */
  HAL_TIM_IC_ConfigChannel(&htim1, &ic, TIM_CHANNEL_1);

  /* CH2: save the count when Echo falls */
  ic.ICPolarity = TIM_ICPOLARITY_FALLING;
  ic.ICSelection = TIM_ICSELECTION_INDIRECTTI;    /* CH1's pin too, so no second wire */
  HAL_TIM_IC_ConfigChannel(&htim1, &ic, TIM_CHANNEL_2);

  HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);       /* starts the pings; also switches on TIM1's output (MOE) */
}

void Ultrasonic_Update(void)
{
  uint32_t now = HAL_GetTick();
  uint16_t rise;
  uint16_t fall;
  uint32_t mm;

  /* No reading for a while: nothing in front of the sensor */
  if ((now - last_reading) >= US_STALE_MS) { distance_mm = ULTRASONIC_NONE; }

  /* Wait until the timer has saved a falling edge, i.e. a complete echo */
  if (!__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_CC2)) { return; }

  rise = (uint16_t)HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_1);   /* reading clears the flags */
  fall = (uint16_t)HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_2);
  if (fall <= rise) { return; }

  mm = ((uint32_t)(fall - rise) * 343U) / 1600U;
  if ((mm < 20U) || (mm > 4000U)) { return; }      /* closer than 2 cm or farther than 4 m: not real */

  if (distance_mm == ULTRASONIC_NONE)
  {
    /* First reading after "nothing there": start the median fresh */
    last3[0] = last3[1] = last3[2] = (uint16_t)mm;
  }
  last3[last3_idx] = (uint16_t)mm;
  last3_idx = (uint8_t)((last3_idx + 1U) % 3U);

  distance_mm = Ultrasonic_Median3(last3[0], last3[1], last3[2]);
  last_reading = now;
}

uint16_t Ultrasonic_GetMm(void)
{
  return distance_mm;
}
