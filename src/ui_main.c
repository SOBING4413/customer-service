#include "ui_main.h"
#include "theme.h"
#include "resource.h"
#include "ui_forms.h"
#include <commctrl.h>
#include <string.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")

#define SIDEBAR_W 230
#define HEADER_H  64

typedef enum { PANEL_DASHBOARD = 0, PANEL_CUSTOMERS, PANEL_ANALYSIS, PANEL_ABOUT, PANEL_COUNT } PanelId;

typedef struct {
    HINSTANCE hInst;
    HWND hMain;
    HWND hDashboard, hCustomers, hAnalysis, hAbout;
    HWND navBtn[4];
    HWND navLogout;
    int  navHover; /* -1 none, else index into navBtn, 100 = logout */
    PanelId currentPanel;
    AppUser user;
    CustomerStore *store;
    BOOL logoutRequested;
} Shell;

static Shell g_shell;

static const char *PANEL_TITLES[PANEL_COUNT] = { "Dashboard", "Data Customer", "Analisis & Tren", "Tentang Aplikasi" };
static const char *PANEL_SUBTITLES[PANEL_COUNT] = {
    "Ringkasan performa customer PRIMAPER",
    "Kelola data customer: tambah, ubah, hapus, dan cari",
    "Bandingkan performa customer antar tahun (otomatis mengikuti data terbaru)",
    "Informasi seputar aplikasi CS Input"
};
static const char *NAV_LABELS[4] = { "Dashboard", "Data Customer", "Analisis & Tren", "Tentang" };

/* forward decls for panel window procs */
static LRESULT CALLBACK DashboardWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK CustPanelWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK AnalysisWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK AboutWndProc(HWND, UINT, WPARAM, LPARAM);

/* ======================================================================
   Small shared drawing helpers
   ====================================================================== */

static void DrawCardTitle(HDC hdc, RECT rc, const char *title) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT_DARK);
    HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_BODY_BOLD));
    RECT tr = rc; tr.bottom = tr.top + 26;
    DrawTextA(hdc, title, -1, &tr, DT_LEFT | DT_VCENTER);
    SelectObject(hdc, old);
}

static void DrawBarList(HDC hdc, RECT rc, LabelCount *arr, int n, COLORREF barColor) {
    if (n == 0) {
        SetTextColor(hdc, CLR_TEXT_MUTED);
        HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_SMALL));
        DrawTextA(hdc, "Belum ada data.", -1, &rc, DT_LEFT | DT_TOP);
        SelectObject(hdc, old);
        return;
    }
    int maxCount = 1;
    for (int i = 0; i < n; i++) if (arr[i].count > maxCount) maxCount = arr[i].count;

    int rowH = 30;
    int y = rc.top;
    int labelW = 140;
    int barAreaX = rc.left + labelW;
    int barAreaW = rc.right - barAreaX - 40;
    HFONT oldFont = (HFONT)SelectObject(hdc, theme_font(FONT_SMALL));
    for (int i = 0; i < n && y + rowH <= rc.bottom; i++) {
        RECT lr = { rc.left, y, barAreaX - 8, y + rowH };
        SetTextColor(hdc, CLR_TEXT_DARK);
        DrawTextA(hdc, arr[i].label, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        int bw = (int)((double)arr[i].count / maxCount * barAreaW);
        if (bw < 4) bw = 4;
        RECT br = { barAreaX, y + 6, barAreaX + bw, y + rowH - 6 };
        HBRUSH b = theme_brush(barColor);
        FillRect(hdc, &br, b);

        char numbuf[16];
        sprintf(numbuf, "%d", arr[i].count);
        RECT nr = { barAreaX + barAreaW + 6, y, rc.right, y + rowH };
        SetTextColor(hdc, CLR_TEXT_MUTED);
        DrawTextA(hdc, numbuf, -1, &nr, DT_LEFT | DT_VCENTER);
        y += rowH;
    }
    SelectObject(hdc, oldFont);
}

