#ifndef GOP_H
#define GOP_H

#include <efi.h>
#include <efilib.h>

typedef struct { UINT8 b, g, r, a; } Pixel;

#define RGB(r,g,b)  ((Pixel){(b),(g),(r),0xFF})

/* All colours as Pixel */
#define COL_BG          RGB(0x0D,0x0F,0x1A)
#define COL_ACCENT      RGB(0x5E,0x81,0xAC)
#define COL_ACCENT2     RGB(0x88,0xC0,0xD0)
#define COL_TEXT        RGB(0xEC,0xEF,0xF4)
#define COL_DIM         RGB(0x4C,0x56,0x6A)
#define COL_SEL_BG      RGB(0x5E,0x81,0xAC)
#define COL_SEL_FG      RGB(0xFF,0xFF,0xFF)
#define COL_BORDER      RGB(0x3B,0x42,0x52)
#define COL_TIMER       RGB(0xA3,0xBE,0x8C)
#define COL_HIGHLIGHT   RGB(0xEB,0xCB,0x8B)
#define COL_ERROR       RGB(0xBF,0x61,0x6A)
#define COL_SUCCESS     RGB(0xA3,0xBE,0x8C)
#define COL_LOGO1       RGB(0x5E,0x81,0xAC)
#define COL_LOGO2       RGB(0x88,0xC0,0xD0)

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    UINT32  *fb;
    UINTN    fb_size;
    UINT32   width;
    UINT32   height;
    UINT32   stride;
    BOOLEAN  ready;
} GopCtx;

extern GopCtx g_gop;

EFI_STATUS gop_init(EFI_SYSTEM_TABLE *ST);
void       gop_clear(Pixel c);
void       gop_put_pixel(UINT32 x, UINT32 y, Pixel c);
void       gop_fill_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, Pixel c);
void       gop_draw_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 t, Pixel c);
void       gop_draw_hline_px(UINT32 x, UINT32 y, UINT32 len, UINT32 t, Pixel c);
void       gop_fill_rect_alpha(UINT32 x, UINT32 y, UINT32 w, UINT32 h, Pixel c, UINT8 alpha);
Pixel      gop_blend(Pixel src, Pixel dst, UINT8 alpha);
void       gop_draw_char(UINT32 x, UINT32 y, char ch, Pixel fg, Pixel bg, BOOLEAN transp);
void       gop_draw_str(UINT32 x, UINT32 y, const char *s, Pixel fg, Pixel bg, BOOLEAN transp);
void       gop_draw_wstr(UINT32 x, UINT32 y, const CHAR16 *s, Pixel fg, Pixel bg, BOOLEAN transp);
UINT32     gop_str_width(const char *s);
UINT32     gop_wstr_width(const CHAR16 *s);
UINT32     gop_char_width(void);
UINT32     gop_char_height(void);

#endif /* GOP_H */