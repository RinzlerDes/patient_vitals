/**
 ******************************************************************************
 * @file    config_table.h
 * @brief   Config-table console. It holds one table of {name, min, max,
 *          scale, pointer} and the generic SET and SHOW handlers.
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *          PROVIDED MODULE — do not modify.
 *
 *  Each §7.3 setting is one row in one table. This module writes SET and
 *  SHOW one time, against the table. That is the full idea: you add a
 *  setting with a new row, and not with a new command.
 *
 *  What stays yours (R19–R21, without a change):
 *      - assemble console input into a line WITHOUT a block. Use
 *        console_poll() in your loop (R20).
 *      - log each ACCEPTED setting as a timestamped [ssss.mmm] EVENT line
 *        (R19). This module reports the result, and it does not write your
 *        audit log.
 *      - print STATUS (R21). STATUS is a summary, and SHOW is not STATUS.
 *
 *  The settings live here (cfg_* below, demo defaults §7.3), thus the
 *  full application reads one authority. The code stores them in the
 *  working units of the device: fixed-point tenths for angles and
 *  temperature, ms for the presence windows, and integer seconds for the
 *  others.
 *
 *  Console grammar (case-insensitive):
 *      SET <name> <value>     value in display units. You can give one
 *                             decimal where the unit has tenths, for
 *                             example SET HOB 32.5
 *      SHOW                   print the table: name, value, unit, range
 *
 *  Integration sketch:
 *      const config_entry_t *e;
 *      switch (config_table_command(line, &e)) {
 *      case CFG_CMD_SET_OK:   ... write the R19 EVENT echo for e ... break;
 *      case CFG_CMD_REJECTED: break;   // the module printed the cause
 *      case CFG_CMD_SHOWN:    break;
 *      case CFG_CMD_NOT_MINE: ... your other commands (STATUS, CAL) ...
 *      }
 ******************************************************************************
 */
#ifndef CONFIG_TABLE_H
#define CONFIG_TABLE_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* The settings (§7.3 demo defaults). Working units in the comments.  */
/* ------------------------------------------------------------------ */
extern int32_t cfg_turn_interval_s;   /* R10  turn interval, s          */
extern int32_t cfg_delta_turn_tenths; /* R9   Δ_turn, 0.1°              */
extern int32_t cfg_t_hold_s;          /* R9   T_hold sustain, s         */
extern int32_t cfg_hob_limit_tenths;  /* R11/R12  HOB limit, 0.1°       */
extern int32_t cfg_t_grace_s;         /* R11  T_grace, s                */
extern int32_t cfg_delta_t_tenths;    /* R14  ΔT over baseline, 0.1 °C  */
extern int32_t cfg_present_ms;        /* R1   presence assert, ms       */
extern int32_t cfg_absent_ms;         /* R1   presence release, ms      */
extern int32_t cfg_rearm_s;           /* R17  re-alarm after ACK, s     */

/** One table row. `scale` = stored units for one display unit (1, 10 for
 *  tenths, 1000 for a ms-stored value that SHOW gives in seconds). */
typedef struct {
    const char *name;   /**< console token, "hob" for example   */
    const char *unit;   /**< display unit, "deg" for example    */
    int32_t     min;    /**< inclusive, stored units            */
    int32_t     max;    /**< inclusive, stored units            */
    int32_t     scale;  /**< stored units for one display unit  */
    int32_t    *value;  /**< the live setting                   */
    const char *what;   /**< one-line description for SHOW      */
} config_entry_t;

/** Result of config_table_command(). */
typedef enum {
    CFG_CMD_NOT_MINE = 0, /**< not SET or SHOW: try your commands    */
    CFG_CMD_SET_OK,       /**< accepted and stored: log it (R19)     */
    CFG_CMD_REJECTED,     /**< bad name or range: cause printed      */
    CFG_CMD_SHOWN         /**< SHOW handled                          */
} config_cmd_result_t;

/**
 * Handle one complete console line. On CFG_CMD_SET_OK, *set_entry points
 * at the changed row when the caller supplies a non-NULL pointer. That row
 * holds all the data that the R19 echo needs. The function does not block.
 * It prints two things only: the cause when it rejects a value, and the
 * SHOW output.
 */
config_cmd_result_t config_table_command(const char *line,
                                         const config_entry_t **set_entry);

/** Range-checked set from code (stored units). The DEMO or CLINICAL
 *  scaling command that you can add is a loop on this call. */
bool config_table_set(const config_entry_t *e, int32_t stored_value);

/** Table access for STATUS or for a scaling loop. */
int                   config_table_count(void);
const config_entry_t *config_table_entry(int index);
const config_entry_t *config_table_find(const char *name);

/** Format the current value of one row in display units ("32.5"), for
 *  your STATUS screen and the R19 echo. Returns dst. */
char *config_table_format(const config_entry_t *e, char *dst, int dst_len);

#endif /* CONFIG_TABLE_H */