static void DrawMonthlyChart(HDC hdc, RECT rc, LabelCount *months, int n, COLORREF barColor) {
    int maxCount = 1;
    for (int i = 0; i < n; i++) if (months[i].count > maxCount) maxCount = months[i].count;
    int gap = 8;
    int barW = (rc.right - rc.left - gap * (n + 1)) / n;
    if (barW < 6) barW = 6;
    int chartBottom = rc.bottom - 20;
    int chartTop = rc.top + 6;
    int chartH = chartBottom - chartTop;

    HFONT oldFont = (HFONT)SelectObject(hdc, theme_font(FONT_SMALL));
    int x = rc.left + gap;
    for (int i = 0; i < n; i++) {
        int h = (int)((double)months[i].count / maxCount * chartH);
        RECT br = { x, chartBottom - h, x + barW, chartBottom };
        HBRUSH b = theme_brush(months[i].count > 0 ? barColor : CLR_BORDER);
        FillRect(hdc, &br, b);

        if (months[i].count > 0) {
            char buf[8]; sprintf(buf, "%d", months[i].count);
            RECT nr = { x - 4, chartBottom - h - 18, x + barW + 4, chartBottom - h };
            SetTextColor(hdc, CLR_TEXT_MUTED);
            DrawTextA(hdc, buf, -1, &nr, DT_CENTER);
        }
        RECT lr = { x - 4, chartBottom + 2, x + barW + 4, chartBottom + 18 };
        SetTextColor(hdc, CLR_TEXT_MUTED);
        DrawTextA(hdc, months[i].label, -1, &lr, DT_CENTER);
        x += barW + gap;
    }
    SelectObject(hdc, oldFont);
    /* baseline */
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, rc.left, chartBottom, NULL);
    LineTo(hdc, rc.right, chartBottom);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void DrawMonthlyComparisonChart(HDC hdc, RECT rc, LabelCount *monthsA, LabelCount *monthsB,
                                        COLORREF colorA, COLORREF colorB,
                                        const char *labelA, const char *labelB) {
    int maxCount = 1;
    for (int i = 0; i < 12; i++) {
        if (monthsA[i].count > maxCount) maxCount = monthsA[i].count;
        if (monthsB[i].count > maxCount) maxCount = monthsB[i].count;
    }
    int gap = 10;
    int groupW = (rc.right - rc.left - gap * 13) / 12;
    if (groupW < 14) groupW = 14;
    int barW = (groupW - 3) / 2;
    int chartBottom = rc.bottom - 20;
    int chartTop = rc.top + 24; /* leave room for the legend */
    int chartH = chartBottom - chartTop;

    /* legend, top-right of the chart area */
    HFONT oldFont = (HFONT)SelectObject(hdc, theme_font(FONT_SMALL));
    int lx = rc.right - 170, ly = rc.top;
    RECT swA = { lx, ly + 3, lx + 12, ly + 15 };
    FillRect(hdc, &swA, theme_brush(colorA));
    RECT ltA = { lx + 18, ly, lx + 80, ly + 18 };
    SetTextColor(hdc, CLR_TEXT_DARK);
    DrawTextA(hdc, labelA, -1, &ltA, DT_LEFT | DT_VCENTER);
    int lx2 = lx + 85;
    RECT swB = { lx2, ly + 3, lx2 + 12, ly + 15 };
    FillRect(hdc, &swB, theme_brush(colorB));
    RECT ltB = { lx2 + 18, ly, lx2 + 80, ly + 18 };
    DrawTextA(hdc, labelB, -1, &ltB, DT_LEFT | DT_VCENTER);

    int x = rc.left + gap;
    for (int i = 0; i < 12; i++) {
        int hA = (int)((double)monthsA[i].count / maxCount * chartH);
        int hB = (int)((double)monthsB[i].count / maxCount * chartH);
        RECT brA = { x, chartBottom - hA, x + barW, chartBottom };
        FillRect(hdc, &brA, theme_brush(monthsA[i].count > 0 ? colorA : CLR_BORDER));
        RECT brB = { x + barW + 3, chartBottom - hB, x + barW + 3 + barW, chartBottom };
        FillRect(hdc, &brB, theme_brush(monthsB[i].count > 0 ? colorB : CLR_BORDER));

        if (monthsA[i].count > 0) {
            char buf[8]; sprintf(buf, "%d", monthsA[i].count);
            RECT nr = { x - 6, chartBottom - hA - 16, x + barW + 6, chartBottom - hA };
            SetTextColor(hdc, CLR_TEXT_MUTED);
            DrawTextA(hdc, buf, -1, &nr, DT_CENTER);
        }
        if (monthsB[i].count > 0) {
            char buf[8]; sprintf(buf, "%d", monthsB[i].count);
            RECT nr = { x + barW - 3, chartBottom - hB - 16, x + barW * 2 + 9, chartBottom - hB };
            SetTextColor(hdc, CLR_TEXT_MUTED);
            DrawTextA(hdc, buf, -1, &nr, DT_CENTER);
        }
        RECT lr = { x - 6, chartBottom + 2, x + groupW + 6, chartBottom + 18 };
        SetTextColor(hdc, CLR_TEXT_MUTED);
        DrawTextA(hdc, monthsA[i].label, -1, &lr, DT_CENTER);
        x += groupW + gap;
    }
    SelectObject(hdc, oldFont);
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, rc.left, chartBottom, NULL);
    LineTo(hdc, rc.right, chartBottom);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

/* ======================================================================
   Main shell window
   ====================================================================== */

static void PositionSidebarButtons(void) {
    RECT rc; GetClientRect(g_shell.hMain, &rc);
    int y = 150;
    for (int i = 0; i < 4; i++) {
        MoveWindow(g_shell.navBtn[i], 16, y, SIDEBAR_W - 32, 44, TRUE);
        y += 52;
    }
    MoveWindow(g_shell.navLogout, 16, rc.bottom - 70, SIDEBAR_W - 32, 44, TRUE);
}

static void ShowPanel(PanelId p) {
    g_shell.currentPanel = p;
    ShowWindow(g_shell.hDashboard, p == PANEL_DASHBOARD ? SW_SHOW : SW_HIDE);
    ShowWindow(g_shell.hCustomers, p == PANEL_CUSTOMERS ? SW_SHOW : SW_HIDE);
    ShowWindow(g_shell.hAnalysis, p == PANEL_ANALYSIS ? SW_SHOW : SW_HIDE);
    ShowWindow(g_shell.hAbout, p == PANEL_ABOUT ? SW_SHOW : SW_HIDE);
    if (p == PANEL_DASHBOARD) InvalidateRect(g_shell.hDashboard, NULL, TRUE);
    if (p == PANEL_ANALYSIS) InvalidateRect(g_shell.hAnalysis, NULL, TRUE);
    if (p == PANEL_CUSTOMERS) SendMessageA(g_shell.hCustomers, WM_APP + 1, 0, 0); /* refresh list */
    if (p == PANEL_ANALYSIS) SendMessageA(g_shell.hAnalysis, WM_APP + 2, 0, 0);   /* refresh year pickers */
    InvalidateRect(g_shell.hMain, NULL, FALSE);
    for (int i = 0; i < 4; i++) InvalidateRect(g_shell.navBtn[i], NULL, FALSE);
}

