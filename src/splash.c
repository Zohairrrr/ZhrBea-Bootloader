/* splash.c - ZhrBea liquid glass background + splash */
#include <efi.h>
#include <efilib.h>
#include "../include/gop.h"
#include "../include/splash.h"

void splash_draw_background(void)
{
    if (!g_gop.ready) return;
    UINT32 W = g_gop.width, H = g_gop.height;
    UINT32 x, y;

    /* Base: dark blue-black gradient */
    for (y = 0; y < H; y++) {
        UINT8 r = (UINT8)(6  + (y * 8)  / H);
        UINT8 g2= (UINT8)(8  + (y * 6)  / H);
        UINT8 b = (UINT8)(18 + (y * 14) / H);
        UINT32 val = ((UINT32)r<<16)|((UINT32)g2<<8)|b;
        for (x = 0; x < W; x++)
            g_gop.fb[y * g_gop.stride + x] = val;
    }

    /* Colourful ambient blobs - these show through the glass giving it colour */
    /* Top-left: blue-purple */
    gop_fill_rect_alpha(0,       0,       W*2/5, H*2/5, RGB(0x30,0x20,0x80), 70);
    /* Top-right: cyan */
    gop_fill_rect_alpha(W*3/5,   0,       W*2/5, H/3,   RGB(0x10,0x60,0xA0), 60);
    /* Centre-left: pink */
    gop_fill_rect_alpha(0,       H/3,     W/3,   H/3,   RGB(0x80,0x20,0x60), 45);
    /* Bottom-right: teal */
    gop_fill_rect_alpha(W*2/3,   H*2/3,   W/3,   H/3,   RGB(0x10,0x70,0x70), 55);
    /* Centre: soft white glow (where the panel sits) */
    gop_fill_rect_alpha(W/4,     H/4,     W/2,   H/2,   RGB(0x30,0x50,0x90), 40);

    /* Darken everything slightly to keep it subtle */
    gop_fill_rect_alpha(0, 0, W, H, RGB(0x02,0x04,0x0C), 140);

    /* Scanline effect - very subtle horizontal lines */
    for (y = 0; y < H; y += 4)
        gop_draw_hline_px(0, y, W, 1, RGB(0x00,0x00,0x00));
    gop_fill_rect_alpha(0, 0, W, H, RGB(0x00,0x00,0x00), 30);

    /* Top and bottom bars */
    gop_fill_rect(0, 0,    W, 3,  RGB(0x60,0xA0,0xD0));
    gop_fill_rect(0, H-40, W, 40, RGB(0x02,0x03,0x08));
    gop_draw_hline_px(0, H-41, W, 1, RGB(0x30,0x50,0x80));
}

static void _logo(UINT32 cx, UINT32 cy, UINT8 alpha)
{
    Pixel name_col = RGB(
        (UINT8)(0xC0*alpha/255),
        (UINT8)(0xE0*alpha/255),
        (UINT8)(0xFF*alpha/255)
    );
    Pixel sub_col = RGB(
        (UINT8)(0x50*alpha/255),
        (UINT8)(0x80*alpha/255),
        (UINT8)(0xB0*alpha/255)
    );

    const char *name = "ZhrBea";
    UINT32 nw = gop_str_width(name);
    gop_draw_str(cx-nw/2, cy-10, name, name_col, RGB(0,0,0), TRUE);

    const char *sub = "Boot Manager";
    UINT32 sw = gop_str_width(sub);
    gop_draw_str(cx-sw/2, cy+10, sub, sub_col, RGB(0,0,0), TRUE);

    /* Underline glow */
    Pixel line = RGB((UINT8)(0x50*alpha/255),(UINT8)(0x90*alpha/255),(UINT8)(0xC0*alpha/255));
    gop_draw_hline_px(cx-nw/2, cy+28, nw, 1, line);
}

void splash_draw(void)
{
    if (!g_gop.ready) return;
    UINT32 W = g_gop.width, H = g_gop.height;
    UINT32 cx = W/2, cy = H/2 - 20;
    UINT32 step;

    /* Fade in */
    for (step = 1; step <= 18; step++) {
        gop_clear(RGB(0x06,0x08,0x14));
        _logo(cx, cy, (UINT8)(step * 14));
        uefi_call_wrapper(gBS->Stall, 1, (UINTN)25000);
    }

    /* Hold */
    gop_clear(RGB(0x06,0x08,0x14));
    _logo(cx, cy, 255);
    uefi_call_wrapper(gBS->Stall, 1, (UINTN)600000);

    /* Slide up + fade out */
    for (step = 1; step <= 14; step++) {
        gop_clear(RGB(0x06,0x08,0x14));
        UINT8 alpha = (UINT8)(255 - step*18);
        _logo(cx, cy - step*6, alpha);
        uefi_call_wrapper(gBS->Stall, 1, (UINTN)20000);
    }

    splash_draw_background();
}