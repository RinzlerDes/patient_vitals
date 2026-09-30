/**
 ******************************************************************************
 * @file    app.c
 * @brief   Starter smoke test. REPLACE the body of this module with your
 *          DG-30 code (spec §3/§4).
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *
 *  What this smoke test does, and all that it does:
 *   1. OLED banner and console banner (the provided HAL-port drivers).
 *   2. Scans I2C3 with the raw HAL_I2C_IsDeviceReady() call and reports
 *      the two sensors by address. This shows that the BUS works before
 *      you write one line of the IO layer. Your POST does more: it reads
 *      WHO_AM_I through the component drivers (spec R3,
 *      App/vitals_bus.c).
 *   3. Acquires the TS1 touch key about 10 times each second and shows
 *      the raw TSC counts. Touch the pad, and see the number DROP. This
 *      acquisition sequence is the worked example for your presence gate
 *      (R1). The hysteresis and the presence rules stay yours.
 *   4. Counts B1 and B2 presses through HAL_GPIO_EXTI_Callback (the
 *      worked example for R17: latch the event in the ISR, use it in the
 *      loop, and do no heavy work here).
 *   5. Calls vitals_bus_init() one time at boot. As shipped, that call
 *      prints NOT IMPLEMENTED. Your first task removes that line.
 *
 *  Flash it without a change first. You then get the banner on the OLED
 *  and in PuTTY, and "found 0x38 0x6B" on the console. The raw touch
 *  counts move when you touch the pad, and the button counts increase.
 *  Then start to replace the smoke test.
 *
 *  All the code here obeys the loop discipline that the rubric grades.
 *  app_service() does not block. Each periodic step runs on a
 *  HAL_GetTick() elapsed-time check, which is the P0 and P1 idiom, and
 *  HAL does not change it.
 ******************************************************************************
 */
#include "app.h"

#include <stdbool.h>
#include <stdio.h>

#include "config_table.h"
#include "console.h"
#include "ism330dhcx.h"
#include "main.h"
#include "oled.h"
#include "stts22h.h"
#include "vitals_bus.h"

extern I2C_HandleTypeDef hi2c3; /* CubeMX-generated handles      */
extern TSC_HandleTypeDef htsc;

/* Sensor 7-bit addresses as this board straps them (UM2825 tbl 11), in the
 * left-shifted HAL notation (addr << 1). The macros in the component
 * headers, STTS22H_I2C_ADD_H (0x71) and ISM330DHCX_I2C_ADD_H (0xD7), are
 * the same strappings with bit 0 (the R/W bit) set. The I2C peripheral
 * ignores bit 0 in 7-bit mode, thus each notation addresses the same
 * device.                                                              */
#define ADDR_STTS22H    (0x38u << 1)
#define ADDR_ISM330DHCX (0x6Bu << 1)

#define TOUCH_PERIOD_MS  100u
#define STATUS_PERIOD_MS 500u
#define TEMP_PERIOD_MS   1000u

typedef enum { STANDBY, MONITOR } device_mode_t;

/* EXTI press counters. The ISR writes them, and the loop reads them (R23). */
static volatile uint32_t b1_presses, b2_presses, imu_int1_events;
static device_mode_t device_mode = STANDBY;
// static uint32_t monitor_start_ms = 0;
static bool sensors_ready = false;
static volatile bool imu_axes_ready = false;
static volatile bool print_debug_flag = false;

static float calibration_offset = 0;
// static float angle_raw = 0;
static float angle_smoothed = 0;
static float angle_calibrated = 0;

static uint32_t last_reposition_ms = 0;
static float angle_at_last_reset = 0;
static bool turn_due = false;

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
    /* ISR rule (R23): latch the event and return. No I2C, no printf, and
     * no OLED work here. */
    if (pin == User_B1_Pin) {
        b1_presses++;
        print_debug_flag = true;
    }
    if (pin == User_B2_Pin) {
        b2_presses++;
    }
    if (pin == INT1_Pin) {
        imu_int1_events++;
        imu_axes_ready = true;
    }
    // if (pin == DRDY_Pin) {
    //     temp_drdy_flag = true;
    // }
}

/*
 * Non-blocking TSC acquisition of the TS1 key (group 6, and group 4 is
 * the shield electrode). Call it in each loop cycle. It returns the new
 * raw charge-transfer count after an acquisition ends, or -1. A lower
 * count means a finger on the pad. The finger adds capacitance, thus
 * fewer transfer cycles fill the sampling capacitor. Measure YOUR pad
 * and see this effect (R1).
 *
 * The function has three phases, and no phase waits. It discharges the
 * electrodes (>=1 ms), starts the acquisition, then polls until the
 * acquisition ends. This is the R1 worked example, and your presence
 * gate adds hysteresis above it.
 */