static const char *CLASS_MAIN = "CSInputMainWnd";

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        for (int i = 0; i < 4; i++) {
            int id = IDC_NAV_DASHBOARD + i;
            g_shell.navBtn[i] = CreateWindowExA(0, "BUTTON", NAV_LABELS[i], WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                16, 150 + i * 52, SIDEBAR_W - 32, 44, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
        }
        g_shell.navLogout = CreateWindowExA(0, "BUTTON", "Keluar / Logout", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            16, 500, SIDEBAR_W - 32, 44, hwnd, (HMENU)(INT_PTR)IDC_NAV_LOGOUT, NULL, NULL);

        RECT rc; GetClientRect(hwnd, &rc);
        RECT content = { SIDEBAR_W, HEADER_H, rc.right, rc.bottom };
        g_shell.hDashboard = CreateWindowExA(0, "CSInputDashboardWnd", "", WS_CHILD | WS_VISIBLE,
            content.left, content.top, content.right - content.left, content.bottom - content.top,
            hwnd, NULL, g_shell.hInst, NULL);
        g_shell.hCustomers = CreateWindowExA(0, "CSInputCustPanel", "", WS_CHILD,
            content.left, content.top, content.right - content.left, content.bottom - content.top,
            hwnd, NULL, g_shell.hInst, NULL);
        g_shell.hAnalysis = CreateWindowExA(0, "CSInputAnalysisWnd", "", WS_CHILD,
            content.left, content.top, content.right - content.left, content.bottom - content.top,
            hwnd, NULL, g_shell.hInst, NULL);
        g_shell.hAbout = CreateWindowExA(0, "CSInputAboutWnd", "", WS_CHILD,
            content.left, content.top, content.right - content.left, content.bottom - content.top,
            hwnd, NULL, g_shell.hInst, NULL);

        g_shell.currentPanel = PANEL_DASHBOARD;
        g_shell.navHover = -1;
        return 0;
    }
    case WM_SIZE: {
        int cx = LOWORD(lp), cy = HIWORD(lp);
        PositionSidebarButtons();
        int w = cx - SIDEBAR_W, h = cy - HEADER_H;
        if (w < 0) w = 0; if (h < 0) h = 0;
        MoveWindow(g_shell.hDashboard, SIDEBAR_W, HEADER_H, w, h, TRUE);
        MoveWindow(g_shell.hCustomers, SIDEBAR_W, HEADER_H, w, h, TRUE);
        MoveWindow(g_shell.hAnalysis, SIDEBAR_W, HEADER_H, w, h, TRUE);
        MoveWindow(g_shell.hAbout, SIDEBAR_W, HEADER_H, w, h, TRUE);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        RECT sidebar = { 0, 0, SIDEBAR_W, rc.bottom };
        FillRect(hdc, &sidebar, theme_brush(CLR_SIDEBAR));
        RECT header = { SIDEBAR_W, 0, rc.right, HEADER_H };
        FillRect(hdc, &header, theme_brush(CLR_CARD));
        RECT hline = { SIDEBAR_W, HEADER_H - 1, rc.right, HEADER_H };
        FillRect(hdc, &hline, theme_brush(CLR_BORDER));

        SetBkMode(hdc, TRANSPARENT);
        /* brand */
        SetTextColor(hdc, RGB(255,255,255));
        HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_SUBTITLE));
        RECT br = { 24, 28, SIDEBAR_W - 16, 60 };
        DrawTextA(hdc, "CS Input", -1, &br, DT_LEFT);
        SelectObject(hdc, theme_font(FONT_SMALL));
        SetTextColor(hdc, RGB(0x8B,0x95,0xB5));
        RECT br2 = { 24, 58, SIDEBAR_W - 16, 78 };
        DrawTextA(hdc, "PRIMAPER Customer Manager", -1, &br2, DT_LEFT);

        /* user info footer */
        SetTextColor(hdc, RGB(0xB9,0xC3,0xDD));
        RECT ur = { 24, 456, SIDEBAR_W - 16, 496 };
        char ubuf[160];
        sprintf(ubuf, "Masuk sebagai:\n%s", g_shell.user.nama[0] ? g_shell.user.nama : g_shell.user.username);
        DrawTextA(hdc, ubuf, -1, &ur, DT_LEFT);

        /* header title */
        SelectObject(hdc, theme_font(FONT_SUBTITLE));
        SetTextColor(hdc, CLR_TEXT_DARK);
        RECT tr = { SIDEBAR_W + 28, 10, rc.right - 260, 34 };
        DrawTextA(hdc, PANEL_TITLES[g_shell.currentPanel], -1, &tr, DT_LEFT);
        SelectObject(hdc, theme_font(FONT_SMALL));
        SetTextColor(hdc, CLR_TEXT_MUTED);
        RECT sr = { SIDEBAR_W + 28, 34, rc.right - 260, 56 };
        DrawTextA(hdc, PANEL_SUBTITLES[g_shell.currentPanel], -1, &sr, DT_LEFT);
        SelectObject(hdc, old);
        return TRUE;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis->CtlID == IDC_NAV_LOGOUT) {
            int state = (dis->itemState & ODS_SELECTED) ? 2 : (g_shell.navHover == 100 ? 1 : 0);
            COLORREF bg = state == 0 ? CLR_SIDEBAR : (state == 1 ? CLR_SIDEBAR_HOVER : CLR_SIDEBAR_HOVER);
            RECT rc = dis->rcItem;
            HBRUSH b = theme_brush(bg);
            FillRect(dis->hDC, &rc, b);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, RGB(0xFF,0x8A,0x8A));
            HFONT old = (HFONT)SelectObject(dis->hDC, theme_font(FONT_BUTTON));
            DrawTextA(dis->hDC, "Keluar / Logout", -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, old);
            return TRUE;
        }
        for (int i = 0; i < 4; i++) {
            if ((int)dis->CtlID == IDC_NAV_DASHBOARD + i) {
                BOOL active = (g_shell.currentPanel == i);
                int state = (dis->itemState & ODS_SELECTED) ? 2 : (g_shell.navHover == i ? 1 : 0);
                COLORREF bg = active ? CLR_SIDEBAR_ACTIVE : (state == 1 ? CLR_SIDEBAR_HOVER : CLR_SIDEBAR);
                COLORREF fg = active ? RGB(255,255,255) : RGB(0xC7,0xCF,0xE6);
                RECT rc = dis->rcItem;
                HBRUSH b = theme_brush(bg);
                if (active) {
                    HPEN pen = CreatePen(PS_SOLID, 1, bg);
                    HPEN oldp = (HPEN)SelectObject(dis->hDC, pen);
                    HBRUSH oldb = (HBRUSH)SelectObject(dis->hDC, b);
                    RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
                    SelectObject(dis->hDC, oldp); SelectObject(dis->hDC, oldb);
                    DeleteObject(pen);
                } else {
                    FillRect(dis->hDC, &rc, b);
                }
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, fg);
                HFONT old = (HFONT)SelectObject(dis->hDC, theme_font(FONT_BUTTON));
                RECT tr = rc; tr.left += 16;
                DrawTextA(dis->hDC, NAV_LABELS[i], -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(dis->hDC, old);
                return TRUE;
            }
        }
        return FALSE;
    }
    case WM_MOUSEMOVE: {
        POINT pt; GetCursorPos(&pt);
        int newHover = -1;
        for (int i = 0; i < 4; i++) {
            RECT rc; GetWindowRect(g_shell.navBtn[i], &rc);
            if (PtInRect(&rc, pt)) { newHover = i; break; }
        }
        if (newHover == -1) {
            RECT rc; GetWindowRect(g_shell.navLogout, &rc);
            if (PtInRect(&rc, pt)) newHover = 100;
        }
        if (newHover != g_shell.navHover) {
            int old = g_shell.navHover;
            g_shell.navHover = newHover;
            if (old >= 0 && old < 4) InvalidateRect(g_shell.navBtn[old], NULL, FALSE);
            if (old == 100) InvalidateRect(g_shell.navLogout, NULL, FALSE);
            if (newHover >= 0 && newHover < 4) InvalidateRect(g_shell.navBtn[newHover], NULL, FALSE);
            if (newHover == 100) InvalidateRect(g_shell.navLogout, NULL, FALSE);
        }
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            int id = LOWORD(wp);
            if (id >= IDC_NAV_DASHBOARD && id <= IDC_NAV_ABOUT) {
                ShowPanel((PanelId)(id - IDC_NAV_DASHBOARD));
                return 0;
            }
            if (id == IDC_NAV_LOGOUT) {
                if (ShowConfirmDialog(hwnd, "Logout", "Keluar dari akun saat ini?")) {
                    g_shell.logoutRequested = TRUE;
                    DestroyWindow(hwnd);
                }
                return 0;
            }
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ======================================================================
   Dashboard panel
   ====================================================================== */

static void DrawStatCard(HDC hdc, RECT rc, const char *label, const char *value, COLORREF accent) {
    theme_draw_card(hdc, rc, 10);
    RECT accentBar = { rc.left, rc.top, rc.left + 5, rc.bottom };
    theme_draw_card(hdc, accentBar, 0); /* placeholder no-op */
    HBRUSH ab = theme_brush(accent);
    RECT stripe = { rc.left, rc.top + 8, rc.left + 5, rc.bottom - 8 };
    FillRect(hdc, &stripe, ab);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT_MUTED);
    HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_SMALL));
    RECT lr = { rc.left + 20, rc.top + 16, rc.right - 12, rc.top + 36 };
    DrawTextA(hdc, label, -1, &lr, DT_LEFT);

    SelectObject(hdc, theme_font(FONT_STAT_NUM));
    SetTextColor(hdc, CLR_TEXT_DARK);
    RECT vr = { rc.left + 18, rc.top + 38, rc.right - 12, rc.bottom - 10 };
    DrawTextA(hdc, value, -1, &vr, DT_LEFT);
    SelectObject(hdc, old);
}

