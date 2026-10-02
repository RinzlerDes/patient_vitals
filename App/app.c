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

#include "clock.h"
#include "config_table.h"
#include "console.h"
#include "ism330dhcx.h"
#include "main.h"
#include "oled.h"
#include "qualify.h"
#include "rgb_led.h"
#include "stts22h.h"
#include "ui_pages.h"
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

typedef enum { ALERT_NONE, ALERT_TURN_DUE, ALERT_HOB_HIGH, ALERT_TEMP_RISE } alert_type_t;

typedef struct {
    float* samples;
    uint32_t sample_size;
    uint32_t index;
    uint32_t count;
} smoother_t;

/* EXTI press counters. The ISR writes them, and the loop reads them (R23). */
static volatile uint32_t b1_presses, b2_presses, imu_int1_events;

static bool ts1_is_touched = false;

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

static bool HOB_is_high = false;

static float angle_samples[4] = { 0 };
static float temp_samples[10] = { 0 };

static smoother_t angle_smoother = { .samples = angle_samples, .sample_size = 4 };

static smoother_t temp_smoother = { .samples = temp_samples, .sample_size = 10 };
static bool temp_smoothed_baseline_ready = false;
static float temp_smoothed_baseline = 0;
static float temp_smoothed = 0;
static qualify_t temp_qualify;
static bool temp_is_high = false;

// static bool turn_due_ack = false;
// static bool hob_ack = false;
// static bool temp_ack = false;

// static uint32_t turn_due_alert_start_ms = 0;
// static uint32_t hob_alert_start_ms = 0;
// static uint32_t temp_alert_start_ms = 0;

// static uint32_t turn_due_ack_ms = 0;
// static uint32_t hob_ack_ms = 0;
// static uint32_t temp_ack_ms = 0;

typedef struct {
    uint32_t start_ms;
    uint32_t ack_ms;
    bool acknowledged;
} alert_t;

static alert_t turn_due_alert;
static alert_t hob_high_alert;
static alert_t temp_rise_alert;

static bool b1_pressed = false;
static bool b2_pressed = false;

static uint32_t mode_start_ms = 0;
static uint32_t session_event_count = 0;
static uint32_t session_alert_count = 0;

typedef enum { POSTURE_SUPINE, POSTURE_LEFT_30, POSTURE_RIGHT_30, POSTURE_COUNT } posture_t;

static float lateral_angle_samples[4] = { 0 };
static smoother_t lateral_angle_smoother = { .samples = lateral_angle_samples, .sample_size = 4 };
static float lateral_angle_smoothed = 0.0f;

static posture_t last_posture = POSTURE_SUPINE;
static posture_t current_posture = POSTURE_SUPINE;
static uint32_t posture_time_ms[POSTURE_COUNT] = { 0 };
static uint32_t posture_start_ms = 0;

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

static void uint32_split_ms(uint32_t value_ms, uint32_t* out_whole, uint32_t* out_fraction) {
    *out_whole = value_ms / 1000;
    *out_fraction = value_ms % 1000;
}

