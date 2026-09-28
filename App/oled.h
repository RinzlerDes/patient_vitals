/**
 ******************************************************************************
 * @file    oled.h
 * @brief   Text API for the on-board 0.96" 128x64 OLED (SSD1315, SPI1) —
 *          HAL port of the course driver (provided).
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *          PROVIDED DRIVER — do not change it. Read it to see how a raw-SPI
 *          register driver from P0/P1 becomes a HAL driver. The SSD1315
 *          command stream is the same, but the driver calls
 *          HAL_SPI_Transmit() and HAL_GPIO_WritePin() on the
 *          CubeMX-generated hspi1, not raw register writes.
 *
 *  Ownership: CubeMX configures SPI1 and the PH0/PC8/PC9 control pins
 *  (MX_SPI1_Init and MX_GPIO_Init). This driver only USES them. It does not
 *  initialize hardware. That division of work, where CubeMX owns the init
 *  and the code owns the behavior, is the full point of requirement R24.
 *
 *  Text grid: 8 rows (0 = top) x 21 columns, 5x7 font in a 6x8 cell.
 *  Cost: about 170 µs for each line at 8 MHz SPI. Call it from the main loop
 *  only, and not from an ISR.
 *
 *  Usage:
 *      oled_init();                          // after MX_SPI1_Init()
 *      oled_clear();
 *      oled_printf(2, "HOB %3d.%d deg", a / 10, a % 10);
 *      // careful: that idiom mangles NEGATIVE tenths (lateral tilt,
 *      // GR-A) — print sign, then abs()/10 and abs()%10
 ******************************************************************************
 */
#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#define OLED_ROWS 8u   /* text rows, 0 = top */
#define OLED_COLS 21u  /* characters in a row */

/** Panel reset + init sequence. Call once, after MX_SPI1_Init(). ~120 ms. */
void oled_init(void);

/** Clear the full display. */
void oled_clear(void);

/** Write one full 21-column text row (0..7). The driver fills the remainder
 *  of the row with blanks. */
void oled_write_line(uint8_t row, const char *text);

/** printf-style wrapper around oled_write_line(). */
void oled_printf(uint8_t row, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* OLED_H */
