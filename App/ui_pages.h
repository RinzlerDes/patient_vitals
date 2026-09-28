/**
 ******************************************************************************
 * @file    ui_pages.h
 * @brief   OLED page template: the LIVE/CLOCKS/SESSION registry, the
 *          labeled-row formatting, and the change-only redraw bookkeeping.
 *
 *          SWEN 563 / CMPE 663 — P2 "DG-30 DecuGuard" starter.
 *          PROVIDED MODULE — do not modify.
 *
 *  What it does for you: the page plumbing. It holds the page that the
 *  panel shows, the B2 page cycle (§3), the title row, and the R18 redraw
 *  discipline. It rewrites a row only when the text of that row changed,
 *  and a page switch causes a full redraw. The cost of about 170 µs for
 *  each line applies only when the content moves.
 *
 *  What stays yours: the page CONTENTS (one page callback for each page)
 *  and the alert banner. R16–R18 stay the same. This module does not run
 *  in an ISR, and your callbacks must not run in an ISR. Call
 *  ui_pages_service() from the main loop only.
 *
 *  Wiring:
 *      ui_pages_init()
 *      ui_pages_register(UI_PAGE_LIVE,    "LIVE",    render_live)
 *      ui_pages_register(UI_PAGE_CLOCKS,  "CLOCKS",  render_clocks)
 *      ui_pages_register(UI_PAGE_SESSION, "SESSION", render_session)
 *  After a B2 press (the EXTI callback latches it, and the loop uses it),
 *  call ui_pages_next(). When a displayed value changes, call
 *  ui_pages_mark_dirty(). In each loop cycle, call ui_pages_service().
 *
 *  A page callback draws rows 1..7 with ui_row_put(). Row 0 is the title
 *  row, and it belongs to the module (banner text through
 *  ui_pages_banner()).
 ******************************************************************************
 */
#ifndef UI_PAGES_H
#define UI_PAGES_H

#include <stdint.h>

/** The three §3 pages, in B2 cycle sequence. */
typedef enum {
    UI_PAGE_LIVE = 0,
    UI_PAGE_CLOCKS,
    UI_PAGE_SESSION,
    UI_PAGE_COUNT
} ui_page_t;

/** Page-content callback: draw rows 1..7 with ui_row_put(). Loop context. */
typedef void (*ui_page_render_fn)(void);

/** Reset the registry and the row shadow. The module then shows LIVE
 *  first. Call it one time, after oled_init(). */
void ui_pages_init(void);

/** Register (or replace) the title and the page callback of one page. */
void ui_pages_register(ui_page_t page, const char *title,
                       ui_page_render_fn render);

/** Move to the next page (B2). The module draws the new page in full. */
void ui_pages_next(void);

/** The page currently displayed. */
ui_page_t ui_pages_current(void);

/** Mark the displayed content stale. The module draws the current page
 *  again on the next service call. This call is cheap, so use it when a
 *  displayed value changes. */
void ui_pages_mark_dirty(void);

/** Set the title-row banner, for example the mode or the name of the
 *  alert with the highest priority. NULL gives the plain page title
 *  again. This call marks the display dirty. */
void ui_pages_banner(const char *text);

/** Run the redraw bookkeeping. It draws only after a dirty mark or a page
 *  switch, and it writes only the rows with changed text. Main loop
 *  only (R18). */
void ui_pages_service(void);

/**
 * Write one labeled row, "LABEL<spaces>value", with spaces to the full 21
 * columns. The function writes to the panel only when the text differs
 * from the text that the row shows. Rows 1..7 (row 0 is the title row).
 */
void ui_row_put(uint8_t row, const char *label, const char *value);

#endif /* UI_PAGES_H */
