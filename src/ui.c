/* =============================================================================
 * ui.c - ZhrBea Boot Manager - Liquid Glass UI
 *
 * Liquid glass technique (no real blur available in framebuffer):
 *   1. Background shows through via alpha blending
 *   2. Bright white specular strip at top of each element (light refraction)
 *   3. Soft inner glow on borders
 *   4. Bottom shadow for depth
 *   5. Slight tint (blue/cyan) to simulate the glass colour cast
 * =============================================================================
 */
#include <efi.h>
#include <efilib.h>
#include "../include/ui.h"
#include "../include/gop.h"
#include "../include/splash.h"
#include "../include/health.h"

EFI_SYSTEM_TABLE *g_ST = NULL;

static UINT32 SW, SH;
static UINT32 PX, PY, PW, PH;
static UINT32 MX, MY, MW;
static UINT32 MRH = 46;

static void _layout(void)
{
    SW = g_gop.width;  SH = g_gop.height;
    PW = 520;          PH = 360;
    PX = (SW - PW)/2;  PY = (SH - PH)/2 - 20;
    MX = PX + 16;      MY = PY + 72;
    MW = PW - 32;
}

/* ── Liquid glass primitive ──────────────────────────────────────────────────
 * Layers from bottom to top:
 *   a) dark translucent fill  (the "frosted" body)
 *   b) blue-tinted midlayer   (glass colour cast)
 *   c) bright top specular    (light hitting the curved glass surface)
 *   d) crisp 1px white border (glass edge)
 *   e) soft inner glow        (light scattering inside glass)
 * ─────────────────────────────────────────────────────────────────────────── */
static void _liquid_glass(UINT32 x, UINT32 y, UINT32 w, UINT32 h,
                           UINT8 tint_alpha,   /* how blue the tint is 0-255 */
                           UINT8 body_alpha)   /* overall opacity     0-255 */
{
    /* Layer 1: dark base body */
    gop_fill_rect_alpha(x, y, w, h, RGB(0x06,0x0A,0x1E), body_alpha);

    /* Layer 2: blue-cyan glass tint */
    gop_fill_rect_alpha(x, y, w, h, RGB(0x28,0x6A,0xAA), tint_alpha);

    /* Layer 3: top specular highlight - bright white strip, fades down */
    gop_fill_rect_alpha(x+2, y+1,   w-4, 1, RGB(0xFF,0xFF,0xFF), 200);
    gop_fill_rect_alpha(x+2, y+2,   w-4, 1, RGB(0xFF,0xFF,0xFF), 120);
    gop_fill_rect_alpha(x+2, y+3,   w-4, 1, RGB(0xFF,0xFF,0xFF), 60);
    gop_fill_rect_alpha(x+2, y+4,   w-4, 1, RGB(0xFF,0xFF,0xFF), 20);

    /* Layer 4: left edge specular (vertical highlight) */
    gop_fill_rect_alpha(x+1, y+2, 1, h-4, RGB(0xFF,0xFF,0xFF), 80);
    gop_fill_rect_alpha(x+2, y+2, 1, h-4, RGB(0xFF,0xFF,0xFF), 30);

    /* Layer 5: outer border - bright white with slight blue */
    gop_draw_rect(x, y, w, h, 1, RGB(0xC0,0xD8,0xFF));

    /* Layer 6: inner glow border */
    gop_fill_rect_alpha(x+1,   y+1,   w-2, 1, RGB(0x80,0xB0,0xFF), 60);
    gop_fill_rect_alpha(x+1,   y+h-2, w-2, 1, RGB(0x20,0x40,0x80), 60);
    gop_fill_rect_alpha(x+1,   y+1,   1, h-2, RGB(0x80,0xB0,0xFF), 40);
    gop_fill_rect_alpha(x+w-2, y+1,   1, h-2, RGB(0x20,0x40,0x80), 40);

    /* Layer 7: bottom shadow (inside) */
    gop_fill_rect_alpha(x+2, y+h-3, w-4, 2, RGB(0x00,0x00,0x10), 100);
}

