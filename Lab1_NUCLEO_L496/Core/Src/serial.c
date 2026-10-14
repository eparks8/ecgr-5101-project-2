/**
  ******************************************************************************
  * @file    serial.c
  * @brief   Serial link to a PC over LPUART1 and the ST-LINK virtual COM port.
  *
  * Wiring: none. On the NUCLEO-L496ZG-P, LPUART1 TX (PG7) and RX (PG8) are
  * already connected to the ST-LINK, which shows up on the PC as a COM port
  * over the same USB cable used for flashing.
  *
  * Write (STM32 -> PC):
  *   - a status line every STATUS_MS: time, hunger, happiness, mood, state
  *   - a message the moment the pet dies or runs away
  *
  * Read (PC -> STM32), one key at a time:
  *   W A S D      = joystick UP / LEFT / DOWN / RIGHT
  *   Space, Enter = joystick CENTER
  *   I            = status line right now
  *   H or ?       = list the commands
  *
  * Nothing here waits. Each received byte lands in rx_buf from the LPUART1
  * interrupt and is handled by Serial_Update() in the main loop. Text to send
  * is queued in tx_buf and sent in the background with HAL_UART_Transmit_IT().
  ******************************************************************************
  */
#include <stdio.h>
#include <string.h>
#include "serial.h"
#include "input.h"
#include "pet.h"
#include "main.h"

#define SERIAL_BAUD   115200U
#define STATUS_MS     5000U
#define RX_BUF_LEN    32U      /* power of two, so the index wraps with a mask */
#define TX_BUF_LEN    1024U    /* power of two; holds the help text with room to spare */

UART_HandleTypeDef hlpuart1;   /* shared with LPUART1_IRQHandler() in stm32l4xx_it.c */

static uint8_t           rx_byte;              /* HAL receives one byte at a time into here */
static uint8_t           rx_buf[RX_BUF_LEN];
static volatile uint16_t rx_head;              /* written by the interrupt */
static uint16_t          rx_tail;              /* read by the main loop */

static uint8_t           tx_buf[TX_BUF_LEN];
static uint16_t          tx_head;              /* written by Serial_Print() */
static volatile uint16_t tx_tail;              /* advanced when a transfer finishes */
static volatile uint16_t tx_len;               /* bytes in the transfer now running, 0 = idle */

static uint32_t last_status;
static uint8_t  was_dead;
static uint8_t  was_gone;

static const char help_text[] =
  "\r\n"
  "=== VIRTUAL PET - serial link ===\r\n"
  "  W A S D     joystick up / left / down / right\r\n"
  "  Space/Enter joystick center (select)\r\n"
  "  I           status now\r\n"
  "  H or ?      this help\r\n"
  "Status is sent every 5 s.\r\n"
  "\r\n";

void Serial_Print(const char *text)
{
  while (*text != '\0')
  {
    uint16_t next = (uint16_t)((tx_head + 1U) & (TX_BUF_LEN - 1U));

    if (next == tx_tail) { return; }   /* queue full: drop the rest rather than wait */
    tx_buf[tx_head] = (uint8_t)*text++;
    tx_head = next;
  }
}

/* Start sending the next queued chunk if nothing is being sent */
static void Serial_KickTx(void)
{
  uint16_t tail = tx_tail;
  uint16_t len;

  if ((tx_len != 0U) || (tx_head == tail)) { return; }

  /* Send up to the newest byte, or up to the end of the buffer if the queue wraps */
  len = (tx_head > tail) ? (uint16_t)(tx_head - tail) : (uint16_t)(TX_BUF_LEN - tail);
  tx_len = len;
  if (HAL_UART_Transmit_IT(&hlpuart1, &tx_buf[tail], len) != HAL_OK)
  {
    tx_len = 0U;   /* try again next pass */
  }
}

static const char *Serial_MoodName(PetMood_t mood)
{
  switch (mood)
  {
    case PET_MOOD_HAPPY:  return "HAPPY";
    case PET_MOOD_SAD:    return "SAD";
    case PET_MOOD_HUNGRY: return "HUNGRY";
    default:              return "OK";
  }
}

