/**
 ******************************************************************************
 * @file    app.h
 * @brief   Application entry points that the generated main.c calls from
 *          its USER CODE sections. They keep main.c thin, and they stay in
 *          place after each regeneration.
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *
 *  The generated Core/Src/main.c calls exactly two of your functions:
 *      app_init()      one time, after all MX_*_Init() calls
 *      app_service()   in each cycle of the infinite loop (do not block,
 *                      R22)
 *  The stm32wbxx HAL sends the button and IMU EXTI events to the shared
 *  callback HAL_GPIO_EXTI_Callback(), which app.c holds (R17/R23).
 *
 *  As shipped, app_init and app_service run the SMOKE TEST in app.c: OLED
 *  banner, console echo, raw I2C bus scan, raw touch-key values, and
 *  button press counters. Replace the smoke test with the DG-30 state
 *  machine as you build the spec. The TSC acquisition sequence and the
 *  bus scan of the smoke test are worked examples for R1 and R3.
 ******************************************************************************
 */
#ifndef APP_H
#define APP_H

void app_init(void);
void app_service(void);

#endif /* APP_H */
