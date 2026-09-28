/**
 ******************************************************************************
 * @file    console.c
 * @brief   VCP console on USART1 — printf retarget + non-blocking poll
 *          (HAL port of the course driver, provided).
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *          PROVIDED DRIVER — do not change it.
 *
 *  Retarget: the newlib _write() call arrives here (refer to syscalls.c), and
 *  printf() then transmits through the CubeMX-generated huart1.
 *
 *  Compare this driver with the P1 console.c. The register poll on TXE and
 *  RXNE became one HAL_UART_Transmit() call and one flag test. The silicon is
 *  the same, at one more level of abstraction.
 ******************************************************************************
 */
#include <stdio.h>
#include "main.h"
#include "console.h"

extern UART_HandleTypeDef huart1;        /* CubeMX-generated (main.c)     */

#define TX_TMO_MS 100u

void console_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);    /* printf: no libc buffering     */
}

/* newlib retarget: all printf and puts output comes through this function. */
int _write(int fd, char *ptr, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        if (ptr[i] == '\n') {
            uint8_t cr = '\r';
            HAL_UART_Transmit(&huart1, &cr, 1u, TX_TMO_MS);
        }
        HAL_UART_Transmit(&huart1, (uint8_t *)&ptr[i], 1u, TX_TMO_MS);
    }
    return len;
}

int console_poll(void)
{
    /* Clear a latched overrun. The user can type while no code reads RDR. */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
    }
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        return (int)(huart1.Instance->RDR & 0xFFu);
    }
    return -1;
}
