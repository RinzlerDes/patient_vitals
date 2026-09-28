/**
 ******************************************************************************
 * @file    console.h
 * @brief   VCP console on USART1 (115200 8N1): printf retarget + a
 *          non-blocking key poll — HAL port of the course driver (provided).
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *          PROVIDED DRIVER — do not change it.
 *
 *  Ownership: CubeMX configures USART1 (MX_USART1_UART_Init, huart1). This
 *  driver only uses the handle. printf sends the output to PuTTY, and this
 *  driver expands '\n' to "\r\n" to keep the session log clean. That log is
 *  the audit record (R19).
 *
 *  All printf output BLOCKS at 115200 (about 87 µs for each character), so
 *  budget the cost. The only input call, console_poll(), does not block.
 *  Use console_poll() in the super loop (R20).
 ******************************************************************************
 */
#ifndef CONSOLE_H
#define CONSOLE_H

/** Nothing to configure — CubeMX owns USART1. Call once for the banner. */
void console_init(void);

/**
 * Non-blocking receive. Return the next received character (0..255), or -1
 * when the receive register is empty. This call clears an overrun error
 * without a message, thus type-ahead stays safe.
 */
int console_poll(void);

#endif /* CONSOLE_H */
