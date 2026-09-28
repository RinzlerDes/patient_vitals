/**
 ******************************************************************************
 * @file    qualify.h
 * @brief   Generic alert qualifier — threshold + sustain + hysteresis with a
 *          latched state and transition events (provided).
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *          PROVIDED MODULE — do not modify.
 *
 *  You build this pattern by hand one time, for the repositioning clock
 *  (R8–R10). That instance is the graded one, and it SHALL NOT use this
 *  module (R9). Its candidate-based reset rules are different by design,
 *  and D6 grades exactly that difference.
 *
 *  The *supervisors* are HOB-HIGH (R11) and TEMP-RISE (R14). A third and
 *  a fourth hand-built copy teach nothing new, thus the course permits
 *  this module there. The graded content then becomes the correct
 *  content: the limits, the clear rules, and the logging.
 *
 *  Rules (one instance for each supervised condition):
 *      condition   value >= threshold             (units of the caller)
 *      SET         condition held continuously >= sustain_ms → latch active
 *      CLEAR       active && value < threshold - hysteresis  → release.
 *  One sample alone does not change the state (the R1/R9/R11 doctrine).
 *  All times come from HAL_GetTick(), the one timebase (§5.3). Wrap-safe.
 *
 *  Typical use (HOB-HIGH, tenths of a degree):
 *      qualify_t hob;
 *      qualify_init(&hob, cfg_hob_limit_tenths, cfg_t_grace_s * 1000u,
 *                   20, HAL_GetTick());               // 2.0° hysteresis
 *      ...each new smoothed sample, main-loop context:
 *      switch (qualify_feed(&hob, angle_tenths, HAL_GetTick())) {
 *      case QUALIFY_SET:   ... log the onset, raise the alert ... break;
 *      case QUALIFY_CLEAR: ... log the clear ...                  break;
 *      case QUALIFY_NONE:  break;
 *      }
 *  For TEMP-RISE, supply (current - baseline) against threshold = ΔT.
 *  The baseline capture itself is yours (R14).
 ******************************************************************************
 */
#ifndef QUALIFY_H
#define QUALIFY_H

#include <stdint.h>
#include <stdbool.h>

/** Transition event returned by qualify_feed(). */
typedef enum {
    QUALIFY_NONE = 0,  /**< no state change on this sample         */
    QUALIFY_SET,       /**< condition sustained: state latched      */
    QUALIFY_CLEAR      /**< condition cleared through hysteresis    */
} qualify_event_t;

/** Qualifier instance. Treat it as opaque, and read the state with
 *  qualify_active(). */
typedef struct {
    int32_t  threshold;   /**< assert level, units of the caller     */
    int32_t  hysteresis;  /**< clear at threshold - hysteresis       */
    uint32_t sustain_ms;  /**< how long condition must hold to SET   */
    uint32_t t_cross;     /**< tick when condition first went true   */
    bool     pending;     /**< above threshold, sustain timer runs   */
    bool     active;      /**< latched state                         */
} qualify_t;

/**
 * Initialize (or re-arm) an instance. The state becomes idle, with no
 * candidate pending. The threshold and the hysteresis take the units that
 * you supply, so keep the units the same everywhere. This project uses
 * fixed-point tenths. now_ms = HAL_GetTick().
 */
void qualify_init(qualify_t *q, int32_t threshold, uint32_t sustain_ms,
                  int32_t hysteresis, uint32_t now_ms);

/**
 * Change the limits at runtime (CONFIG changes, R12) and keep the latched
 * state. A supervisor must not make a false state change because a nurse
 * typed a limit again. The new limits take effect on the next sample.
 */
void qualify_config(qualify_t *q, int32_t threshold, uint32_t sustain_ms,
                    int32_t hysteresis);

/**
 * Supply one smoothed sample. Returns the transition event, which is
 * QUALIFY_NONE most of the time. Call it at your sampling rate, and from
 * main-loop context only.
 */
qualify_event_t qualify_feed(qualify_t *q, int32_t value, uint32_t now_ms);

/** Latched state: returns true while the condition stays active. */
bool qualify_active(const qualify_t *q);

#endif /* QUALIFY_H */
