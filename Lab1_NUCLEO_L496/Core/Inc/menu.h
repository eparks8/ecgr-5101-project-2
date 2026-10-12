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

/* Draws text horizontally centred on a black background (5x7 font, any size) */
void Menu_DrawCentered(uint16_t y, const char *text, uint16_t color, uint8_t size);

/* Draws the pet and its meters. menu.c has a weak placeholder; a real
 * implementation anywhere in the project replaces it automatically. */
void MainScreen_Draw(void);

/* Called every main-loop pass while the main screen is showing, for idle
 * animations and live meter updates. Must not block. Weak no-op in menu.c. */
void MainScreen_Update(void);

#endif /* MENU_H */
