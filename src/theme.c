#include "theme.h"

static HFONT g_fonts[FONT_COUNT];
static HBRUSH g_brushCache[16];
static COLORREF g_brushColors[16];
static int g_brushCount = 0;

static HFONT mkfont(int size, int weight, const char *face) {
    return CreateFontA(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
}

void theme_init(void) {
    g_fonts[FONT_TITLE]     = mkfont(26, FW_SEMIBOLD, "Segoe UI");
    g_fonts[FONT_SUBTITLE]  = mkfont(16, FW_SEMIBOLD, "Segoe UI");
    g_fonts[FONT_BODY]      = mkfont(14, FW_NORMAL, "Segoe UI");
    g_fonts[FONT_BODY_BOLD] = mkfont(14, FW_SEMIBOLD, "Segoe UI");
    g_fonts[FONT_SMALL]     = mkfont(12, FW_NORMAL, "Segoe UI");
    g_fonts[FONT_BUTTON]    = mkfont(14, FW_SEMIBOLD, "Segoe UI");
    g_fonts[FONT_STAT_NUM]  = mkfont(30, FW_BOLD, "Segoe UI");
    g_brushCount = 0;
}

void theme_cleanup(void) {
    for (int i = 0; i < FONT_COUNT; i++) if (g_fonts[i]) DeleteObject(g_fonts[i]);
    for (int i = 0; i < g_brushCount; i++) if (g_brushCache[i]) DeleteObject(g_brushCache[i]);
}

HFONT theme_font(ThemeFontId id) { return g_fonts[id]; }

HBRUSH theme_brush(COLORREF c) {
    for (int i = 0; i < g_brushCount; i++) {
        if (g_brushColors[i] == c) return g_brushCache[i];
    }
    if (g_brushCount < 16) {
        HBRUSH b = CreateSolidBrush(c);
        g_brushColors[g_brushCount] = c;
        g_brushCache[g_brushCount] = b;
        g_brushCount++;
        return b;
    }
    return CreateSolidBrush(c); /* fallback, leaked but rare */
}

void theme_draw_card(HDC hdc, RECT rc, int radius) {
    HBRUSH bg = theme_brush(CLR_CARD);
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bg);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

void theme_draw_flat_button(HDC hdc, RECT rc, const char *text, COLORREF bg, COLORREF fg, int state, int radius) {
    COLORREF drawBg = bg;
    if (state == 1) {
        int r = GetRValue(bg), g = GetGValue(bg), b = GetBValue(bg);
        r = max(0, r - 14); g = max(0, g - 14); b = max(0, b - 14);
        drawBg = RGB(r, g, b);
    } else if (state == 2) {
        int r = GetRValue(bg), g = GetGValue(bg), b = GetBValue(bg);
        r = max(0, r - 28); g = max(0, g - 28); b = max(0, b - 28);
        drawBg = RGB(r, g, b);
    }
    HBRUSH brush = CreateSolidBrush(drawBg);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, drawBg);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    if (radius > 0)
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    else
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    HFONT oldFont = (HFONT)SelectObject(hdc, theme_font(FONT_BUTTON));
    DrawTextA(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}
