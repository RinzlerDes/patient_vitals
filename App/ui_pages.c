/**
 ******************************************************************************
 * @file    ui_pages.c
 * @brief   OLED page template: the registry and the change-only redraw
 *          bookkeeping. PROVIDED MODULE — do not modify. Refer to
 *          ui_pages.h for the contract.
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *
 *  The shadow buffer below IS the R18 discipline in code. The module
 *  writes to the panel only where the text moved. Read ui_row_put() to
 *  see the full idea.
 ******************************************************************************
 */
#include "ui_pages.h"
#include "oled.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    const char       *title;
    ui_page_render_fn render;
} ui_page_slot_t;

static ui_page_slot_t s_pages[UI_PAGE_COUNT];
static ui_page_t      s_current;
static const char    *s_banner;                       /* NULL = page title  */
static uint8_t        s_dirty;                        /* content changed    */
static uint8_t        s_repaint;                      /* full clear needed  */
static char           s_shadow[OLED_ROWS][OLED_COLS + 1];

void ui_pages_init(void)
{
    memset(s_pages, 0, sizeof s_pages);
    memset(s_shadow, 0, sizeof s_shadow);
    s_current = UI_PAGE_LIVE;
    s_banner  = 0;
    s_dirty   = 1u;
    s_repaint = 1u;
}

void ui_pages_register(ui_page_t page, const char *title,
                       ui_page_render_fn render)
{
    if (page >= UI_PAGE_COUNT) {
        return;
    }
    s_pages[page].title  = title;
    s_pages[page].render = render;
    s_dirty = 1u;
}

void ui_pages_next(void)
{
    s_current = (ui_page_t)((s_current + 1u) % UI_PAGE_COUNT);
    s_repaint = 1u;
}

ui_page_t ui_pages_current(void)
{
    return s_current;
}

void ui_pages_mark_dirty(void)
{
    s_dirty = 1u;
}

void ui_pages_banner(const char *text)
{
    s_banner = text;
    s_dirty  = 1u;
}

/** Write one full padded row through the shadow. The panel gets a write
 *  on a change only. */
static void row_write(uint8_t row, const char *text)
{
    char line[OLED_COLS + 1];
    unsigned i = 0;

    while (i < OLED_COLS && text[i] != '\0') {
        line[i] = text[i];
        i++;
    }
    while (i < OLED_COLS) {
        line[i++] = ' ';
    }
    line[OLED_COLS] = '\0';

    if (row >= OLED_ROWS || strcmp(line, s_shadow[row]) == 0) {
        return;                                /* unchanged: 0 µs spent */
    }
    memcpy(s_shadow[row], line, sizeof line);
    oled_write_line(row, line);
}

void ui_row_put(uint8_t row, const char *label, const char *value)
{
    char line[OLED_COLS + 1];

    if (row == 0u || row >= OLED_ROWS) {
        return;                                /* row 0 is the title row */
    }
    snprintf(line, sizeof line, "%-8.8s %s",
             (label != 0) ? label : "", (value != 0) ? value : "");
    row_write(row, line);
}

void ui_pages_service(void)
{
    const ui_page_slot_t *p = &s_pages[s_current];

    if (s_repaint) {
        /* Page switch: clear the panel and the shadow, thus each row gets
         * a new write. */
        oled_clear();
        memset(s_shadow, 0, sizeof s_shadow);
        s_repaint = 0u;
        s_dirty   = 1u;
    }
    if (!s_dirty) {
        return;
    }
    s_dirty = 0u;

    row_write(0, (s_banner != 0)   ? s_banner
               : (p->title != 0)   ? p->title
                                   : "DG-30");
    if (p->render != 0) {
        p->render();                           /* your page content */
    }
}