static LRESULT CALLBACK DashboardWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme_brush(CLR_CANVAS));

        CustomerStore *s = g_shell.store;
        int total = store_total(s);

        int years[64];
        int yearCount = store_distinct_years(s, years, 64);
        int yearPrev = yearCount >= 2 ? years[yearCount - 2] : (yearCount == 1 ? years[0] : 0);
        int yearLatest = yearCount >= 1 ? years[yearCount - 1] : 0;
        char yearPrevStr[8], yearLatestStr[8];
        sprintf(yearPrevStr, "%d", yearPrev);
        sprintf(yearLatestStr, "%d", yearLatest);

        int cPrev = yearCount > 0 ? store_count_by_year(s, yearPrevStr) : 0;
        int cLatest = yearCount > 0 ? store_count_by_year(s, yearLatestStr) : 0;
        LabelCount *mLatest = yearCount > 0 ? store_monthly_for_year(s, yearLatestStr) : NULL;
        static LabelCount emptyMonths[12];
        if (!mLatest) { memset(emptyMonths, 0, sizeof(emptyMonths)); mLatest = emptyMonths; }

        int pad = 24, gap = 16;
        int cardW = (rc.right - pad * 2 - gap * 3) / 4;
        int cardH = 100;
        char buf[32];

        RECT c1 = { pad, pad, pad + cardW, pad + cardH };
        sprintf(buf, "%d", total);
        DrawStatCard(hdc, c1, "TOTAL CUSTOMER", buf, CLR_ACCENT);

        RECT c2 = { c1.right + gap, pad, c1.right + gap + cardW, pad + cardH };
        char lbl2[32]; sprintf(lbl2, "CUSTOMER TAHUN %d", yearPrev);
        sprintf(buf, "%d", cPrev);
        DrawStatCard(hdc, c2, yearCount > 0 ? lbl2 : "CUSTOMER TAHUN LALU", buf, CLR_SUCCESS);

        RECT c3 = { c2.right + gap, pad, c2.right + gap + cardW, pad + cardH };
        char lbl3[32]; sprintf(lbl3, "CUSTOMER TAHUN %d", yearLatest);
        sprintf(buf, "%d", cLatest);
        DrawStatCard(hdc, c3, yearCount > 0 ? lbl3 : "CUSTOMER TAHUN INI", buf, CLR_WARNING);

        RECT c4 = { c3.right + gap, pad, c3.right + gap + cardW, pad + cardH };
        double growth = cPrev > 0 ? ((double)cLatest - cPrev) / cPrev * 100.0 : 0;
        sprintf(buf, "%+.0f%%", growth);
        char lbl4[40]; sprintf(lbl4, "PERTUMBUHAN %d->%d", yearPrev, yearLatest);
        DrawStatCard(hdc, c4, yearCount >= 2 ? lbl4 : "PERTUMBUHAN", buf, CLR_DANGER);

        int rowY = pad + cardH + gap;
        int chartH = 230;
        RECT chartCard = { pad, rowY, pad + (rc.right - pad * 2) * 6 / 10, rowY + chartH };
        theme_draw_card(hdc, chartCard, 10);
        RECT chartTitle = { chartCard.left + 18, chartCard.top + 12, chartCard.right - 12, chartCard.top + 38 };
        char chartTitleBuf[48]; sprintf(chartTitleBuf, "Tren Customer Baru per Bulan - %d", yearLatest);
        DrawCardTitle(hdc, chartTitle, yearCount > 0 ? chartTitleBuf : "Tren Customer Baru per Bulan");
        RECT chartArea = { chartCard.left + 18, chartCard.top + 44, chartCard.right - 18, chartCard.bottom - 14 };
        DrawMonthlyChart(hdc, chartArea, mLatest, 12, CLR_ACCENT);

        int rightX = chartCard.right + gap;
        int rightW = rc.right - pad - rightX;
        RECT topCabangCard = { rightX, rowY, rightX + rightW, rowY + chartH / 2 - gap / 2 };
        theme_draw_card(hdc, topCabangCard, 10);
        RECT tcTitle = { topCabangCard.left + 16, topCabangCard.top + 10, topCabangCard.right - 10, topCabangCard.top + 32 };
        DrawCardTitle(hdc, tcTitle, "Top Wilayah Cabang");
        int n1 = 0;
        LabelCount *cabang = store_top_group(s, 0, 4, &n1);
        RECT cbArea = { topCabangCard.left + 16, topCabangCard.top + 36, topCabangCard.right - 12, topCabangCard.bottom - 8 };
        DrawBarList(hdc, cbArea, cabang, n1, CLR_ACCENT);
        free(cabang);

        RECT topSalesCard = { rightX, topCabangCard.bottom + gap, rightX + rightW, rowY + chartH };
        theme_draw_card(hdc, topSalesCard, 10);
        RECT tsTitle = { topSalesCard.left + 16, topSalesCard.top + 10, topSalesCard.right - 10, topSalesCard.top + 32 };
        DrawCardTitle(hdc, tsTitle, "Top Sales");
        int n2 = 0;
        LabelCount *sales = store_top_group(s, 1, 4, &n2);
        RECT tsArea = { topSalesCard.left + 16, topSalesCard.top + 36, topSalesCard.right - 12, topSalesCard.bottom - 8 };
        DrawBarList(hdc, tsArea, sales, n2, CLR_SUCCESS);
        free(sales);

        if (yearCount > 0) free(mLatest);
        return TRUE;
    }
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ======================================================================
   Customer list panel
   ====================================================================== */