static int32_t touch_read_raw(void) {
    static enum { T_IDLE, T_DISCHARGE, T_ACQUIRE } phase = T_IDLE;
    static uint32_t t_phase;
    uint32_t now = HAL_GetTick();

    switch (phase) {
        case T_IDLE:
            HAL_TSC_IODischarge(&htsc, ENABLE);
            t_phase = now;
            phase = T_DISCHARGE;
            return -1;

        case T_DISCHARGE:
            if ((uint32_t)(now - t_phase) < 2u) {
                return -1; /* let the electrodes drain      */
            }
            HAL_TSC_IODischarge(&htsc, DISABLE);
            HAL_TSC_Start(&htsc);
            phase = T_ACQUIRE;
            return -1;

        case T_ACQUIRE:
        default:
            /* A max-count error (MCE) stops the acquisition and does NOT set
             * the group-complete flag. Code that polls only for the end of an
             * acquisition stops here forever after one such error. Your R1
             * presence gate must also survive a stopped acquisition.      */
            if (__HAL_TSC_GET_FLAG(&htsc, TSC_FLAG_MCE)) {
                HAL_TSC_Stop(&htsc); /* clears EOA/MCE, back to READY */
                phase = T_IDLE;
                return -1;
            }
            if (HAL_TSC_GroupGetStatus(&htsc, TSC_GROUP6_IDX) != TSC_GROUP_COMPLETED) {
                return -1; /* still counting: come back     */
            }
            {
                int32_t v = (int32_t)HAL_TSC_GroupGetValue(&htsc, TSC_GROUP6_IDX);
                HAL_TSC_Stop(&htsc);
                phase = T_IDLE;
                return v;
            }
    }
}

void app_init(void) {
    console_init();
    oled_init();

    printf("\nDG-30 DecuGuard -- P2 starter smoke test\n");
    printf("SWEN 563 / CMPE 663. Type: it echoes. B1/B2: counted.\n\n");

    oled_write_line(0, "DG-30  P2 STARTER");
    oled_write_line(2, "smoke test running");
    oled_write_line(7, "touch TS1 pad...");

    /* Raw bus proof: do the two sensors answer at all? (Your POST
     * identifies each one by WHO_AM_I through the component drivers.)   */
    printf("I2C3 scan:");
    if (HAL_I2C_IsDeviceReady(&hi2c3, ADDR_STTS22H, 2u, 10u) == HAL_OK) {
        printf("  STTS22H @0x38 OK");
    } else {
        printf("  STTS22H @0x38 MISSING");
    }
    if (HAL_I2C_IsDeviceReady(&hi2c3, ADDR_ISM330DHCX, 2u, 10u) == HAL_OK) {
        printf("  ISM330DHCX @0x6B OK");
    } else {
        printf("  ISM330DHCX @0x6B MISSING");
    }
    printf("\n");

    /* Your first task: make this call succeed (App/vitals_bus.c). */
    if (vitals_bus_init() != 0) {
        sensors_ready = false;
        printf("Failed component drivers bound: WHO_AM_I not verified\n");
    } else {
        sensors_ready = true;
        printf("component drivers bound: WHO_AM_I verified\n");
    }
}

void float_split(float input, uint32_t decimal_places, bool* out_is_positive, uint32_t* out_whole,
                 uint32_t* out_fraction) {
    if (input < 0.0f) {
        *out_is_positive = false;
        input = -input;
    } else {
        *out_is_positive = true;
    }

    uint32_t scale = 1;
    for (uint32_t i = 0; i < decimal_places; i++) {
        scale *= 10;
    }

    uint32_t scaled = (uint32_t)(input * scale + 0.5);

    *out_whole = scaled / scale;
    *out_fraction = scaled % scale;
}

static bool temp_start_one_shot_if_due(uint32_t now) {
    static uint32_t temp_prev_start_ms = 0;

    if ((now - temp_prev_start_ms) < TEMP_PERIOD_MS) {
        return false;
    }

    int32_t status = STTS22H_Set_One_Shot(&temp_sensor);
    if (status != STTS22H_OK) {
        printf("Failed to start temp one shot\n");
        return false;
    }

    temp_prev_start_ms = now;
    return true;
}