/* ── Selected row: bright liquid glass with stronger glow ────────────────── */
static void _liquid_row_selected(UINT32 x, UINT32 y, UINT32 w, UINT32 h)
{
    /* Bright cyan-blue fill */
    gop_fill_rect_alpha(x, y, w, h, RGB(0x1A,0x5A,0x9A), 200);
    gop_fill_rect_alpha(x, y, w, h, RGB(0x40,0x90,0xD0), 80);

    /* Strong specular at top */
    gop_fill_rect_alpha(x+2, y+1, w-4, 2, RGB(0xFF,0xFF,0xFF), 180);
    gop_fill_rect_alpha(x+2, y+3, w-4, 2, RGB(0xFF,0xFF,0xFF), 60);
    gop_fill_rect_alpha(x+2, y+5, w-4, 1, RGB(0xFF,0xFF,0xFF), 20);

    /* Left accent glow */
    gop_fill_rect(x, y, 3, h, RGB(0x88,0xC0,0xD0));
    gop_fill_rect_alpha(x+3, y, 4, h, RGB(0x88,0xC0,0xD0), 60);

    /* Glowing border */
    gop_draw_rect(x, y, w, h, 1, RGB(0xA0,0xD8,0xFF));

    /* Inner glow */
    gop_fill_rect_alpha(x+1, y+1, w-2, 1, RGB(0xFF,0xFF,0xFF), 80);
}

/* ── Normal row: subtle glass ────────────────────────────────────────────── */
static void _liquid_row_normal(UINT32 x, UINT32 y, UINT32 w, UINT32 h)
{
    gop_fill_rect_alpha(x, y, w, h, RGB(0x08,0x10,0x28), 90);
    gop_fill_rect_alpha(x, y, w, 1, RGB(0xFF,0xFF,0xFF), 15);
    gop_fill_rect_alpha(x, y+h-1, w, 1, RGB(0x00,0x00,0x20), 40);
}

/* ── Chrome ──────────────────────────────────────────────────────────────── */
void ui_draw_chrome(void)
{
    if (!g_gop.ready) return;

    splash_draw_background();

    /* Outer drop shadow */
    gop_fill_rect_alpha(PX+10, PY+10, PW, PH, RGB(0x00,0x00,0x00), 140);
    gop_fill_rect_alpha(PX+6,  PY+6,  PW, PH, RGB(0x00,0x00,0x10), 100);

    /* Main panel - liquid glass */
    _liquid_glass(PX, PY, PW, PH, 55, 170);

    /* Header glass sub-panel (brighter tint) */
    _liquid_glass(PX+1, PY+1, PW-2, 60, 80, 80);

    /* Brand */
    const char *brand = "ZhrBea";
    UINT32 bw = gop_str_width(brand);
    gop_draw_str(PX+(PW-bw)/2, PY+14, brand, RGB(0xE8,0xF4,0xFF), RGB(0,0,0), TRUE);

    const char *sub = "Boot Manager";
    UINT32 sw2 = gop_str_width(sub);
    gop_draw_str(PX+(PW-sw2)/2, PY+32, sub, RGB(0x80,0xA8,0xD0), RGB(0,0,0), TRUE);

    /* Separator */
    gop_fill_rect_alpha(PX+12, PY+62, PW-24, 1, RGB(0xFF,0xFF,0xFF), 35);

    /* Hint */
    const char *hint = "Up/Down   Enter: boot   F5: rescan";
    UINT32 hw = gop_str_width(hint);
    gop_draw_str((SW-hw)/2, PY+PH+18, hint, RGB(0x50,0x68,0x90), RGB(0,0,0), TRUE);
}

void ui_draw_health(HealthReport *report)
{
    if (!g_gop.ready || !report) return;
    UINT32 hx = PX+16, hy = PY+PH-28;

    UINTN i;
    for (i = 0; i < report->count && i < 4; i++) {
        Pixel dot = (report->checks[i].status == HEALTH_OK)   ? RGB(0x80,0xD0,0x80) :
                    (report->checks[i].status == HEALTH_WARN) ? RGB(0xE0,0xC0,0x40) :
                                                                 RGB(0xD0,0x50,0x50);
        /* Glowing dot */
        gop_fill_rect(hx, hy+3, 6, 6, dot);
        gop_fill_rect_alpha(hx-1, hy+2, 8, 8, dot, 40);
        gop_draw_str(hx+10, hy, report->checks[i].label,
                     RGB(0x80,0x98,0xC0), RGB(0,0,0), TRUE);
        hx += gop_str_width(report->checks[i].label) + 26;
        if (hx > PX+PW-60) break;
    }
}

