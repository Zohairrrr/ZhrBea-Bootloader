/* =============================================================================
 * ui.c  -  No-flicker UI renderer
 *
 * Every output line is EXACTLY 80 characters wide (no \r\n wrapping issues).
 * NO variadic Print() calls. ONE OutputString per row.
 * =============================================================================
 */
#include <efi.h>
#include <efilib.h>
#include "../include/ui.h"

EFI_SYSTEM_TABLE *g_ST = NULL;

#define OUT(s)   uefi_call_wrapper(g_ST->ConOut->OutputString,    2, g_ST->ConOut, (CHAR16*)(s))
#define COL(a)   uefi_call_wrapper(g_ST->ConOut->SetAttribute,    2, g_ST->ConOut, (UINTN)(a))
#define POS(c,r) uefi_call_wrapper(g_ST->ConOut->SetCursorPosition,3,g_ST->ConOut, (UINTN)(c),(UINTN)(r))
#define CLR()    uefi_call_wrapper(g_ST->ConOut->ClearScreen,      1, g_ST->ConOut)

/* All row strings are EXACTLY 80 chars + \r\n + \0 = 83 CHAR16 */
/* Verified: border=80, blank=80, every content row=80            */

static const CHAR16 S_BORDER[]   = L"+------------------------------------------------------------------------------+\r\n";
static const CHAR16 S_TITLE[]    = L"|                            *** THE BOOTLOADER ***                            |\r\n";
static const CHAR16 S_SUBTITLE[] = L"|                          A modern UEFI boot manager                          |\r\n";
static const CHAR16 S_HINT[]     = L"|             Up/Down: select    Enter: boot    F5: reload config              |\r\n";
static const CHAR16 S_BLANK[]    = L"|                                                                              |\r\n";
static const CHAR16 S_TIMER_CLR[]= L"                                                                                \r\n";
static const CHAR16 S_FOOTER[]   = L"                                UEFI Bootloader                                 \r\n";

void ui_init(EFI_SYSTEM_TABLE *ST)
{
    g_ST = ST;
    uefi_call_wrapper(ST->ConOut->Reset,        2, ST->ConOut, (UINTN)FALSE);
    uefi_call_wrapper(ST->ConIn->Reset,         2, ST->ConIn,  (UINTN)FALSE);
    uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, (UINTN)FALSE);
    COL(COL_NORMAL);
    CLR();
}

void ui_goto (UINTN c, UINTN r) { POS(c,r); }
void ui_color(UINTN a)          { COL(a);   }
void ui_print(const CHAR16 *s)  { OUT(s);   }

/* ── Chrome ──────────────────────────────────────────────────────────────── */
void ui_draw_chrome(void)
{
    POS(0, 0);

    COL(COL_BORDER);  OUT(S_BORDER);
    COL(COL_TITLE);   OUT(S_TITLE);
    COL(COL_FOOTER);  OUT(S_SUBTITLE);
    COL(COL_BORDER);  OUT(S_BORDER);
    COL(COL_FOOTER);  OUT(S_HINT);

    /* 16 blank menu rows */
    COL(COL_NORMAL);
    OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK);
    OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK);
    OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK);
    OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK); OUT(S_BLANK);

    COL(COL_BORDER);  OUT(S_BORDER);
    COL(COL_NORMAL);  OUT(S_TIMER_CLR);
    COL(COL_FOOTER);  OUT(S_FOOTER);
    COL(COL_NORMAL);
}

/* ── Menu row ────────────────────────────────────────────────────────────── */
/*
 * Format: | [>] label(72 chars padded with spaces) |
 * Positions: 0='|' 1=' ' 2=' ' 3='>'or' ' 4=' '
 *            5..76=label(72chars) 77=' ' 78=' ' 79='|'
 * Total: 80 chars + \r + \0
 */