static bool temp_handle_one_shot(float* out_temp) {
    uint8_t drdy_status = 0;
    int32_t status = STTS22H_TEMP_Get_DRDY_Status(&temp_sensor, &drdy_status);

    if (status != STTS22H_OK) {
        printf("Failed to get temp drdy status\n");
        return false;
    } else if (drdy_status != 1) {
        return false;
    }

    float val = 0;
    uint32_t temp_whole = 0;
    uint32_t temp_fraction = 0;
    bool temp_is_positive = true;

    status = STTS22H_TEMP_GetTemperature(&temp_sensor, &val);
    if (status != STTS22H_OK) {
        printf("get temp failed\n");
        return false;
    }

    // float_split(val, 1, &temp_is_positive, &temp_whole, &temp_fraction);
    // char* sign = temp_is_positive ? "" : "-";
    // printf("temp: %s%lu.%lu c\n", sign, temp_whole, temp_fraction);

    *out_temp = val;

    return true;
}

bool ts1_touch_update(uint32_t now, int32_t val) {
    static const int32_t ts1_pressed_threshold = 2350;
    static const int32_t ts1_not_pressed_threshold = 2400;
    static bool present = false;
    static bool transition_started = false;
    static uint32_t transition_start = 0;
    bool is_transition;
    uint32_t required_transition_duration;

    if (present) {
        is_transition = val > ts1_not_pressed_threshold;
        required_transition_duration = cfg_absent_ms;
    } else {
        is_transition = val < ts1_pressed_threshold;
        required_transition_duration = cfg_present_ms;
    }

    if (!is_transition) {
        transition_started = false;
        return present;
    }

    if (!transition_started) {
        transition_started = true;
        transition_start = now;
        return present;
    }

    if ((now - transition_start) >= required_transition_duration) {
        present = !present;
        transition_started = false;
    }

    return present;
}

static void log_event(uint32_t now, const char* message) {
    printf("[%04lu.%03lu] %s", now / 1000, now % 1000, message);
}

static void service_device_mode(uint32_t now, bool ts1_is_touched) {
    switch (device_mode) {
        case STANDBY:
            if (ts1_is_touched && sensors_ready) {
                // monitor_start_ms = now;
                device_mode = MONITOR;
                last_reposition_ms = now;
                angle_at_last_reset = angle_calibrated;
                turn_due = false;
                log_event(now, "Device mode: Standby -> Monitor\n");
            }
            break;
        case MONITOR:
            if (!ts1_is_touched) {
                device_mode = STANDBY;
                log_event(now, "Device mode: Monitor -> Standby");
            }
            break;

        default:
            break;
    }
}

static bool imu_read(ISM330DHCX_Axes_t* out_axes) {
    if (!imu_axes_ready) {
        return false;
    }
    int32_t status = ISM330DHCX_ACC_GetAxes(&imu, out_axes);
    if (status != ISM330DHCX_OK) {
        printf("Failed imu acc get axes\n");
        return false;
    }

    return true;
}

static float calc_angle(const ISM330DHCX_Axes_t* axes) {
    return atan2f(-axes->x, axes->z) * 180.0f / 3.14159265f;
}

static float smooth_angle(float new_angle) {
    enum { SAMPLE_SIZE = 4 };
    static float samples[SAMPLE_SIZE] = { 0 };
    static uint32_t index = 0;
    static uint32_t count = 0;

    samples[index] = new_angle;
    index = (index + 1) % SAMPLE_SIZE;

    if (count < SAMPLE_SIZE) {
        count++;
    }

    float sum = 0;

    for (uint32_t i = 0; i < count; i++) {
        sum += samples[i];
    }

    return sum / count;
}

static void console_handle_custom_command(uint32_t now, char* str) {
    if (strcmp(str, "CAL") == 0) {
        calibration_offset = angle_smoothed;
        angle_calibrated = 0;

        log_event(now, "Angle calibrated\n");
    }
}

static void console_handle_line(uint32_t now) {
    enum { CMD_SIZE = 64 };
    static char cmd[CMD_SIZE];
    static uint32_t cmd_len = 0;

    int ch = console_poll();
    if (ch < 0) {
        return;
    }

    if (ch == '\r') {
        cmd[cmd_len] = '\0';
        printf("\n");
        // console_handle_line(now, cmd);
        config_cmd_result_t result = config_table_command(cmd, NULL);
        if (result == CFG_CMD_NOT_MINE) {
            console_handle_custom_command(now, cmd);
        }
        cmd_len = 0;
        cmd[0] = 0;
        return;
    }

    if (ch >= 0x20 && ch <= 0x7E) {
        if (cmd_len < CMD_SIZE - 1) {
            cmd[cmd_len] = ch;
            cmd_len++;
            putchar(ch);
        }
    }
}

