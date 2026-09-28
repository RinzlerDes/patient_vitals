/**
 ******************************************************************************
 * @file    vitals_bus.h
 * @brief   YOUR FILE — the sensor bus IO layer. It connects the
 *          bus-agnostic ST component drivers (Drivers/Components) to the
 *          CubeMX-owned I2C3 handle.
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *
 *  This header and its .c hold the HAL-era interop lesson. The component
 *  drivers (stts22h.c, ism330dhcx.c) know nothing about your board. They
 *  speak through five function pointers that you give them (STTS22H_IO_t
 *  and ISM330DHCX_IO_t). CubeMX owns the peripheral INIT, because
 *  MX_I2C3_Init built hi2c3. Your IO layer owns the TRANSACTIONS, which
 *  are the HAL_I2C_Mem_Read and HAL_I2C_Mem_Write calls on that same
 *  handle. No layer initializes the bus two times. That division of work
 *  is the point of spec R24. It also causes one more rule: this project
 *  does NOT use the full ST BSP, because the BSP bus layer initializes
 *  I2C3 again on a private duplicate handle.
 *
 *  After vitals_bus_init() returns 0, call the component APIs directly:
 *      STTS22H_TEMP_GetTemperature(&temp_sensor, &deg_c);      // float °C
 *      ISM330DHCX_ACC_GetAxes(&imu, &axes);                    // mg
 ******************************************************************************
 */
#ifndef VITALS_BUS_H
#define VITALS_BUS_H

#include "ism330dhcx.h"
#include "stts22h.h"

/* The two connected driver objects. They are ready for component API calls
 * after vitals_bus_init() returns 0. vitals_bus.c owns them. */
extern STTS22H_Object_t    temp_sensor;
extern ISM330DHCX_Object_t imu;

/**
 * Connect the two component drivers to hi2c3, compare the two WHO_AM_I ids
 * against the expected ids, and enable measurement. The temperature part
 * free-runs at 1 Hz. The accel part runs at 104 Hz and ±2 g. Returns 0 on
 * success, or a negative code at the first failure. POST uses this code, so
 * report WHICH device failed (R3).
 */
int32_t vitals_bus_init(void);

#endif /* VITALS_BUS_H */
