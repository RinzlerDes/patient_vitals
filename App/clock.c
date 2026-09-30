/**
 ******************************************************************************
 * @file    clock.c
 * @brief   64 MHz SYSCLK bring-up for the STM32WB5MM-DK (provided driver).
 *
 *          SWEN 563 / CMPE 663 — Project 1 "PaceCheck PC-1000" starter code.
 *          PROVIDED DRIVER — do not change it, and do not send it as your
 *          own work.
 *
 *  Clock tree after clock_init():
 *
 *      HSE 32 MHz (crystal, ±ppm)
 *        └─ PLL:  /M=4 → 8 MHz → xN=16 → VCO 128 MHz → /R=2 → 64 MHz
 *             └─ SYSCLK = HCLK1 = 64 MHz   (CPU1, AHB)
 *                  ├─ PCLK1 = 64 MHz       (APB1: TIM2 kernel clock = 64 MHz)
 *                  ├─ PCLK2 = 64 MHz       (APB2: USART1, SPI1)
 *                  └─ HCLK2 = 32 MHz       (CPU2 domain: must stay ≤ 32 MHz)
 *
 *  This driver uses direct register access only (RM0434). Read it. It
 *  applies the same discipline that your own RCC, GPIO and TIM2 code must
 *  apply.
 ******************************************************************************
 */
#include "stm32wbxx.h"
#include "clock.h"

#define CLOCK_SYSCLK_HZ  64000000u

void clock_init(void)
{
    /* --- Flash wait states -------------------------------------------------
     * 64 MHz at Vcore range 1 needs 3 wait states (RM0434, FLASH_ACR).
     * Set the latency and the prefetch BEFORE you raise the clock. Then read
     * FLASH_ACR back: the write is not guaranteed effective until it is
     * observable.
     */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_3WS
                 | FLASH_ACR_PRFTEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_3WS) { }

    /* --- HSE on -------------------------------------------------------------
     * The WB5MM module has its own 32 MHz crystal. A timing instrument runs
     * from the crystal, not from an RC oscillator.
     */
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u) { }

    /* --- PLL configuration (the PLL must be off when you write PLLCFGR) --- */
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) { }

    /* Source = HSE (0b11), M = /4 (field = M-1 = 3), N = x16, R = /2
     * (field = R-1 = 1), PLLR output enabled. VCO = 32/4*16 = 128 MHz,
     * PLLRCLK = 128/2 = 64 MHz.
     */
    RCC->PLLCFGR = (RCC->PLLCFGR
                    & ~(RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM
                        | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLR
                        | RCC_PLLCFGR_PLLREN))
                   | (RCC_PLLCFGR_PLLSRC_0 | RCC_PLLCFGR_PLLSRC_1) /* HSE   */
                   | (3u  << RCC_PLLCFGR_PLLM_Pos)                 /* /4    */
                   | (16u << RCC_PLLCFGR_PLLN_Pos)                 /* x16   */
                   | (1u  << RCC_PLLCFGR_PLLR_Pos)                 /* /2    */
                   | RCC_PLLCFGR_PLLREN;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) { }

    /* --- CPU2 (radio core) prescaler ---------------------------------------
     * HCLK2 must stay at 32 MHz or below (RM0434). Divide by 2 (0b1000)
     * BEFORE you switch SYSCLK to 64 MHz. The CPU1 (HPRE), APB1 and APB2
     * prescalers keep their /1 reset values: HCLK1 = PCLK1 = PCLK2 = 64 MHz.
     */
    RCC->EXTCFGR = (RCC->EXTCFGR & ~RCC_EXTCFGR_C2HPRE)
                   | (8u << RCC_EXTCFGR_C2HPRE_Pos);

    /* --- Switch SYSCLK to the PLL, then wait until SWS shows the PLL ------ */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (RCC_CFGR_SW_0 | RCC_CFGR_SW_1);
    while ((RCC->CFGR & RCC_CFGR_SWS) != (RCC_CFGR_SWS_0 | RCC_CFGR_SWS_1)) { }

    /* Update the CMSIS SystemCoreClock variable to the new frequency. */
    SystemCoreClockUpdate();

    /* --- DWT cycle counter --------------------------------------------------
     * Free-running 32-bit counter at 64 MHz. clock_delay_us() and the other
     * provided drivers use it for their internal timing. No provided driver
     * touches SysTick at any time — SysTick belongs to student code.
     */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t clock_sysclk_hz(void)
{
    return CLOCK_SYSCLK_HZ;
}


void dwt_init(void)
{
    static uint8_t initialized = 0u;

    if (initialized == 0u) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0u;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

        initialized = 1u;
    }
}

void clock_delay_us(uint32_t us)
{
    /* 64 cycles in each microsecond. The unsigned subtraction is wrap-safe. */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (CLOCK_SYSCLK_HZ / 1000000u);
    while ((uint32_t)(DWT->CYCCNT - start) < ticks) { }
}

void clock_delay_ms(uint32_t ms)
{
    while (ms-- > 0u) {
        clock_delay_us(1000u);
    }
}