static void Serial_SendStatus(uint32_t now)
{
  char        line[96];
  const char *state = "ALIVE";

  if (Pet_IsDead())          { state = "DEAD"; }
  else if (Pet_HasRunAway()) { state = "RAN AWAY"; }

  (void)snprintf(line, sizeof(line), "[%5lu s] HUNGER %3u  HAPPY %3u  MOOD %-6s  %s\r\n",
                 (unsigned long)(now / 1000U), (unsigned int)g_pet.hunger,
                 (unsigned int)g_pet.happiness, Serial_MoodName(Pet_GetMood()), state);
  Serial_Print(line);
}

static void Serial_HandleKey(uint8_t key, uint32_t now)
{
  switch (key)
  {
    case 'w': case 'W':           Input_Inject(JOY_UP_MASK);     break;
    case 'a': case 'A':           Input_Inject(JOY_LEFT_MASK);   break;
    case 's': case 'S':           Input_Inject(JOY_DOWN_MASK);   break;
    case 'd': case 'D':           Input_Inject(JOY_RIGHT_MASK);  break;
    case ' ': case '\r':          Input_Inject(JOY_CENTER_MASK); break;
    case 'i': case 'I':           Serial_SendStatus(now);        break;
    case 'h': case 'H': case '?': Serial_Print(help_text);       break;
    default:                      break;   /* '\n' after Enter and anything else: ignored */
  }
}

void Serial_Init(void)
{
  GPIO_InitTypeDef         gpio = {0};
  RCC_PeriphCLKInitTypeDef clk = {0};

  /* LPUART1 runs from PCLK1 (28.4 MHz) */
  clk.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
  clk.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) { return; }
  __HAL_RCC_LPUART1_CLK_ENABLE();

  /* PG7 = TX, PG8 = RX (port G is powered by HAL_PWREx_EnableVddIO2() in MX_GPIO_Init) */
  __HAL_RCC_GPIOG_CLK_ENABLE();
  gpio.Pin = STLK_RX_Pin | STLK_TX_Pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;   /* idle-high line stays high if the ST-LINK side is unpowered */
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF8_LPUART1;
  HAL_GPIO_Init(GPIOG, &gpio);

  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = SERIAL_BAUD;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK) { return; }

  HAL_NVIC_SetPriority(LPUART1_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(LPUART1_IRQn);

  (void)HAL_UART_Receive_IT(&hlpuart1, &rx_byte, 1U);   /* wait for the first key */

  Serial_Print(help_text);
  last_status = HAL_GetTick();
}

void Serial_Update(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t  dead = Pet_IsDead();
  uint8_t  gone = Pet_HasRunAway();

  /* Read: handle every key received since the last pass */
  while (rx_tail != rx_head)
  {
    Serial_HandleKey(rx_buf[rx_tail], now);
    rx_tail = (uint16_t)((rx_tail + 1U) & (RX_BUF_LEN - 1U));
  }

  /* Write: announce the end conditions as soon as they happen */
  if (dead && !was_dead) { Serial_Print("*** Your pet starved. Press Space to restart. ***\r\n"); }
  if (gone && !was_gone) { Serial_Print("*** Your pet got bored and ran away. Press Space to restart. ***\r\n"); }
  was_dead = dead;
  was_gone = gone;

  /* Write: regular status line */
  if ((now - last_status) >= STATUS_MS)
  {
    last_status = now;
    Serial_SendStatus(now);
  }

  Serial_KickTx();
}

/* --- HAL callbacks (run in the LPUART1 interrupt) ------------------------- */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart != &hlpuart1) { return; }

  uint16_t next = (uint16_t)((rx_head + 1U) & (RX_BUF_LEN - 1U));
  if (next != rx_tail)   /* buffer full: drop the key */
  {
    rx_buf[rx_head] = rx_byte;
    rx_head = next;
  }
  (void)HAL_UART_Receive_IT(&hlpuart1, &rx_byte, 1U);   /* wait for the next key */
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart != &hlpuart1) { return; }

  tx_tail = (uint16_t)((tx_tail + tx_len) & (TX_BUF_LEN - 1U));
  tx_len = 0U;   /* the main loop starts the next chunk */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart != &hlpuart1) { return; }

  /* A line glitch (e.g. the USB cable replugged) aborts reception - restart it */
  (void)HAL_UART_Receive_IT(&hlpuart1, &rx_byte, 1U);
}