void ui_draw_menu_row(UINTN idx, const CHAR16 *label, int sel)
{
    if (!g_gop.ready) return;
    UINT32 rx=MX, ry=MY+(UINT32)idx*MRH, rh=MRH-5;

    if (sel) {
        _liquid_row_selected(rx, ry, MW, rh);
        gop_draw_str (rx+14, ry+(rh-16)/2, ">",   RGB(0xFF,0xFF,0xFF), RGB(0,0,0), TRUE);
        gop_draw_wstr(rx+30, ry+(rh-16)/2, label, RGB(0xFF,0xFF,0xFF), RGB(0,0,0), TRUE);
    } else {
        _liquid_row_normal(rx, ry, MW, rh);
        gop_draw_wstr(rx+30, ry+(rh-16)/2, label, RGB(0xC0,0xD4,0xF0), RGB(0,0,0), TRUE);
    }
}

void ui_update_selection(UINTN oi, const CHAR16 *ol, UINTN ni, const CHAR16 *nl)
{
    ui_draw_menu_row(oi, ol, 0);
    ui_draw_menu_row(ni, nl, 1);
}

void ui_update_timer(UINTN secs)
{
    if (!g_gop.ready) return;
    UINT32 ty=PY+PH+38, bby=ty+20;
    gop_fill_rect((SW-PW)/2-10, ty-4, PW+20, 32, RGB(0x06,0x08,0x14));
    if (!secs) return;

    char msg[48]; char *p=msg;
    const char *pre="Auto-boot in ";
    while(*pre)*p++=*pre++;
    char d[4]; UINTN dc=0,n=secs;
    do{d[dc++]='0'+n%10;n/=10;}while(n&&dc<3);
    while(dc)*p++=d[--dc];
    *p++='s'; *p='\0';

    UINT32 tw=gop_str_width(msg);
    gop_draw_str((SW-tw)/2, ty, msg, RGB(0x90,0xD0,0x90), RGB(0,0,0), TRUE);

    /* Liquid glass progress bar */
    UINT32 bx=PX, bw=PW;
    UINT32 filled=(UINT32)(secs*bw/10); if(filled>bw)filled=bw;
    gop_fill_rect_alpha(bx, bby, bw, 6, RGB(0xFF,0xFF,0xFF), 15);
    gop_fill_rect_alpha(bx, bby, filled, 6, RGB(0x50,0xC0,0x80), 200);
    gop_fill_rect_alpha(bx, bby, filled, 2, RGB(0xFF,0xFF,0xFF), 100);
    gop_draw_rect(bx, bby, bw, 6, 1, RGB(0x40,0x60,0x90));
}

void ui_set_status(Pixel attr, const CHAR16 *msg)
{
    if (!g_gop.ready) return;
    UINT32 y=SH-20;
    gop_fill_rect(0, y, SW, 20, RGB(0x04,0x05,0x0E));
    UINT32 tw=gop_wstr_width(msg);
    gop_draw_wstr((SW-tw)/2, y+2, msg, attr, RGB(0,0,0), TRUE);
}

void ui_init(EFI_SYSTEM_TABLE *ST)
{
    g_ST = ST;
    uefi_call_wrapper(ST->ConIn->Reset,         2, ST->ConIn,  (UINTN)FALSE);
    uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, (UINTN)FALSE);
    gop_init(ST);
    _layout();
    if (g_gop.ready) splash_draw();
}

void ui_clear_row(UINTN r,UINTN a)                       {(void)r;(void)a;}
void ui_print_centered(UINTN r,UINTN a,const CHAR16 *s)  {ui_set_status(COL_TEXT,s);(void)r;(void)a;}
void ui_draw_hline(UINTN r,UINTN a,int s)               {(void)r;(void)a;(void)s;}
void ui_goto(UINTN c,UINTN r)                           {(void)c;(void)r;}
void ui_color(UINTN a)                                   {(void)a;}
void ui_print(const CHAR16 *s)                           {(void)s;}
void ui_full_redraw(void)                                {ui_draw_chrome();}