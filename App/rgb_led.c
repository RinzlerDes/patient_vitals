/**
 ******************************************************************************
 * @file    rgb_led.c
 * @brief   TI TLC59731 single-wire RGB LED driver for the STM32WB5MM-DK
 *          (provided driver).
 *
 *          SWEN 563 / CMPE 663 — Project 1 "PaceCheck PC-1000" starter code.
 *          PROVIDED DRIVER — do not change it, and do not send it as your
 *          own work.
 *
 *  Hardware (board BSP / UM2825):
 *      PA7  LED serial data (SDI)  — shared with OLED SPI1 MOSI through JP5.
 *      PH1  LED select             — it gates the data input of the LED
 *                                    chip. While PH1 is low, the LED chip
 *                                    ignores SPI traffic on PA7.
 *
 *  Protocol (TLC59731 "EasySet", transcribed from the ST BSP, which times
 *  it in 5 us units). The chip learns the bit period T_CYCLE from the gap
 *  between the first two rising edges. The chip then samples one bit in
 *  each cycle. A rising edge opens each bit, and a second pulse in the
 *  first half of the cycle encodes a '1'. A 32-bit frame is  0x3A | RED |
 *  GREEN | BLUE  (8-bit grayscale each). The chip latches the frame when
 *  the line stays idle.
 *
 *  PA7 handoff: this driver makes PA7 a plain GPIO output for the frame.
 *  It then gives PA7 back to SPI1 (AF5), so an OLED call can follow
 *  immediately. Main loop only. Do not call a provided driver from an ISR
 *  or from in a capture loop.
 *
 *  Pin discipline: read-modify-write on the owned bits only, exactly the
 *  rule student code must follow in all other places.
 ******************************************************************************
 */
#include "stm32wbxx.h"
#include "clock.h"
#include "rgb_led.h"

#define TLC_WRITE_CMD 0x3Au

/* BSP timing: one protocol "cycle" is 5 us. All delays below are in cycles. */
#define TLC_CYCLE_US  5u
#define TLC_DELAY     1u   /* high/low phase in a bit          */
#define TLC_TCYCLE_0  4u   /* tail wait, '0' bit               */
#define TLC_TCYCLE_1  1u   /* tail wait, '1' bit               */

static uint8_t rgb_up;     /* init guard */

static inline void tlc_wait(uint32_t cycles)
{
    clock_delay_us(cycles * TLC_CYCLE_US);
}

static inline void sdi_high(void)   { GPIOA->BSRR = GPIO_BSRR_BS7; }
static inline void sdi_low(void)    { GPIOA->BSRR = GPIO_BSRR_BR7; }
static inline void sel_high(void)   { GPIOH->BSRR = GPIO_BSRR_BS1; }
static inline void sel_low(void)    { GPIOH->BSRR = GPIO_BSRR_BR1; }

/* Borrow PA7 from SPI1: plain GPIO output, driven low. */
static void pa7_to_gpio(void)
{
    sdi_low();
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE7) | GPIO_MODER_MODE7_0;
}

/* Give PA7 back to SPI1 (AF5 — the oled driver re-asserts the other bits). */
static void pa7_to_spi(void)
{
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~GPIO_AFRL_AFSEL7)
                    | (5u << GPIO_AFRL_AFSEL7_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE7) | GPIO_MODER_MODE7_1;
}

static void tlc_send_bit(uint32_t bit)
{
    /* Rising edge opens the bit ... */
    sdi_high();
    tlc_wait(TLC_DELAY);
    sdi_low();
    tlc_wait(TLC_DELAY);

    if (bit != 0u) {
        /* ... a second pulse in the first half-cycle encodes '1'. */
        sdi_high();
        tlc_wait(TLC_DELAY);
        sdi_low();
        tlc_wait(TLC_TCYCLE_1);
    } else {
        tlc_wait(TLC_TCYCLE_0);
    }
}

static void tlc_send_byte(uint8_t byte)
{
    for (uint32_t i = 0u; i < 8u; i++) {
        tlc_send_bit((uint32_t)byte & (0x80u >> i));
    }
}

void rgb_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOHEN;
    (void)RCC->AHB2ENR;

    /* PH1 -> push-pull output, deselected (low): the LED ignores PA7. */
    GPIOH->MODER = (GPIOH->MODER & ~GPIO_MODER_MODE1) | GPIO_MODER_MODE1_0;
    sel_low();

    rgb_up = 1u;
}

void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    if (rgb_up == 0u) {
        return;
    }

    pa7_to_gpio();

    /* Select the LED chip. The BSP allows 10 ms for the enable to settle. */
    sel_low();
    clock_delay_ms(10u);
    sel_high();

    /* T_CYCLE measurement preamble: two rising edges 5 cycles apart teach
     * the chip the bit period (transcribed 1:1 from the ST BSP). */
    sdi_low();
    tlc_wait(TLC_DELAY);
    sdi_high();
    tlc_wait(TLC_TCYCLE_0);
    sdi_low();
    tlc_wait(TLC_DELAY);
    sdi_high();
    tlc_wait(TLC_TCYCLE_0);

    /* 32-bit frame: write command, then 8-bit grayscale for each channel. */
    tlc_send_byte(TLC_WRITE_CMD);
    tlc_send_byte(r);
    tlc_send_byte(g);
    tlc_send_byte(b);

    /* Idle low. A low PH1 gates the data line, and the chip latches. */
    sdi_low();
    sel_low();

    pa7_to_spi();
}