static HWND g_custSearch, g_custList;
static HWND g_btnAdd, g_btnEdit, g_btnDelete, g_btnDetail, g_btnRefresh;
static int g_btnHover = -1; /* 0 add 1 edit 2 delete 3 detail 4 refresh */

static void CustPanel_Populate(HWND hwnd) {
    char query[FIELD_LEN];
    GetWindowTextA(g_custSearch, query, sizeof(query));

    ListView_DeleteAllItems(g_custList);
    int n = 0;
    Customer **filtered = store_filter(g_shell.store, query, &n);
    for (int i = 0; i < n; i++) {
        Customer *c = filtered[i];
        LVITEMA lvi = {0};
        char noBuf[16]; sprintf(noBuf, "%d", i + 1);
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = noBuf;
        lvi.lParam = (LPARAM)c->id;
        int idx = ListView_InsertItem(g_custList, &lvi);
        ListView_SetItemText(g_custList, idx, 1, c->wilayah_customer);
        ListView_SetItemText(g_custList, idx, 2, c->wilayah_cabang);
        ListView_SetItemText(g_custList, idx, 3, c->sales);
        ListView_SetItemText(g_custList, idx, 4, c->tanggal);
        ListView_SetItemText(g_custList, idx, 5, c->customer_number);
    }
    free(filtered);
    (void)hwnd;
}

static int CustPanel_GetSelectedId(void) {
    int idx = ListView_GetNextItem(g_custList, -1, LVNI_SELECTED);
    if (idx < 0) return -1;
    LVITEMA lvi = {0};
    lvi.mask = LVIF_PARAM;
    lvi.iItem = idx;
    ListView_GetItem(g_custList, &lvi);
    return (int)lvi.lParam;
}

static WNDPROC g_searchOldProc;
static LRESULT CALLBACK SearchSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT r = CallWindowProcA(g_searchOldProc, hwnd, msg, wp, lp);
    if (msg == WM_KEYUP || msg == WM_CHAR) {
        CustPanel_Populate(GetParent(hwnd));
    }
    return r;
}

