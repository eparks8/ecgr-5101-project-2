/**
  ******************************************************************************
  * @file    menu.h
  * @brief   Screen state machine and joystick menu navigation.
  ******************************************************************************
  */
#ifndef MENU_H
#define MENU_H

#include <stdint.h>

void Menu_Init(void);
void Menu_HandleInput(uint8_t pressed);
void Menu_Update(void);

/* Draws the pet and its meters. menu.c has a weak placeholder; a real
 * implementation anywhere in the project replaces it automatically. */
void MainScreen_Draw(void);

#endif /* MENU_H */
