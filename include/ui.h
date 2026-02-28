#ifndef UI_H
#define UI_H

#include <efi.h>
#include <efilib.h>

/* Colour attributes */
#define ATTR(fg, bg)    ((UINTN)((bg) << 4 | (fg)))
#define COL_NORMAL      ATTR(EFI_WHITE,     EFI_BACKGROUND_BLACK)
#define COL_TITLE       ATTR(EFI_CYAN,      EFI_BACKGROUND_BLACK)
#define COL_SELECTED    ATTR(EFI_BLACK,     EFI_BACKGROUND_CYAN)
#define COL_UNSELECTED  ATTR(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK)
#define COL_FOOTER      ATTR(EFI_DARKGRAY,  EFI_BACKGROUND_BLACK)
#define COL_HIGHLIGHT   ATTR(EFI_YELLOW,    EFI_BACKGROUND_BLACK)
#define COL_ERROR       ATTR(EFI_RED,       EFI_BACKGROUND_BLACK)
#define COL_SUCCESS     ATTR(EFI_GREEN,     EFI_BACKGROUND_BLACK)
#define COL_TIMER       ATTR(EFI_LIGHTCYAN, EFI_BACKGROUND_BLACK)
#define COL_BORDER      ATTR(EFI_BLUE,      EFI_BACKGROUND_BLACK)

/* Screen size */
#define SCREEN_COLS     80
#define SCREEN_ROWS     25

/*
 * Row layout (must match what ui_draw_chrome() actually prints):
 *
 *  0  +-----border-----+
 *  1  | title          |
 *  2  | subtitle       |
 *  3  +-----border-----+
 *  4  | hints          |
 *  5  | menu entry 0   |  <- ROW_MENU_START
 *  6  | menu entry 1   |
 *  ...
 * 20  | menu entry 15  |
 * 21  +-----border-----+  <- ROW_BORDER_BOT
 * 22  | timer          |  <- ROW_TIMER
 * 23  | footer         |  <- ROW_FOOTER
 */
#define ROW_BORDER_TOP   0
#define ROW_TITLE        1
#define ROW_SUBTITLE     2
#define ROW_BORDER_SEP   3
#define ROW_HINT         4
#define ROW_MENU_START   5
#define ROW_BORDER_BOT  21
#define ROW_TIMER       22
#define ROW_FOOTER      23

/* Public API */
void ui_init(EFI_SYSTEM_TABLE *ST);
void ui_goto(UINTN col, UINTN row);
void ui_color(UINTN attr);
void ui_print(const CHAR16 *str);
void ui_clear_row(UINTN row, UINTN attr);
void ui_print_centered(UINTN row, UINTN attr, const CHAR16 *str);
void ui_draw_hline(UINTN row, UINTN attr, int style);
void ui_draw_chrome(void);
void ui_draw_menu_row(UINTN row_index, const CHAR16 *label, int is_selected);
void ui_update_selection(UINTN old_idx, const CHAR16 *old_label,
                         UINTN new_idx, const CHAR16 *new_label);
void ui_update_timer(UINTN seconds_left);
void ui_set_status(UINTN attr, const CHAR16 *msg);
void ui_full_redraw(void);

extern EFI_SYSTEM_TABLE *g_ST;

#endif