static LRESULT CALLBACK CustPanelWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowExA(0, "STATIC", "Cari:", WS_CHILD | WS_VISIBLE, 20, 18, 40, 24, hwnd, NULL, NULL, NULL);
        g_custSearch = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            60, 16, 260, 28, hwnd, (HMENU)(INT_PTR)IDC_CUST_SEARCH, NULL, NULL);
        SendMessage(g_custSearch, WM_SETFONT, (WPARAM)theme_font(FONT_BODY), TRUE);
        g_searchOldProc = (WNDPROC)SetWindowLongPtrA(g_custSearch, GWLP_WNDPROC, (LONG_PTR)SearchSubclassProc);

        int bx = 340, bw = 118, bh = 32, gap = 8;
        g_btnAdd = CreateWindowExA(0, "BUTTON", "+ Tambah", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, bx, 14, bw, bh, hwnd, (HMENU)(INT_PTR)IDC_CUST_ADD, NULL, NULL);
        bx += bw + gap;
        g_btnEdit = CreateWindowExA(0, "BUTTON", "Ubah", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, bx, 14, bw, bh, hwnd, (HMENU)(INT_PTR)IDC_CUST_EDIT, NULL, NULL);
        bx += bw + gap;
        g_btnDelete = CreateWindowExA(0, "BUTTON", "Hapus", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, bx, 14, bw, bh, hwnd, (HMENU)(INT_PTR)IDC_CUST_DELETE, NULL, NULL);
        bx += bw + gap;
        g_btnDetail = CreateWindowExA(0, "BUTTON", "Detail", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, bx, 14, bw, bh, hwnd, (HMENU)(INT_PTR)IDC_CUST_DETAIL, NULL, NULL);
        bx += bw + gap;
        g_btnRefresh = CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, bx, 14, bw, bh, hwnd, (HMENU)(INT_PTR)IDC_CUST_REFRESH, NULL, NULL);

        g_custList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            20, 60, 800, 400, hwnd, (HMENU)(INT_PTR)IDC_CUST_LIST, NULL, NULL);
        ListView_SetExtendedListViewStyle(g_custList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        SendMessage(g_custList, WM_SETFONT, (WPARAM)theme_font(FONT_BODY), TRUE);

        LVCOLUMNA col = {0};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        struct { const char *name; int w; } cols[] = {
            {"No", 44}, {"Wilayah Customer", 170}, {"Wilayah Cabang", 150},
            {"Sales / Kepala Operasional", 190}, {"Tanggal", 100}, {"Nomor Customer", 150}
        };
        for (int i = 0; i < 6; i++) {
            col.pszText = (LPSTR)cols[i].name;
            col.cx = cols[i].w;
            col.iSubItem = i;
            ListView_InsertColumn(g_custList, i, &col);
        }
        return 0;
    }
    case WM_APP + 1:
        CustPanel_Populate(hwnd);
        return 0;
    case WM_SIZE: {
        int cx = LOWORD(lp), cy = HIWORD(lp);
        MoveWindow(g_custList, 20, 60, max(100, cx - 40), max(100, cy - 80), TRUE);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme_brush(CLR_CANVAS));
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_DARK);
        return (LRESULT)theme_brush(CLR_CANVAS);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(255,255,255));
        SetTextColor(hdc, CLR_TEXT_DARK);
        return (LRESULT)theme_brush(RGB(255,255,255));
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        struct { UINT id; const char *label; COLORREF bg; COLORREF fg; int hoverIdx; } map[] = {
            { IDC_CUST_ADD, "+ Tambah", CLR_ACCENT, RGB(255,255,255), 0 },
            { IDC_CUST_EDIT, "Ubah", RGB(0xEC,0xEE,0xF3), CLR_TEXT_DARK, 1 },
            { IDC_CUST_DELETE, "Hapus", RGB(0xFD,0xEC,0xEC), CLR_DANGER_DARK, 2 },
            { IDC_CUST_DETAIL, "Detail", RGB(0xEC,0xEE,0xF3), CLR_TEXT_DARK, 3 },
            { IDC_CUST_REFRESH, "Refresh", RGB(0xEC,0xEE,0xF3), CLR_TEXT_DARK, 4 },
        };
        for (int i = 0; i < 5; i++) {
            if (dis->CtlID == map[i].id) {
                int state = (dis->itemState & ODS_SELECTED) ? 2 : (g_btnHover == map[i].hoverIdx ? 1 : 0);
                theme_draw_flat_button(dis->hDC, dis->rcItem, map[i].label, map[i].bg, map[i].fg, state, 6);
                return TRUE;
            }
        }
        return FALSE;
    }
    case WM_MOUSEMOVE: {
        HWND btns[5] = { g_btnAdd, g_btnEdit, g_btnDelete, g_btnDetail, g_btnRefresh };
        POINT pt; GetCursorPos(&pt);
        int newHover = -1;
        for (int i = 0; i < 5; i++) {
            RECT rc; GetWindowRect(btns[i], &rc);
            if (PtInRect(&rc, pt)) { newHover = i; break; }
        }
        if (newHover != g_btnHover) {
            int old = g_btnHover;
            g_btnHover = newHover;
            if (old >= 0) InvalidateRect(btns[old], NULL, FALSE);
            if (newHover >= 0) InvalidateRect(btns[newHover], NULL, FALSE);
        }
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->hwndFrom == g_custList && nm->code == NM_DBLCLK) {
            int id = CustPanel_GetSelectedId();
            if (id >= 0) {
                Customer *c = store_find(g_shell.store, id);
                if (c) ShowCustomerDetailDialog(g_shell.hInst, g_shell.hMain, c);
            }
        }
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            int id = LOWORD(wp);
            if (id == IDC_CUST_ADD) {
                if (ShowCustomerFormDialog(g_shell.hInst, g_shell.hMain, g_shell.store, NULL))
                    CustPanel_Populate(hwnd);
            } else if (id == IDC_CUST_EDIT) {
                int cid = CustPanel_GetSelectedId();
                if (cid < 0) { MessageBoxA(hwnd, "Pilih data customer terlebih dahulu.", "Ubah Data", MB_OK | MB_ICONINFORMATION); return 0; }
                Customer *c = store_find(g_shell.store, cid);
                if (c && ShowCustomerFormDialog(g_shell.hInst, g_shell.hMain, g_shell.store, c))
                    CustPanel_Populate(hwnd);
            } else if (id == IDC_CUST_DELETE) {
                int cid = CustPanel_GetSelectedId();
                if (cid < 0) { MessageBoxA(hwnd, "Pilih data customer terlebih dahulu.", "Hapus Data", MB_OK | MB_ICONINFORMATION); return 0; }
                if (ShowConfirmDialog(hwnd, "Hapus Data", "Yakin ingin menghapus data customer ini?")) {
                    store_delete(g_shell.store, cid);
                    CustPanel_Populate(hwnd);
                }
            } else if (id == IDC_CUST_DETAIL) {
                int cid = CustPanel_GetSelectedId();
                if (cid < 0) { MessageBoxA(hwnd, "Pilih data customer terlebih dahulu.", "Detail Data", MB_OK | MB_ICONINFORMATION); return 0; }
                Customer *c = store_find(g_shell.store, cid);
                if (c) ShowCustomerDetailDialog(g_shell.hInst, g_shell.hMain, c);
            } else if (id == IDC_CUST_REFRESH) {
                SetWindowTextA(g_custSearch, "");
                CustPanel_Populate(hwnd);
            }
        }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ======================================================================
   Analysis panel
   ====================================================================== */

