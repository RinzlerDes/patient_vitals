/**
 ******************************************************************************
 * @file    qualify.c
 * @brief   Generic alert qualifier — threshold + sustain + hysteresis.
 *          PROVIDED MODULE — do not modify. See qualify.h for the contract.
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *
 *  Read it anyway. This module has the same shape that you build by hand
 *  for the posture-change detector (R9). It does not have the candidate
 *  reset of R9, which is the part that D6 grades. Elapsed-time comparisons
 *  use the unsigned-subtraction idiom, thus a HAL-tick wrap (49.7 days)
 *  cannot cause a false verdict.
 ******************************************************************************
 */
#include "qualify.h"

void qualify_init(qualify_t *q, int32_t threshold, uint32_t sustain_ms,
                  int32_t hysteresis, uint32_t now_ms)
{
    q->threshold  = threshold;
    q->hysteresis = hysteresis;
    q->sustain_ms = sustain_ms;
    q->t_cross    = now_ms;
    q->pending    = false;
    q->active     = false;
}

void qualify_config(qualify_t *q, int32_t threshold, uint32_t sustain_ms,
                    int32_t hysteresis)
{
    q->threshold  = threshold;
    q->hysteresis = hysteresis;
    q->sustain_ms = sustain_ms;
    /* The latched state stays unchanged by design. The next
     * qualify_feed() call examines a runtime limit change (R12), thus no
     * false state change occurs here. */
}

qualify_event_t qualify_feed(qualify_t *q, int32_t value, uint32_t now_ms)
{
    if (!q->active) {
        if (value >= q->threshold) {
            if (!q->pending) {                 /* condition went true here  */
                q->pending = true;
                q->t_cross = now_ms;
            } else if ((uint32_t)(now_ms - q->t_cross) >= q->sustain_ms) {
                q->pending = false;            /* sustained: qualify + latch */
                q->active  = true;
                return QUALIFY_SET;
            }
        } else {
            q->pending = false;                /* transient — no memory of it */
        }
    } else {
        if (value < q->threshold - q->hysteresis) {
            q->active  = false;                /* clear through the band     */
            q->pending = false;
            return QUALIFY_CLEAR;
        }
    }
    return QUALIFY_NONE;
}

bool qualify_active(const qualify_t *q)
{
    return q->active;
}
