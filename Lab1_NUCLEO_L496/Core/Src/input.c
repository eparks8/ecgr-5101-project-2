/**
  ******************************************************************************
  * @file    input.c
  * @brief   Debounced X-NUCLEO-GFX01M2 joystick (B1) press events.
  *
  * A raw reading only counts once it has been stable for DEBOUNCE_MS, and
  * only the released -> pressed edge is reported, so holding a direction
  * produces exactly one event.
  ******************************************************************************
  */
#include "input.h"
#include "main.h"
#include "gfx01m2_conf.h"

#define DEBOUNCE_MS 30U

static uint8_t  last_raw;
static uint8_t  stable;
static uint32_t last_change;
static uint8_t  pressed_events;

static uint8_t Input_ReadRaw(void)
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

void Input_Update(void)
{
  uint8_t  raw = Input_ReadRaw();
  uint32_t now = HAL_GetTick();

  if (raw != last_raw)
  {
    /* Still bouncing - restart the settle timer */
    last_raw = raw;
    last_change = now;
  }
  else if (((now - last_change) >= DEBOUNCE_MS) && (raw != stable))
  {
    /* Settled on a new state: record buttons that went released -> pressed */
    pressed_events |= (uint8_t)(raw & (uint8_t)~stable);
    stable = raw;
  }
}

uint8_t Input_GetPressed(void)
{
  uint8_t events = pressed_events;

  pressed_events = 0;
  return events;
}
