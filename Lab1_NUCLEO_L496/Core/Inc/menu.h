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

/* Main screen - implemented in test_pet.c */

/* Draws the pet and its meters */
void MainScreen_Draw(void);

/* Called every main-loop pass while the main screen is showing, for idle
 * animations and live meter updates. Must not block. */
void MainScreen_Update(void);

/* One clap heard on the main screen: the pet notices you */
void MainScreen_Notice(void);

#endif /* MENU_H */