static HWND g_cmbYearA, g_cmbYearB;
static int  g_anYears[64];
static int  g_anYearCount = 0;

static void AnalysisPopulateYearPickers(HWND hwnd) {
    (void)hwnd;
    int curA = (int)SendMessageA(g_cmbYearA, CB_GETCURSEL, 0, 0);
    int curB = (int)SendMessageA(g_cmbYearB, CB_GETCURSEL, 0, 0);
    int prevA = (curA >= 0 && curA < g_anYearCount) ? g_anYears[curA] : -1;
    int prevB = (curB >= 0 && curB < g_anYearCount) ? g_anYears[curB] : -1;

    g_anYearCount = store_distinct_years(g_shell.store, g_anYears, 64);
    SendMessageA(g_cmbYearA, CB_RESETCONTENT, 0, 0);
    SendMessageA(g_cmbYearB, CB_RESETCONTENT, 0, 0);

    if (g_anYearCount == 0) return;

    char buf[8];
    int idxA = g_anYearCount >= 2 ? g_anYearCount - 2 : 0; /* default: previous year */
    int idxB = g_anYearCount - 1;                          /* default: latest year */
    for (int i = 0; i < g_anYearCount; i++) {
        sprintf(buf, "%d", g_anYears[i]);
        SendMessageA(g_cmbYearA, CB_ADDSTRING, 0, (LPARAM)buf);
        SendMessageA(g_cmbYearB, CB_ADDSTRING, 0, (LPARAM)buf);
        if (g_anYears[i] == prevA) idxA = i;
        if (g_anYears[i] == prevB) idxB = i;
    }
    SendMessageA(g_cmbYearA, CB_SETCURSEL, idxA, 0);
    SendMessageA(g_cmbYearB, CB_SETCURSEL, idxB, 0);
}

static LRESULT CALLBACK AnalysisWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowExA(0, "STATIC", "Bandingkan tahun", WS_CHILD | WS_VISIBLE,
            24, 14, 130, 24, hwnd, NULL, NULL, NULL);
        g_cmbYearA = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            150, 10, 90, 200, hwnd, (HMENU)(INT_PTR)IDC_ANALYSIS_YEARA, NULL, NULL);
        CreateWindowExA(0, "STATIC", "dengan", WS_CHILD | WS_VISIBLE,
            250, 14, 60, 24, hwnd, NULL, NULL, NULL);
        g_cmbYearB = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            310, 10, 90, 200, hwnd, (HMENU)(INT_PTR)IDC_ANALYSIS_YEARB, NULL, NULL);
        SendMessage(g_cmbYearA, WM_SETFONT, (WPARAM)theme_font(FONT_BODY), TRUE);
        SendMessage(g_cmbYearB, WM_SETFONT, (WPARAM)theme_font(FONT_BODY), TRUE);
        AnalysisPopulateYearPickers(hwnd);
        return 0;
    }
    case WM_APP + 2:
        AnalysisPopulateYearPickers(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wp) == CBN_SELCHANGE &&
            (LOWORD(wp) == IDC_ANALYSIS_YEARA || LOWORD(wp) == IDC_ANALYSIS_YEARB)) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme_brush(CLR_CANVAS));
        CustomerStore *s = g_shell.store;

        if (g_anYearCount == 0) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_TEXT_MUTED);
            RECT er = { 24, 60, rc.right - 24, 100 };
            DrawTextA(hdc, "Belum ada data customer untuk dianalisis.", -1, &er, DT_LEFT);
            return TRUE;
        }

        int idxA = (int)SendMessageA(g_cmbYearA, CB_GETCURSEL, 0, 0);
        int idxB = (int)SendMessageA(g_cmbYearB, CB_GETCURSEL, 0, 0);
        if (idxA < 0) idxA = 0;
        if (idxB < 0) idxB = g_anYearCount - 1;
        int yearA = g_anYears[idxA];
        int yearB = g_anYears[idxB];
        char yearAStr[8], yearBStr[8];
        sprintf(yearAStr, "%d", yearA);
        sprintf(yearBStr, "%d", yearB);

        int totalA = store_count_by_year(s, yearAStr);
        int totalB = store_count_by_year(s, yearBStr);
        int activeA = store_active_months_count(s, yearAStr);
        int activeB = store_active_months_count(s, yearBStr);
        double avgA = activeA > 0 ? (double)totalA / activeA : 0;
        double avgB = activeB > 0 ? (double)totalB / activeB : 0;
        double growthTotal = totalA > 0 ? ((double)totalB - totalA) / totalA * 100.0 : 0;
        double growthAvg = avgA > 0 ? (avgB - avgA) / avgA * 100.0 : 0;

        LabelCount *monthsA = store_monthly_for_year(s, yearAStr);
        LabelCount *monthsB = store_monthly_for_year(s, yearBStr);

        int pad = 24;
        RECT card1 = { pad, pad + 40, rc.right - pad, pad + 40 + 260 };
        theme_draw_card(hdc, card1, 10);
        RECT t1 = { card1.left + 18, card1.top + 12, card1.right - 12, card1.top + 36 };
        char titleBuf[64];
        sprintf(titleBuf, "Perbandingan Bulanan: %d vs %d", yearA, yearB);
        DrawCardTitle(hdc, t1, titleBuf);
        RECT chartArea = { card1.left + 18, card1.top + 44, card1.right - 18, card1.bottom - 14 };
        DrawMonthlyComparisonChart(hdc, chartArea, monthsA, monthsB, CLR_WARNING, CLR_ACCENT, yearAStr, yearBStr);
        free(monthsA);
        free(monthsB);

        int rowY = card1.bottom + 16;
        int colW = (rc.right - pad * 2 - 16) / 2;
        RECT card2 = { pad, rowY, pad + colW, rowY + 190 };
        theme_draw_card(hdc, card2, 10);
        RECT t2 = { card2.left + 18, card2.top + 12, card2.right - 12, card2.top + 36 };
        DrawCardTitle(hdc, t2, "Ringkasan Statistik");
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_DARK);
        HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_BODY));
        char buf[512];
        sprintf(buf,
            "- Total customer %d: %d (rata-rata %.1f/bulan aktif)\n"
            "- Total customer %d: %d (rata-rata %.1f/bulan aktif)\n"
            "- Pertumbuhan total %d -> %d: %+.0f%%\n"
            "- Pertumbuhan rata-rata per bulan: %+.0f%%",
            yearA, totalA, avgA, yearB, totalB, avgB, yearA, yearB, growthTotal, growthAvg);
        RECT br = { card2.left + 18, card2.top + 40, card2.right - 14, card2.bottom - 10 };
        DrawTextA(hdc, buf, -1, &br, DT_LEFT | DT_WORDBREAK);
        SelectObject(hdc, old);

        RECT card3 = { pad + colW + 16, rowY, pad + colW + 16 + colW, rowY + 190 };
        theme_draw_card(hdc, card3, 10);
        RECT t3 = { card3.left + 18, card3.top + 12, card3.right - 12, card3.top + 36 };
        DrawCardTitle(hdc, t3, "Proyeksi & Catatan");
        SetTextColor(hdc, CLR_TEXT_DARK);
        HFONT old2 = (HFONT)SelectObject(hdc, theme_font(FONT_BODY));
        char buf2[512];
        int pos = 0;
        if (activeB < 12 && avgB > 0) {
            int projectedFullB = totalB + (int)(avgB * (12 - activeB) + 0.5);
            pos += sprintf(buf2 + pos,
                "Estimasi total tahun %d (genap 12 bulan): %d\n", yearB, projectedFullB);
        }
        if (avgB > 0) {
            int projectedNext = (int)(avgB * 12 + 0.5);
            pos += sprintf(buf2 + pos,
                "Jika tren rata-rata/bulan tahun %d berlanjut,\nperkiraan customer baru tahun %d: %d\n",
                yearB, yearB + 1, projectedNext);
        }
        sprintf(buf2 + pos,
            "\nGunakan dropdown di atas untuk membandingkan\npasangan tahun lain.");
        RECT br2 = { card3.left + 18, card3.top + 40, card3.right - 14, card3.bottom - 10 };
        DrawTextA(hdc, buf2, -1, &br2, DT_LEFT | DT_WORDBREAK);
        SelectObject(hdc, old2);
        return TRUE;
    }
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ======================================================================
   About panel
   ====================================================================== */