static void service_posture_change(uint32_t now) {
    static bool transition_started = false;
    static uint32_t transition_start = 0;

    float angle_delta = fabsf(angle_calibrated - angle_at_last_reset);
    float required_delta = cfg_delta_turn_tenths / 10.0f;

    if (angle_delta < required_delta) {
        transition_started = false;
        return;
    }

    if (!transition_started) {
        transition_started = true;
        transition_start = now;
        return;
    }

    if ((now - transition_start) >= (cfg_t_hold_s * 1000)) {
        last_reposition_ms = now;
        angle_at_last_reset = angle_calibrated;
        transition_started = false;

        if (turn_due) {
            turn_due = false;
            log_event(now, "TURN-DUE cleared after posture change\n");
        }
    }
}

static void service_turn_due(uint32_t now) {
    if (!turn_due && (now - last_reposition_ms) >= (cfg_turn_interval_s * 1000)) {
        turn_due = true;
        log_event(now, "TURN-DUE\n");
    }
}

void app_service(void) {
    static uint32_t touch_last, status_last;
    static int32_t touch_raw = -1;
    uint32_t now = HAL_GetTick();
    static bool ts1_is_touched = false;
    static ISM330DHCX_Axes_t imu_axes;
    static bool success = false;

    // SENSOR WORK
    // ----------------------------------------------------------------------------------------------------
    /* Touch sampling — non-blocking, ~10 Hz (R1 groundwork). */
    if ((uint32_t)(now - touch_last) >= TOUCH_PERIOD_MS) {
        int32_t v = touch_read_raw();
        if (v >= 0) {
            touch_raw = v;
            touch_last = now;
            ts1_is_touched = ts1_touch_update(now, v);
        }
    }

    static bool temp_one_shot_active = false;
    static float temp_current = 0;
    if (!temp_one_shot_active) {
        temp_one_shot_active = temp_start_one_shot_if_due(now);
    } else {
        if (temp_handle_one_shot(&temp_current)) {
            temp_one_shot_active = false;
        }
    }

    if (imu_axes_ready) {
        success = imu_read(&imu_axes);
        imu_axes_ready = false;
        if (success) {
            float angle_raw = calc_angle(&imu_axes);
            angle_smoothed = smooth_angle(angle_raw);
            angle_calibrated = angle_smoothed - calibration_offset;
        }
        // if (!success)
    }
    // SENSOR WORK
    // ----------------------------------------------------------------------------------------------------

    console_handle_line(now);

    // /* Status line — on change cadence, cheap (R18 discipline). */
    // if ((uint32_t)(now - status_last) >= STATUS_PERIOD_MS) {
    //     status_last = now;
    //     oled_printf(4, "touch %5ld", (long)touch_raw);
    //     oled_printf(5, "B1 x%lu  B2 x%lu", (unsigned long)b1_presses, (unsigned
    //     long)b2_presses);
    // }

    // /* Console echo — the one non-blocking console call (R20). */
    // {
    //     int ch = console_poll();
    //     if (ch >= 0x20 && ch <= 0x7E) {
    //         putchar(ch);
    //         fflush(stdout);
    //     } else if (ch == '\r') {
    //         printf("\n");
    //     }
    // }

    // update_device_mode(now, ts1_is_touched);
    service_device_mode(now, ts1_is_touched);

    if (device_mode == MONITOR) {
        service_posture_change(now);
        service_turn_due(now);
    }

    // debug print every second
    static uint32_t prev_debug_print_ms = 0;
    // if ((now - prev_debug_print_ms) >= 1000) {
    if (print_debug_flag) {
        print_debug_flag = false;
        uint32_t whole = 0;
        uint32_t fraction = 0;
        bool is_positive = false;

        prev_debug_print_ms = now;
        printf("now: %lu\n", now);
        printf("ts1 is touched: %u\n", ts1_is_touched);
        printf("accel: x=%ld y=%ld z=%ld mg\n", imu_axes.x, imu_axes.y, imu_axes.z);

        float_split(angle_calibrated, 1, &is_positive, &whole, &fraction);

        printf("angle: %s%lu.%lu\n", is_positive ? "" : "-", whole, fraction);
    }
}
