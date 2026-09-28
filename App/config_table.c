/**
 ******************************************************************************
 * @file    config_table.c
 * @brief   Config-table console: the table and the generic SET and SHOW.
 *          PROVIDED MODULE — do not modify. Refer to config_table.h.
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *
 *  Two things are NOT here. There is no console_poll() loop, because the
 *  line assembly is your non-blocking problem (R20). There is no
 *  [ssss.mmm] logging, because the EVENT echo of an accepted value is
 *  your audit record (R19).
 ******************************************************************************
 */
#include "config_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* The settings: §7.3 demo defaults, stored in working units.         */
/* ------------------------------------------------------------------ */
int32_t cfg_turn_interval_s   = 120;   /* R10             */
int32_t cfg_delta_turn_tenths = 100;   /* R9   10.0°      */
int32_t cfg_t_hold_s          = 10;    /* R9              */
int32_t cfg_hob_limit_tenths  = 300;   /* R11  30.0°      */
int32_t cfg_t_grace_s         = 10;    /* R11             */
int32_t cfg_delta_t_tenths    = 20;    /* R14  +2.0 °C    */
int32_t cfg_present_ms        = 1000;  /* R1   1 s        */
int32_t cfg_absent_ms         = 3000;  /* R1   3 s        */
int32_t cfg_rearm_s           = 60;    /* R17             */

/* name        unit    min    max    scale  value                    what */
static const config_entry_t s_table[] = {
    { "turn",    "s",    10,   7200,    1, &cfg_turn_interval_s,
      "turn interval (R10)"                                            },
    { "dturn",   "deg",  20,    450,   10, &cfg_delta_turn_tenths,
      "posture-change delta (R9)"                                      },
    { "hold",    "s",     2,    120,    1, &cfg_t_hold_s,
      "posture-change sustain (R9)"                                    },
    { "hob",     "deg", 100,    450,   10, &cfg_hob_limit_tenths,
      "head-of-bed limit (R11/R12)"                                    },
    { "grace",   "s",     2,    120,    1, &cfg_t_grace_s,
      "HOB grace (R11)"                                                },
    { "tdelta",  "degC",  5,    100,   10, &cfg_delta_t_tenths,
      "temp rise over baseline (R14)"                                  },
    { "present", "s",   200,  10000, 1000, &cfg_present_ms,
      "presence assert (R1)"                                           },
    { "absent",  "s",   500,  30000, 1000, &cfg_absent_ms,
      "presence release (R1)"                                          },
    { "rearm",   "s",     5,   3600,    1, &cfg_rearm_s,
      "re-alarm after ACK (R17)"                                       },
};
#define TABLE_N ((int)(sizeof s_table / sizeof s_table[0]))

/* ---- small local helpers (no ctype dependency) ------------------- */

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool name_eq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (lower(*a++) != lower(*b++)) {
            return false;
        }
    }
    return *a == '\0' && *b == '\0';
}

/**
 * Parse "<int>[.<digit>]" into tenths of a display unit. For example,
 * "32.5" becomes 325. The function rejects more fractional digits,
 * because the resolution of the instrument is tenths (§5.1). It returns
 * false on malformed input.
 */
static bool parse_tenths(const char *s, int32_t *out)
{
    int32_t whole = 0;
    int32_t tenth = 0;
    bool    neg   = false;
    bool    any   = false;

    if (*s == '-') { neg = true; s++; }
    while (*s >= '0' && *s <= '9') {
        if (whole > 214748000L) {
            return false;                      /* overflow guard */
        }
        whole = whole * 10 + (*s - '0');
        any   = true;
        s++;
    }
    if (*s == '.') {
        s++;
        if (*s < '0' || *s > '9') {
            return false;
        }
        tenth = *s - '0';
        any   = true;
        s++;
    }
    if (!any || *s != '\0') {
        return false;
    }
    *out = whole * 10 + tenth;
    if (neg) {
        *out = -*out;
    }
    return true;
}

