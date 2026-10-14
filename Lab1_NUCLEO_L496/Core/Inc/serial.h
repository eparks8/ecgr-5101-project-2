/**
  ******************************************************************************
  * @file    serial.h
  * @brief   Serial link to a PC over LPUART1 and the ST-LINK virtual COM port.
  *
  * Settings for the PC terminal: 115200 baud, 8 data bits, no parity, 1 stop
  * bit (115200 8N1), no flow control.
  ******************************************************************************
  */
#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void Serial_Init(void);               /* starts LPUART1 and prints the welcome text */
void Serial_Update(void);             /* call every main-loop pass - never blocks */
void Serial_Print(const char *text);  /* queues text to send; dropped if the queue is full */

#endif /* SERIAL_H */