void ui_draw_menu_row(UINTN row_index, const CHAR16 *label, int is_selected)
{
    CHAR16 buf[82];
    UINTN i;

    buf[0] = L'|';
    buf[1] = L' ';
    buf[2] = L' ';
    buf[3] = is_selected ? L'>' : L' ';
    buf[4] = L' ';
    for (i = 0; i < 72; i++)
        buf[5+i] = label[i] ? label[i] : L' ';
    buf[77] = L' ';
    buf[78] = L' ';
    buf[79] = L'|';
    buf[80] = L'\r';
    buf[81] = L'\0';

    POS(0, ROW_MENU_START + row_index);
    COL(is_selected ? COL_SELECTED : COL_UNSELECTED);
    OUT(buf);
    COL(COL_NORMAL);
}

void ui_update_selection(UINTN old_idx, const CHAR16 *old_label,
                         UINTN new_idx, const CHAR16 *new_label)
{
    ui_draw_menu_row(old_idx, old_label, 0);
    ui_draw_menu_row(new_idx, new_label, 1);
}

/* ── Timer row ───────────────────────────────────────────────────────────── */
void ui_update_timer(UINTN seconds_left)
{
    POS(0, ROW_TIMER);
    if (seconds_left == 0) {
        COL(COL_NORMAL);
        OUT(L"                                                                                \r");
        return;
    }

    /*
     * Build exactly 80-char string: spaces around the message
     * "  Auto-boot in Xs - press any key to cancel  " padded to 80
     */
    CHAR16 buf[82];
    UINTN i;
    for (i = 0; i < 80; i++) buf[i] = L' ';
    buf[80] = L'\r';
    buf[81] = L'\0';

    /* Write message starting at col 2 */
    UINTN pos = 2;
    const CHAR16 *pre = L"Auto-boot in ";
    while (*pre && pos < 79) buf[pos++] = *pre++;

    CHAR16 d[4]; UINTN dc = 0, n = seconds_left;
    do { d[dc++] = (CHAR16)(L'0' + n % 10); n /= 10; } while (n && dc < 3);
    while (dc && pos < 79) buf[pos++] = d[--dc];

    const CHAR16 *suf = L"s - press any key to cancel";
    while (*suf && pos < 79) buf[pos++] = *suf++;

    COL(COL_TIMER);
    OUT(buf);
    COL(COL_NORMAL);
}

/* ── Status ──────────────────────────────────────────────────────────────── */
void ui_set_status(UINTN attr, const CHAR16 *msg)
{
    /* Build 80-char centred row, no newline, \r to stay on same line */
    CHAR16 buf[82];
    UINTN i;
    for (i = 0; i < 80; i++) buf[i] = L' ';
    buf[80] = L'\r';
    buf[81] = L'\0';

    UINTN len = 0;
    while (msg[len]) len++;
    UINTN start = (len < 80) ? (80 - len) / 2 : 0;
    for (i = 0; i < len && (start+i) < 80; i++)
        buf[start+i] = msg[i];

    POS(0, ROW_FOOTER);
    COL(attr);
    OUT(buf);
    COL(COL_NORMAL);
}

void ui_clear_row(UINTN row, UINTN attr)
{
    POS(0, row);
    COL(attr);
    OUT(L"                                                                                \r");
    POS(0, row);
    COL(COL_NORMAL);
}

void ui_print_centered(UINTN row, UINTN attr, const CHAR16 *str)
{
    /* Clear then print centred - used for misc rows */
    ui_clear_row(row, attr);
    UINTN len = 0;
    while (str[len]) len++;
    UINTN col = (len < 80) ? (80 - len) / 2 : 0;
    POS(col, row);
    COL(attr);
    OUT(str);
    COL(COL_NORMAL);
}

void ui_draw_hline(UINTN row, UINTN attr, int style)
{
    (void)style;
    POS(0, row);
    COL(attr);
    OUT(L"+------------------------------------------------------------------------------+\r");
    COL(COL_NORMAL);
}

void ui_full_redraw(void)
{
    CLR();
    ui_draw_chrome();
}