/**
 ******************************************************************************
 * @file    rgb_led.h
 * @brief   On-board RGB LED (TI TLC59731, single-wire protocol) driver
 *          (provided driver).
 *
 *          SWEN 563 / CMPE 663 — Project 1 "PaceCheck PC-1000" starter code.
 *          PROVIDED DRIVER — do not change it, and do not send it as your
 *          own work.
 *
 * ============================================================================
 *  DO-NOT-TOUCH PINS (spec section 7.3)
 *  This driver exclusively owns PH1 (LED select) and shares PA7 (LED data)
 *  with the OLED SPI1 MOSI (jumper JP5 ON). The provided drivers own all of
 *  the pins below. Do not reconfigure any of them at any time, and
 *  read-modify-write each shared GPIO register.
 *
 *      PA1  - OLED SPI1 SCK.
 *      PA7  - OLED SPI1 MOSI  *and*  TLC59731 RGB-LED data (shared, JP5 ON).
 *      PH0  - OLED chip select.       PH1  - RGB-LED select.
 *      PC8  - OLED reset.             PC9  - OLED data/command.
 *      PB6  - console USART1 TX.      PB7  - console USART1 RX.
 * ============================================================================
 *
 *  The cause for a provided driver: the LED is NOT a simple GPIO. A TLC59731
 *  PWM chip drives it, and a pulse-coded single-wire protocol on PA7 drives
 *  that chip. PA7 is the same physical pin as the SPI data line of the OLED.
 *  rgb_set() borrows PA7 as a plain GPIO, clocks out the 32-bit frame,
 *  latches it, and gives the pin back to SPI1. PH1 gates the LED chip, so
 *  the LED chip ignores OLED traffic on PA7 while PH1 is low.
 *
 *  Cost model: one rgb_set() call takes ~11 ms (dominated by a select-enable
 *  settling delay). Call it at run start and at verdict time, from the main
 *  loop. Do not call it from an ISR. Do not call it in the capture path
 *  (R10). Do not call it while an OLED write continues.
 *
 *  Usage (R8 verdict colors):
 *      rgb_init();                    // after clock_init()
 *      rgb_set(0, 0, 32);             // blue: acquiring
 *      rgb_set(0, 32, 0);             // green: PASS   (latches until next)
 *      rgb_set(32, 0, 0);             // red: FAIL
 *      rgb_set(32, 32, 32);           // white
 *      rgb_set(0, 0, 0);              // off
 *
 *  Brightness is 0..255 per channel. A value of 32 is comfortable indoors.
 *  A value of 255 is sufficient for a demo across the room.
 ******************************************************************************
 */
#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>

/** Prepare the PH1 and PA7 bookkeeping. Call one time, after clock_init(). */
void rgb_init(void);

/** Set the LED color. Blocking, ~11 ms. Main loop only. */
void rgb_set(uint8_t r, uint8_t g, uint8_t b);

#endif /* RGB_LED_H */
