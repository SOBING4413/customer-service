#ifndef THEME_H
#define THEME_H

#include <windows.h>

/* ----- palette (2026-style: deep navy sidebar, soft indigo accent, off-white canvas) ----- */
#define CLR_SIDEBAR      RGB(0x14,0x1B,0x2E)   /* deep navy */
#define CLR_SIDEBAR_HOVER RGB(0x22,0x2C,0x45)
#define CLR_SIDEBAR_ACTIVE RGB(0x4C,0x6F,0xFF) /* accent indigo */
#define CLR_ACCENT       RGB(0x4C,0x6F,0xFF)
#define CLR_ACCENT_DARK  RGB(0x3A,0x57,0xD6)
#define CLR_CANVAS       RGB(0xF4,0xF6,0xFB)   /* app background */
#define CLR_CARD         RGB(0xFF,0xFF,0xFF)
#define CLR_BORDER       RGB(0xE3,0xE7,0xF0)
#define CLR_TEXT_DARK    RGB(0x1B,0x22,0x33)
#define CLR_TEXT_MUTED   RGB(0x6B,0x74,0x8A)
#define CLR_TEXT_LIGHT   RGB(0xE9,0xEC,0xF5)
#define CLR_SUCCESS      RGB(0x1F,0xB6,0x6B)
#define CLR_WARNING      RGB(0xF5,0xA6,0x23)
#define CLR_DANGER       RGB(0xE5,0x4D,0x4D)
#define CLR_DANGER_DARK  RGB(0xC5,0x35,0x35)

typedef enum {
    FONT_TITLE = 0,
    FONT_SUBTITLE,
    FONT_BODY,
    FONT_BODY_BOLD,
    FONT_SMALL,
    FONT_BUTTON,
    FONT_STAT_NUM,
    FONT_COUNT
} ThemeFontId;

void   theme_init(void);
void   theme_cleanup(void);
HFONT  theme_font(ThemeFontId id);
HBRUSH theme_brush(COLORREF c);

/* draws a flat rounded rectangle button; state: 0 normal, 1 hover, 2 pressed */
void theme_draw_flat_button(HDC hdc, RECT rc, const char *text, COLORREF bg, COLORREF fg, int state, int radius);
/* draws a card panel with light border and subtle top accent */
void theme_draw_card(HDC hdc, RECT rc, int radius);

#endif