char *config_table_format(const config_entry_t *e, char *dst, int dst_len)
{
    long v = (long)*e->value;

    if (e->scale == 1) {
        snprintf(dst, (size_t)dst_len, "%ld", v);
    } else {
        /* tenths of a display unit: scale 10 gives v tenths, and a scale
         * of 1000 means the setting is stored in ms. */
        long t = (e->scale == 10) ? v : (v / 100);
        snprintf(dst, (size_t)dst_len, "%s%ld.%ld",
                 (t < 0) ? "-" : "", labs(t) / 10, labs(t) % 10);
    }
    return dst;
}

bool config_table_set(const config_entry_t *e, int32_t stored_value)
{
    if (e == 0 || stored_value < e->min || stored_value > e->max) {
        return false;
    }
    *e->value = stored_value;
    return true;
}

int config_table_count(void)
{
    return TABLE_N;
}

const config_entry_t *config_table_entry(int index)
{
    return (index >= 0 && index < TABLE_N) ? &s_table[index] : 0;
}

const config_entry_t *config_table_find(const char *name)
{
    for (int i = 0; i < TABLE_N; i++) {
        if (name_eq(name, s_table[i].name)) {
            return &s_table[i];
        }
    }
    return 0;
}

static void show_table(void)
{
    char val[16], lo[16], hi[16];

    printf("  %-8s %10s %-5s  %-9s  %s\n",
           "name", "value", "unit", "range", "setting");
    for (int i = 0; i < TABLE_N; i++) {
        const config_entry_t *e = &s_table[i];
        int32_t keep = *e->value;

        config_table_format(e, val, sizeof val);
        *e->value = e->min; config_table_format(e, lo, sizeof lo);
        *e->value = e->max; config_table_format(e, hi, sizeof hi);
        *e->value = keep;
        printf("  %-8s %10s %-5s  %4s-%-4s  %s\n",
               e->name, val, e->unit, lo, hi, e->what);
    }
}

config_cmd_result_t config_table_command(const char *line,
                                         const config_entry_t **set_entry)
{
    char cmd[8]  = { 0 };
    char name[12] = { 0 };
    char arg[16] = { 0 };
    int  n;

    if (set_entry != 0) {
        *set_entry = 0;
    }
    n = sscanf(line, "%7s %11s %15s", cmd, name, arg);
    if (n < 1) {
        return CFG_CMD_NOT_MINE;
    }

    if (name_eq(cmd, "show")) {
        show_table();
        return CFG_CMD_SHOWN;
    }
    if (!name_eq(cmd, "set")) {
        return CFG_CMD_NOT_MINE;
    }
    if (n != 3) {
        printf("SET: usage: SET <name> <value>  (SHOW lists names)\n");
        return CFG_CMD_REJECTED;
    }

    const config_entry_t *e = config_table_find(name);
    if (e == 0) {
        printf("SET: no setting '%s' (SHOW lists names)\n", name);
        return CFG_CMD_REJECTED;
    }

    int32_t tenths;
    if (!parse_tenths(arg, &tenths)) {
        printf("SET: bad value '%s' — number, one decimal at most\n", arg);
        return CFG_CMD_REJECTED;
    }
    /* display tenths → stored units. Scale 10 keeps the tenths. Scale 1
     * drops the decimal, which must be .0. Scale 1000 changes seconds
     * into ms. */
    int32_t stored;
    if (e->scale == 10) {
        stored = tenths;
    } else if (e->scale == 1) {
        if (tenths % 10 != 0) {
            printf("SET: %s takes whole %s\n", e->name, e->unit);
            return CFG_CMD_REJECTED;
        }
        stored = tenths / 10;
    } else {
        stored = tenths * 100;                 /* s (tenths) → ms */
    }

    if (!config_table_set(e, stored)) {
        char lo[16], hi[16];
        int32_t keep = *e->value;
        *e->value = e->min; config_table_format(e, lo, sizeof lo);
        *e->value = e->max; config_table_format(e, hi, sizeof hi);
        *e->value = keep;
        printf("SET: %s out of range (%s-%s %s)\n", e->name, lo, hi, e->unit);
        return CFG_CMD_REJECTED;
    }

    if (set_entry != 0) {
        *set_entry = e;                        /* your R19 echo starts here */
    }
    return CFG_CMD_SET_OK;
}
