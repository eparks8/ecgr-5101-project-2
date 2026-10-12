/**
  ******************************************************************************
  * @file    input.h
  * @brief   Debounced X-NUCLEO-GFX01M2 joystick (B1) press events.
  ******************************************************************************
  */
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* Joystick bit flags returned by Input_GetPressed() */
#define JOY_LEFT_MASK   (1U << 0)
#define JOY_CENTER_MASK (1U << 1)
#define JOY_DOWN_MASK   (1U << 2)
#define JOY_RIGHT_MASK  (1U << 3)
#define JOY_UP_MASK     (1U << 4)

void    Input_Update(void);      /* call every main-loop pass */
uint8_t Input_GetPressed(void);  /* buttons newly pressed since the last call, then clears */

#endif /* INPUT_H */