static bool temp_start_one_shot(uint32_t now) {
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

static void alert_start(alert_t* alert, uint32_t now) {
    alert->start_ms = now;
    alert->ack_ms = 0;
    alert->acknowledged = false;

    session_alert_count++;
}

static void alert_clear(alert_t* alert) {
    alert->start_ms = 0;
    alert->ack_ms = 0;
    alert->acknowledged = false;
}

static void alert_acknowledge(alert_t* alert, uint32_t now) {
    alert->acknowledged = true;
    alert->ack_ms = now;
}

static alert_type_t alert_highest_priority(void) {
    if (turn_due && !turn_due_alert.acknowledged) {
        return ALERT_TURN_DUE;
    }

    if (HOB_is_high && !hob_high_alert.acknowledged) {
        return ALERT_HOB_HIGH;
    }

    if (temp_is_high && !temp_rise_alert.acknowledged) {
        return ALERT_TEMP_RISE;
    }

    return ALERT_NONE;
}

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
    /* ISR rule (R23): latch the event and return. No I2C, no printf, and
     * no OLED work here. */
    if (pin == User_B1_Pin) {
        b1_presses++;
        // print_debug_flag = true;
        b1_pressed = true;
    }
    if (pin == User_B2_Pin) {
        b2_presses++;
        b2_pressed = true;
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

static void render_live(void) {
    char value[16];
    uint32_t whole;
    uint32_t fraction;
    bool positive;

    ui_row_put(1, "MODE", device_mode == MONITOR ? "MONITOR" : "STANDBY");

    ui_row_put(2, "PRESENT", ts1_is_touched ? "YES" : "NO");

    float_split(angle_calibrated, 1, &positive, &whole, &fraction);
    snprintf(value, sizeof(value), "%s%lu.%lu deg", positive ? "" : "-", whole, fraction);
    ui_row_put(3, "ANGLE", value);

    float_split(temp_smoothed, 1, &positive, &whole, &fraction);
    snprintf(value, sizeof(value), "%s%lu.%lu C", positive ? "" : "-", whole, fraction);
    ui_row_put(4, "TEMP", value);

    if (temp_smoothed_baseline_ready) {
        float_split(temp_smoothed_baseline, 1, &positive, &whole, &fraction);
        snprintf(value, sizeof(value), "%s%lu.%lu C", positive ? "" : "-", whole, fraction);
    } else {
        snprintf(value, sizeof(value), "--");
    }

    ui_row_put(5, "BASELINE", value);

    snprintf(value,
             sizeof(value),
             "%ld.%ld deg",
             cfg_hob_limit_tenths / 10,
             cfg_hob_limit_tenths % 10);
    ui_row_put(6, "HOB LIM", value);

    snprintf(value,
             sizeof(value),
             "%ld.%ld C",
             (long)(cfg_delta_t_tenths / 10),
             (long)(cfg_delta_t_tenths % 10));
    ui_row_put(7, "TEMP LIM", value);
}

static void render_clocks(void) {
    char value[16];
    uint32_t now = HAL_GetTick();

    uint32_t turn_seconds = 0;

    if (device_mode == MONITOR) {
        turn_seconds = (now - last_reposition_ms) / 1000;
    }

    snprintf(value, sizeof(value), "%02lu:%02lu", turn_seconds / 60, turn_seconds % 60);
    ui_row_put(1, "TURN", value);

    snprintf(value, sizeof(value), "%ld s", (long)cfg_turn_interval_s);
    ui_row_put(2, "INTERVAL", value);

    uint32_t state_seconds = (now - mode_start_ms) / 1000;

    snprintf(value, sizeof(value), "%02lu:%02lu", state_seconds / 60, state_seconds % 60);
    ui_row_put(3, "IN STATE", value);

    ui_row_put(4, "MODE", device_mode == MONITOR ? "MONITOR" : "STANDBY");

    ui_row_put(5, "TURN DUE", turn_due ? "YES" : "NO");
}

static void render_session(void) {
    char value[16];

    snprintf(value, sizeof(value), "%lu", session_event_count);
    ui_row_put(1, "EVENTS", value);

    snprintf(value, sizeof(value), "%lu", session_alert_count);
    ui_row_put(2, "ALERTS", value);

    ui_row_put(3, "TURN", turn_due ? (turn_due_alert.acknowledged ? "ACK" : "ACTIVE") : "OFF");

    ui_row_put(4, "HOB", HOB_is_high ? (hob_high_alert.acknowledged ? "ACK" : "ACTIVE") : "OFF");

    ui_row_put(5, "TEMP", temp_is_high ? (temp_rise_alert.acknowledged ? "ACK" : "ACTIVE") : "OFF");
}

static void service_ui_banner(void) {
    static alert_type_t previous_alert = ALERT_NONE;

    alert_type_t alert = alert_highest_priority();

    if (alert == previous_alert) {
        return;
    }

    previous_alert = alert;

    switch (alert) {
        case ALERT_TURN_DUE:
            ui_pages_banner("TURN-DUE");
            break;

        case ALERT_HOB_HIGH:
            ui_pages_banner("HOB-HIGH");
            break;

        case ALERT_TEMP_RISE:
            ui_pages_banner("TEMP-RISE");
            break;

        case ALERT_NONE:
            ui_pages_banner(NULL);
            break;
    }
}

void app_init(void) {
    console_init();
    oled_init();

    ui_pages_init();

    ui_pages_register(UI_PAGE_LIVE, "LIVE", render_live);
    ui_pages_register(UI_PAGE_CLOCKS, "CLOCKS", render_clocks);
    ui_pages_register(UI_PAGE_SESSION, "SESSION", render_session);

    dwt_init();
    rgb_init();

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

    printf("Thresholds:\n");

    for (int i = 0; i < config_table_count(); i++) {
        const config_entry_t* entry = config_table_entry(i);
        char value[16];

        config_table_format(entry, value, sizeof(value));

        printf("  %s = %s %s\n", entry->name, value, entry->unit);
    }
}

posture_t posture_classify(float lateral_angle) {
    if (lateral_angle >= 20.0f && lateral_angle <= 40.0f) {
        return POSTURE_RIGHT_30;
    }

    if (lateral_angle <= -20.0f && lateral_angle >= -40.0f) {
        return POSTURE_LEFT_30;
    }

    return POSTURE_SUPINE;
}

static const char* posture_name(posture_t posture) {
    switch (posture) {
        case POSTURE_SUPINE:
            return "SUPINE";

        case POSTURE_LEFT_30:
            return "LEFT-30";

        case POSTURE_RIGHT_30:
            return "RIGHT-30";

        default:
            return "UNKNOWN";
    }
}

static void posture_time_reset() {
    for (uint32_t i = 0; i < POSTURE_COUNT; i++) {
        posture_time_ms[i] = 0;
    }
}

static bool temp_handle_one_shot(float* out_temp) {
    uint8_t drdy_status = 0;
    int32_t status = STTS22H_TEMP_Get_DRDY_Status(&temp_sensor, &drdy_status);

    if (status != STTS22H_OK) {
        printf("Failed to get temp drdy status\n");
        return false;
    } else if (drdy_status != 1) {
        // temp conversion not ready
        return false;
    }

    // float val = 0;
    // uint32_t temp_whole = 0;
    // uint32_t temp_fraction = 0;
    // bool temp_is_positive = true;

    status = STTS22H_TEMP_GetTemperature(&temp_sensor, out_temp);
    if (status != STTS22H_OK) {
        printf("get temp failed\n");
        return false;
    }

    // float_split(val, 1, &temp_is_positive, &temp_whole, &temp_fraction);
    // char* sign = temp_is_positive ? "" : "-";
    // printf("temp: %s%lu.%lu c\n", sign, temp_whole, temp_fraction);

    // *out_temp = val;

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
    session_event_count++;
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
                temp_smoothed_baseline_ready = false;
                temp_smoother.count = 0;
                temp_smoother.index = 0;
                temp_is_high = false;
                mode_start_ms = now;
                session_event_count = 0;
                session_alert_count = 0;
                current_posture = posture_classify(lateral_angle_smoothed);
                last_posture = current_posture;
                posture_time_reset();
                posture_start_ms = now;
                qualify_init(&temp_qualify, cfg_delta_t_tenths, 0, 0, now);
                log_event(now, "Device mode: Standby -> Monitor\n");
            }
            break;
        case MONITOR:
            if (!ts1_is_touched) {
                if (turn_due) {
                    log_event(now, "TURN-DUE cancelled\n");
                }

                if (HOB_is_high) {
                    log_event(now, "HOB-HIGH cancelled\n");
                }

                if (temp_is_high) {
                    log_event(now, "TEMP-RISE cancelled\n");
                }

                turn_due = false;
                HOB_is_high = false;
                temp_is_high = false;

                alert_clear(&turn_due_alert);
                alert_clear(&hob_high_alert);
                alert_clear(&temp_rise_alert);

                device_mode = STANDBY;
                mode_start_ms = now;

                log_event(now, "Device mode: Monitor -> Standby\n");
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

// static float smooth_angle(float new_angle) {
//     enum { SAMPLE_SIZE = 4 };
//     static float samples[SAMPLE_SIZE] = { 0 };
//     static uint32_t index = 0;
//     static uint32_t count = 0;

//     samples[index] = new_angle;
//     index = (index + 1) % SAMPLE_SIZE;

//     if (count < SAMPLE_SIZE) {
//         count++;
//     }

//     float sum = 0;

//     for (uint32_t i = 0; i < count; i++) {
//         sum += samples[i];
//     }

//     return sum / count;
// }

static const char* alert_state(bool condition, const alert_t* alert) {
    if (!condition) {
        return "OFF";
    }

    if (alert->acknowledged) {
        return "ACK";
    }

    return "ACTIVE";
}

static void print_status(uint32_t now) {
    uint32_t whole;
    uint32_t fraction;
    bool positive;

    char angle[16];
    char temp[16];
    char baseline[16];

    float_split(angle_calibrated, 1, &positive, &whole, &fraction);
    snprintf(angle, sizeof(angle), "%s%lu.%lu", positive ? "" : "-", whole, fraction);

    float_split(temp_smoothed, 1, &positive, &whole, &fraction);
    snprintf(temp, sizeof(temp), "%s%lu.%lu", positive ? "" : "-", whole, fraction);

    if (temp_smoothed_baseline_ready) {
        float_split(temp_smoothed_baseline, 1, &positive, &whole, &fraction);

        snprintf(baseline, sizeof(baseline), "%s%lu.%lu", positive ? "" : "-", whole, fraction);
    } else {
        snprintf(baseline, sizeof(baseline), "--");
    }

    uint32_t turn_elapsed_s = 0;

    if (device_mode == MONITOR) {
        turn_elapsed_s = (now - last_reposition_ms) / 1000;
    }

    printf("\n--- STATUS ---\n");

    printf("Mode: %s\n", device_mode == MONITOR ? "MONITOR" : "STANDBY");

    printf("Presence: %s\n", ts1_is_touched ? "PRESENT" : "ABSENT");

    printf("Angle: %s deg  Limit: %ld.%ld deg\n",
           angle,
           cfg_hob_limit_tenths / 10,
           cfg_hob_limit_tenths % 10);

    printf("Temp: %s C  Baseline: %s C  Delta limit: %ld.%ld C\n",
           temp,
           baseline,
           cfg_delta_t_tenths / 10,
           cfg_delta_t_tenths % 10);

    printf("Turn: %02lu:%02lu  Interval: %ld s\n",
           turn_elapsed_s / 60,
           turn_elapsed_s % 60,
           cfg_turn_interval_s);

    printf("Alerts: TURN=%s HOB=%s TEMP=%s\n",
           alert_state(turn_due, &turn_due_alert),
           alert_state(HOB_is_high, &hob_high_alert),
           alert_state(temp_is_high, &temp_rise_alert));

    printf("Session events: %lu\n", session_event_count);

    uint32_t occupancy[POSTURE_COUNT];

    for (uint32_t i = 0; i < POSTURE_COUNT; i++) {
        occupancy[i] = posture_time_ms[i];
    }

    if (device_mode == MONITOR) {
        occupancy[current_posture] += now - posture_start_ms;
    }

    uint32_t total_ms =
        occupancy[POSTURE_SUPINE] + occupancy[POSTURE_LEFT_30] + occupancy[POSTURE_RIGHT_30];

    uint32_t supine_pct = 0;
    uint32_t left_pct = 0;
    uint32_t right_pct = 0;

    if (total_ms > 0) {
        supine_pct = occupancy[POSTURE_SUPINE] * 100u / total_ms;
        left_pct = occupancy[POSTURE_LEFT_30] * 100u / total_ms;
        right_pct = occupancy[POSTURE_RIGHT_30] * 100u / total_ms;
    }

    printf("Posture: %s\n", posture_name(current_posture));

    printf("Occupancy: SUPINE=%lu%% LEFT-30=%lu%% RIGHT-30=%lu%%\n",
           supine_pct,
           left_pct,
           right_pct);

    printf("--------------\n");
}

static void console_handle_custom_command(uint32_t now, char* str) {
    if (strcmp(str, "CAL") == 0) {
        calibration_offset = angle_smoothed;
        angle_calibrated = 0;

        log_event(now, "Angle calibrated\n");
    } else if (strcmp(str, "STATUS") == 0) {
        print_status(now);
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

        const config_entry_t* set_entry = NULL;

        config_cmd_result_t result = config_table_command(cmd, &set_entry);

        if (result == CFG_CMD_SET_OK) {
            char value[16];
            char message[64];

            config_table_format(set_entry, value, sizeof(value));

            snprintf(message,
                     sizeof(message),
                     "CONFIG %s %s %s\n",
                     set_entry->name,
                     value,
                     set_entry->unit);

            log_event(now, message);
        }

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

    posture_t new_posture = posture_classify(lateral_angle_smoothed);

    if (new_posture == last_posture) {
        transition_started = false;
        return;
    }

    // if (angle_delta < required_delta) {
    //     transition_started = false;
    //     return;
    // }

    if (!transition_started) {
        transition_started = true;
        transition_start = now;
        return;
    }

    if ((now - transition_start) >= (cfg_t_hold_s * 1000)) {
        last_reposition_ms = now;
        angle_at_last_reset = angle_calibrated;
        transition_started = false;

        posture_time_ms[current_posture] += now - posture_start_ms;

        current_posture = new_posture;
        last_posture = current_posture;
        posture_start_ms = now;

        char message[64];

        snprintf(message,
                 sizeof(message),
                 "POSTURE-CHANGE %s turn clock reset\n",
                 posture_name(current_posture));

        log_event(now, message);

        // log_event(now, "POSTURE-CHANGE turn clock reset\n");

        if (turn_due) {
            turn_due = false;
            alert_clear(&turn_due_alert);
            log_event(now, "TURN-DUE cleared after posture change\n");
        }
    }
}

static void service_turn_due(uint32_t now) {
    if (!turn_due && (now - last_reposition_ms) >= (cfg_turn_interval_s * 1000)) {
        turn_due = true;

        alert_start(&turn_due_alert, now);

        log_event(now, "TURN-DUE\n");
    }
}

static void service_head_of_bed_high(uint32_t now) {
    static uint32_t transition_start = 0;
    static bool transition_started = false;
    float HOB_limit = cfg_hob_limit_tenths / 10.0f;

    if (HOB_is_high) {
        if (angle_calibrated < (HOB_limit - 2.0f)) {
            HOB_is_high = false;
            transition_started = false;
            alert_clear(&hob_high_alert);
            log_event(now, "HOB-LOW\n");
        }

        return;
    }

    if (angle_calibrated < HOB_limit) {
        transition_started = false;
        return;
    }

    if (!transition_started) {
        transition_started = true;
        transition_start = now;
        return;
    }

    if ((now - transition_start) > (cfg_t_grace_s * 1000)) {
        HOB_is_high = true;
        transition_started = false;

        alert_start(&hob_high_alert, now);

        log_event(now, "HOB-HIGH\n");
    }
}

static float float_smooth(smoother_t* smoother, float new_value) {
    smoother->samples[smoother->index] = new_value;
    smoother->index = (smoother->index + 1) % smoother->sample_size;

    if (smoother->count < smoother->sample_size) {
        smoother->count++;
    }

    float sum = 0;

    for (uint32_t i = 0; i < smoother->count; i++) {
        sum += smoother->samples[i];
    }

    return sum / smoother->count;
}

// use qualify
static void service_temp_rise(uint32_t now, float temp_current) {
    int32_t temp_delta = lroundf((temp_current - temp_smoothed_baseline) * 10);
    // qualify_t qualify_temp_rise;
    // qualify_init(&qualify_temp_rise, cfg_delta_t_tenths, 0, 0, now);
    qualify_event_t event = qualify_feed(&temp_qualify, temp_delta, now);
    switch (event) {
        // case QUALIFY_SET:
        //     temp_is_high = true;

        //     alert_start(&temp_rise_alert, now);

        //     log_event(now, "TEMP-RISE\n");
        //     break;
        case QUALIFY_SET: {
            temp_is_high = true;

            alert_start(&temp_rise_alert, now);

            uint32_t current_whole;
            uint32_t current_fraction;
            bool current_positive;

            uint32_t baseline_whole;
            uint32_t baseline_fraction;
            bool baseline_positive;

            float_split(temp_current, 1, &current_positive, &current_whole, &current_fraction);

            float_split(temp_smoothed_baseline,
                        1,
                        &baseline_positive,
                        &baseline_whole,
                        &baseline_fraction);

            char message[96];

            snprintf(message,
                     sizeof(message),
                     "TEMP-RISE trigger %s%lu.%lu C baseline %s%lu.%lu C\n",
                     current_positive ? "" : "-",
                     current_whole,
                     current_fraction,
                     baseline_positive ? "" : "-",
                     baseline_whole,
                     baseline_fraction);

            log_event(now, message);
            break;
        }
        case QUALIFY_CLEAR:
            temp_is_high = false;
            alert_clear(&temp_rise_alert);
            log_event(now, "TEMP-RISE CLEARED\n");
            break;
        case QUALIFY_NONE:
            // nothing
            break;
        default:
            break;
    }
}

// static void service_rgb_led(uint32_t now) {
//     if (device_mode == STANDBY) {
//         rgb_set(5, 0, 0);
//         return;
//     }
//     if (device_mode == MONITOR) {
//         rgb_set(0, 5, 0);
//     }
// }

static void service_rgb_led(void) {
    typedef enum {
        RGB_STATE_NONE,
        RGB_STATE_STANDBY,
        RGB_STATE_MONITOR,
        RGB_STATE_TEMP,
        RGB_STATE_HOB,
        RGB_STATE_TURN
    } rgb_state_t;

    static rgb_state_t prev_state = RGB_STATE_NONE;
    rgb_state_t new_state;

    if (device_mode == STANDBY) {
        new_state = RGB_STATE_STANDBY;
    } else {
        switch (alert_highest_priority()) {
            case ALERT_TURN_DUE:
                new_state = RGB_STATE_TURN;
                break;

            case ALERT_HOB_HIGH:
                new_state = RGB_STATE_HOB;
                break;

            case ALERT_TEMP_RISE:
                new_state = RGB_STATE_TEMP;
                break;

            case ALERT_NONE:
                new_state = RGB_STATE_MONITOR;
                break;
        }
    }

    if (new_state == prev_state) {
        return;
    }

    prev_state = new_state;

    switch (new_state) {
        case RGB_STATE_STANDBY:
            rgb_set(5, 0, 0);
            break;

        case RGB_STATE_MONITOR:
            rgb_set(0, 5, 0);
            break;

        case RGB_STATE_TURN:
            rgb_set(0, 0, 5);
            break;

        case RGB_STATE_HOB:
            rgb_set(5, 5, 0);
            break;

        case RGB_STATE_TEMP:
            rgb_set(5, 0, 5);
            break;

        default:
            break;
    }
}

static void service_alert_acknowledge(uint32_t now) {
    alert_type_t alert_type = alert_highest_priority();
    alert_t* alert;
    char* name;

    switch (alert_type) {
        case ALERT_TURN_DUE:
            // alert_acknowledge(&turn_due_alert, now);
            alert = &turn_due_alert;
            name = "TURN-DUE";
            break;
        case ALERT_HOB_HIGH:
            // alert_acknowledge(&hob_high_alert, now);
            alert = &hob_high_alert;
            name = "HOB-HIGH";
            break;

        case ALERT_TEMP_RISE:
            // alert_acknowledge(&temp_rise_alert, now);
            alert = &temp_rise_alert;
            name = "TEMP-RISE";
            break;

        case ALERT_NONE:
            return;
            break;
    }

    alert_acknowledge(alert, now);

    uint32_t delta = now - alert->start_ms;
    uint32_t whole;
    uint32_t fraction;

    uint32_split_ms(delta, &whole, &fraction);

    char message[64];

    snprintf(message,
             sizeof(message),
             "Acknowledgement %s overdue %lu.%lu\n",
             name,
             whole,
             fraction);

    log_event(now, message);
}

static void check_alert_rearm(alert_t* alert, bool condition, uint32_t now, char* log_message) {
    bool passed_alert_rearm_threshold = (now - alert->ack_ms) > (cfg_rearm_s * 1000);

    if (alert->acknowledged && condition && passed_alert_rearm_threshold) {
        alert->acknowledged = false;
        log_event(now, log_message);
    }
}

static void service_alert_rearm(uint32_t now) {
    check_alert_rearm(&turn_due_alert, turn_due, now, "TURN-DUE rearmed\n");
    check_alert_rearm(&hob_high_alert, HOB_is_high, now, "HOB-HIGH rearmed\n");
    check_alert_rearm(&temp_rise_alert, temp_is_high, now, "TEMP-RISE rearmed\n");
}

void app_service(void) {
    static uint32_t touch_last, status_last;
    static int32_t touch_raw = -1;
    uint32_t now = HAL_GetTick();
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
    if (!temp_one_shot_active) {
        temp_one_shot_active = temp_start_one_shot(now);
    } else {
        float temp_raw = 0;
        if (temp_handle_one_shot(&temp_raw)) {
            temp_one_shot_active = false;
            temp_smoothed = float_smooth(&temp_smoother, temp_raw);
            if (device_mode == MONITOR && !temp_smoothed_baseline_ready &&
                temp_smoother.count == temp_smoother.sample_size) {
                temp_smoothed_baseline_ready = true;
                temp_smoothed_baseline = temp_smoothed;
                // log_event(now, "Temperature baseline ready\n");
                uint32_t whole;
                uint32_t fraction;
                bool positive;
                char message[64];

                float_split(temp_smoothed_baseline, 1, &positive, &whole, &fraction);

                snprintf(message,
                         sizeof(message),
                         "Temperature baseline %s%lu.%lu C\n",
                         positive ? "" : "-",
                         whole,
                         fraction);

                log_event(now, message);
            }
        }
    }

    if (imu_axes_ready) {
        success = imu_read(&imu_axes);
        imu_axes_ready = false;
        if (success) {
            float angle_raw = calc_angle(&imu_axes);
            // angle_smoothed = smooth_angle(angle_raw);
            angle_smoothed = float_smooth(&angle_smoother, angle_raw);
            angle_calibrated = angle_smoothed - calibration_offset;

            float lateral_raw = atan2f(imu_axes.y, imu_axes.z) * 180.0f / 3.14159265f;
            lateral_angle_smoothed = float_smooth(&lateral_angle_smoother, lateral_raw);
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
        service_head_of_bed_high(now);
        if (temp_smoothed_baseline_ready) {
            service_temp_rise(now, temp_smoothed);
        }
        service_alert_rearm(now);
    }

    service_rgb_led();

    if (b1_pressed) {
        b1_pressed = false;
        service_alert_acknowledge(now);
    }

    if (b2_pressed) {
        b2_pressed = false;
        ui_pages_next();
    }

    service_ui_banner();

    static uint32_t ui_last = 0;

    if ((now - ui_last) >= 500) {
        ui_last = now;
        ui_pages_mark_dirty();
    }

    ui_pages_service();

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
