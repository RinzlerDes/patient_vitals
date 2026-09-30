/**
 ******************************************************************************
 * @file    clock.h
 * @brief   64 MHz SYSCLK bring-up for the STM32WB5MM-DK (provided driver).
 *
 *          SWEN 563 / CMPE 663 — Project 1 "PaceCheck PC-1000" starter code.
 *          PROVIDED DRIVER — do not change it, and do not send it as your
 *          own work.
 *
 * ============================================================================
 *  DO-NOT-TOUCH PINS (spec section 7.3)
 *  The provided drivers exclusively own the pins below. Student code must not
 *  reconfigure them at any time. Student code must not write a full GPIO
 *  configuration register (MODER / OTYPER / OSPEEDR / PUPDR / AFRL / AFRH) on
 *  port A, B, C or H. Configure only the bits you own (read-modify-write).
 *
 *      PA1  - OLED SPI1 SCK.
 *      PA7  - OLED SPI1 MOSI  *and*  TLC59731 RGB-LED data (shared, JP5 ON).
 *      PH0  - OLED chip select.       PH1  - RGB-LED select.
 *      PC8  - OLED reset.             PC9  - OLED data/command.
 *      PB6  - console USART1 TX.      PB7  - console USART1 RX.
 *
 *  A wholesale write such as  GPIOA->MODER = 0x400  kills the display
 *  mid-demo. This is the first read-modify-write object lesson of the course.
 * ============================================================================
 *
 *  What this driver does:
 *   - It turns on the 32 MHz HSE crystal and the PLL multiplies it to a
 *     64 MHz SYSCLK (HSE 32 MHz / M=4 * N=16 / R=2 = 64 MHz), so the TIM2
 *     kernel clock is 64 MHz. Your 1 us timebase (PSC = 63) needs this number.
 *   - It uses the *crystal* (±ppm) and not the internal RC oscillator (±%).
 *     PaceCheck is a timing instrument, and its accuracy chain starts here.
 *   - It sets the flash wait states and the CPU2 prescaler that 64 MHz needs.
 *   - It enables the Cortex-M4 DWT cycle counter that clock_delay_us() uses.
 *
 *  By design this driver does not touch SysTick, TIM2, or any GPIO. Those
 *  belong to student code.
 ******************************************************************************
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/**
 * Set the system clock to 64 MHz. Call it one time, at the start of main(),
 * before you initialize any other provided driver.
 */
// void clock_init(void);

/** Returns the SYSCLK frequency in Hz after clock_init() (64,000,000). */
// uint32_t clock_sysclk_hz(void);

/**
 * Busy-wait delay helpers. They use the DWT cycle counter, so they do not
 * use SysTick. SysTick belongs to student code.
 *
 * These functions BLOCK the CPU. They are correct for init code and for the
 * operator console. Do not call them in your capture loop. Do not call them
 * for the R1 POST timeout. That timeout must be non-blocking, and that
 * is part of the test. One call can delay about 60 s at maximum.
 */
void clock_delay_us(uint32_t us);
void clock_delay_ms(uint32_t ms);
void dwt_init(void);

#endif /* CLOCK_H */