static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme_brush(CLR_CANVAS));

        RECT card = { 24, 24, rc.right - 24, 320 };
        theme_draw_card(hdc, card, 10);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_DARK);
        HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_SUBTITLE));
        RECT tr = { card.left + 20, card.top + 18, card.right - 16, card.top + 44 };
        DrawTextA(hdc, "CS Input - PRIMAPER Customer Manager", -1, &tr, DT_LEFT);
        SelectObject(hdc, theme_font(FONT_BODY));
        const char *info =
            "Aplikasi desktop untuk membantu tim CS/Sales PRIMAPER mengelola data\n"
            "customer secara lokal, cepat, dan rapi.\n\n"
            "- Data disimpan otomatis dalam format JSON di folder data/\n"
            "- Login lokal berbasis file users.json\n"
            "- Dibangun dengan C (Win32 API) + pustaka cJSON\n\n"
            "Versi 1.0 - 2026";
        RECT ir = { card.left + 20, card.top + 56, card.right - 16, card.bottom - 16 };
        DrawTextA(hdc, info, -1, &ir, DT_LEFT | DT_WORDBREAK);
        SelectObject(hdc, old);
        return TRUE;
    }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ======================================================================
   Entry point
   ====================================================================== */

BOOL ShowMainShell(HINSTANCE hInst, AppUser *user, CustomerStore *store) {
    ZeroMemory(&g_shell, sizeof(g_shell));
    g_shell.hInst = hInst;
    g_shell.user = *user;
    g_shell.store = store;
    g_shell.navHover = -1;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSA wc = {0};
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_APPICON));

    wc.lpfnWndProc = MainWndProc;      wc.lpszClassName = CLASS_MAIN;             RegisterClassA(&wc);
    wc.lpfnWndProc = DashboardWndProc; wc.lpszClassName = "CSInputDashboardWnd";  RegisterClassA(&wc);
    wc.lpfnWndProc = CustPanelWndProc; wc.lpszClassName = "CSInputCustPanel";     RegisterClassA(&wc);
    wc.lpfnWndProc = AnalysisWndProc;  wc.lpszClassName = "CSInputAnalysisWnd";   RegisterClassA(&wc);
    wc.lpfnWndProc = AboutWndProc;     wc.lpszClassName = "CSInputAboutWnd";      RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int w = 1180, h = 720;
    if (w > sw - 40) w = sw - 40;
    if (h > sh - 40) h = sh - 40;
    int x = (sw - w) / 2, y = (sh - h) / 2;

    HWND hwnd = CreateWindowExA(0, CLASS_MAIN, "CS Input - PRIMAPER Customer Manager",
        WS_OVERLAPPEDWINDOW, x, y, w, h, NULL, NULL, hInst, NULL);
    g_shell.hMain = hwnd;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    ShowPanel(PANEL_DASHBOARD);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return g_shell.logoutRequested;
}
