#ifndef UI_H
#define UI_H

#include <efi.h>
#include <efilib.h>
#include "gop.h"
#include "health.h"

/* EFI text-mode row constants (fallback only) */
#define SCREEN_COLS      80
#define SCREEN_ROWS      25
#define ROW_BORDER_TOP    0
#define ROW_TITLE         1
#define ROW_SUBTITLE      2
#define ROW_BORDER_SEP    3
#define ROW_HINT          4
#define ROW_MENU_START    5
#define ROW_BORDER_BOT   21
#define ROW_TIMER        22
#define ROW_FOOTER       23

void ui_init(EFI_SYSTEM_TABLE *ST);
void ui_draw_chrome(void);
void ui_draw_menu_row(UINTN idx, const CHAR16 *label, int selected);
void ui_update_selection(UINTN old_idx, const CHAR16 *old_label,
                         UINTN new_idx, const CHAR16 *new_label);
void ui_update_timer(UINTN seconds_left);
void ui_set_status(Pixel attr, const CHAR16 *msg);
void ui_full_redraw(void);
void ui_draw_health(HealthReport *report);

/* Stubs - kept for compatibility */
void ui_goto(UINTN col, UINTN row);
void ui_color(UINTN attr);
void ui_print(const CHAR16 *str);
void ui_clear_row(UINTN row, UINTN attr);
void ui_print_centered(UINTN row, UINTN attr, const CHAR16 *str);
void ui_draw_hline(UINTN row, UINTN attr, int style);

extern EFI_SYSTEM_TABLE *g_ST;

#endif /* UI_H */