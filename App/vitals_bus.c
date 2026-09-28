/**
 ******************************************************************************
 * @file    vitals_bus.c
 * @brief   YOUR FILE — the sensor bus IO layer (refer to vitals_bus.h for
 *          the concept).
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *
 *  WHAT YOU WRITE HERE (spec R3/R4/R13 groundwork):
 *
 *   1. The five IO callbacks that each component driver needs.
 *      STTS22H_IO_t and ISM330DHCX_IO_t give their shapes. Read the
 *      headers in Drivers/Components, because they ARE the documentation.
 *        - Init:     nothing to do, because CubeMX ran MX_I2C3_Init()
 *                    before your code starts. Return 0. (RegisterBusIO
 *                    refuses a NULL pointer.)
 *        - DeInit:   also a no-op that returns 0.
 *        - ReadReg:  HAL_I2C_Mem_Read (&hi2c3, DevAddr, Reg,
 *                    I2C_MEMADD_SIZE_8BIT, pData, Len, timeout). Map
 *                    HAL_OK to 0, and map each other value to -1.
 *        - WriteReg: HAL_I2C_Mem_Write, with the same mapping.
 *        - GetTick:  (int32_t)HAL_GetTick().
 *      The IO struct of the IMU has one more member, Delay. Connect it to
 *      a HAL_Delay wrapper. The ST BSP forgets this member, and a garbage
 *      function pointer is a defect, also while no code calls it today.
 *
 *   2. vitals_bus_init(), for each sensor:
 *        a. fill the IO struct. The Address comes from the component
 *           header (STTS22H_I2C_ADD_H / ISM330DHCX_I2C_ADD_H). The DK
 *           straps the two address pins high. These are 8-bit forms, so
 *           give them to the driver without a change,
 *        b. <sensor>_RegisterBusIO(),
 *        c. <sensor>_ReadID() and CHECK the id (STTS22H_ID /
 *           ISM330DHCX_ID). Report a mismatch by name with printf and
 *           return a negative value (R3),
 *        d. <sensor>_Init(), then TEMP_Enable or ACC_Enable. The drivers
 *           power up DISABLED, and no data streams before you enable one,
 *        e. set the rates with STTS22H_TEMP_SetOutputDataRate(
 *           &temp_sensor, 1.0f). The IMU default after ACC_Enable is
 *           104 Hz at ±2 g, which is correct for R4.
 *
 *  Check your work with the D2 demo row. POST must print the two WHO_AM_I
 *  ids. Then single-step ONE ReadReg call down into HAL_I2C_Mem_Read, and
 *  see that it uses the same I2C3 registers that you programmed by hand
 *  in P1. That path, from your call down to the registers and back, is
 *  the point of this project.
 ******************************************************************************
 */
#include "vitals_bus.h"

#include <stdio.h>

#include "main.h"
#include "stm32wbxx_hal.h"
#include "stm32wbxx_hal_def.h"
#include "stm32wbxx_hal_i2c.h"

extern I2C_HandleTypeDef hi2c3; /* CubeMX-generated (main.c)     */

#define BUS_TMO_MS 100u

STTS22H_Object_t temp_sensor;
ISM330DHCX_Object_t imu;

/* ===================== STUDENT CODE BEGIN — IO glue ===================== */

/* TODO: write the IO callbacks that the header describes, for example:
 *
 *   static int32_t bus_init(void)   { return 0; }
 *   static int32_t bus_read(uint16_t addr, uint16_t reg,
 *                           uint8_t *p, uint16_t len) { ... }
 *   ...
 */

static int32_t bus_init(void) {
    /* MX_I2C3_Init() already configured the peripheral. */
    return 0;
}

static int32_t bus_deinit(void) {
    // no op
    return 0;
}

static int32_t bus_read(uint16_t addr, uint16_t reg, uint8_t* p, uint16_t len) {
    HAL_StatusTypeDef result =
        HAL_I2C_Mem_Read(&hi2c3, addr, reg, I2C_MEMADD_SIZE_8BIT, p, len, BUS_TMO_MS);

    if (result == HAL_OK) {
        return 0;
    }

    return -1;
}

static int32_t bus_write(uint16_t addr, uint16_t reg, uint8_t* p, uint16_t len) {
    HAL_StatusTypeDef result =
        HAL_I2C_Mem_Write(&hi2c3, addr, reg, I2C_MEMADD_SIZE_8BIT, p, len, BUS_TMO_MS);

    if (result == HAL_OK) {
        return 0;
    }

    return -1;
}

static uint32_t bus_get_tick() {
    uint32_t now_ms = HAL_GetTick();
    if (now_ms > INT32_MAX) {
        printf("Tick ms value: %lu can't be truncated. Returning garbage.\n", now_ms);
    }
    return (int32_t)now_ms;
}

static void bus_delay(uint32_t ms) { HAL_Delay(ms); }

/* ====================== STUDENT CODE END — IO glue ====================== */

int32_t vitals_bus_init(void) {
    /* =================== STUDENT CODE BEGIN — binding =================== */

    /* TODO: steps 2a–2e from the header comment, for the two sensors.
     * Erase the two lines below when you start. */
    printf("vitals_bus_init: NOT IMPLEMENTED (see App/vitals_bus.c)\n");
    return -1;

    /* ==================== STUDENT CODE END — binding ==================== */
}
