#include "ui_login.h"
#include "theme.h"
#include "resource.h"
#include <string.h>
#include <stdio.h>

#define WND_W 860
#define WND_H 520
#define BRAND_W 340

static const char *CLASS_LOGIN = "CSInputLoginWnd";

typedef struct {
    HWND hUser, hPass, hBtn, hError;
    HWND hMain;
    BOOL success;
    AppUser user;
    int btnState; /* 0 normal 1 hover 2 pressed */
} LoginCtx;

static LoginCtx g_ctx;

static void DoLoginAttempt(HWND hwnd) {
    char user[FIELD_LEN], pass[FIELD_LEN];
    GetWindowTextA(g_ctx.hUser, user, sizeof(user));
    GetWindowTextA(g_ctx.hPass, pass, sizeof(pass));

    if (strlen(user) == 0 || strlen(pass) == 0) {
        SetWindowTextA(g_ctx.hError, "Username dan password wajib diisi.");
        ShowWindow(g_ctx.hError, SW_SHOW);
        return;
    }

    AppUser found;
    if (users_verify(user, pass, &found)) {
        g_ctx.success = TRUE;
        g_ctx.user = found;
        DestroyWindow(hwnd);
    } else {
        SetWindowTextA(g_ctx.hError, "Username atau password salah. Silakan coba lagi.");
        ShowWindow(g_ctx.hError, SW_SHOW);
        SetWindowTextA(g_ctx.hPass, "");
        SetFocus(g_ctx.hPass);
    }
}

static LRESULT CALLBACK LoginWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        RECT rc; GetClientRect(hwnd, &rc);
        int formLeft = BRAND_W + 60;
        int formW = (rc.right - formLeft) - 60;

        CreateWindowExA(0, "STATIC", "", SS_OWNERDRAW, 0, 0, rc.right, rc.bottom, hwnd, NULL, NULL, NULL); /* background placeholder unused */

        int y = 150;
        CreateWindowExA(0, "STATIC", "Selamat Datang", WS_CHILD | WS_VISIBLE,
            formLeft, y, formW, 34, hwnd, NULL, NULL, NULL);
        y += 40;
        CreateWindowExA(0, "STATIC", "Masuk untuk mengelola data customer PRIMAPER", WS_CHILD | WS_VISIBLE,
            formLeft, y, formW, 24, hwnd, NULL, NULL, NULL);

        y += 55;
        CreateWindowExA(0, "STATIC", "Username", WS_CHILD | WS_VISIBLE, formLeft, y, formW, 20, hwnd, NULL, NULL, NULL);
        y += 24;
        g_ctx.hUser = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            formLeft, y, formW, 32, hwnd, (HMENU)(INT_PTR)IDC_LOGIN_USER, NULL, NULL);

        y += 50;
        CreateWindowExA(0, "STATIC", "Password", WS_CHILD | WS_VISIBLE, formLeft, y, formW, 20, hwnd, NULL, NULL, NULL);
        y += 24;
        g_ctx.hPass = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
            formLeft, y, formW, 32, hwnd, (HMENU)(INT_PTR)IDC_LOGIN_PASS, NULL, NULL);

        y += 44;
        g_ctx.hError = CreateWindowExA(0, "STATIC", "", WS_CHILD, formLeft, y, formW, 20, hwnd, (HMENU)(INT_PTR)IDC_LOGIN_ERROR, NULL, NULL);

        y += 30;
        g_ctx.hBtn = CreateWindowExA(0, "BUTTON", "Masuk", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            formLeft, y, formW, 42, hwnd, (HMENU)(INT_PTR)IDC_LOGIN_BTN, NULL, NULL);

        HFONT bodyFont = theme_font(FONT_BODY);
        HFONT titleFont = theme_font(FONT_SUBTITLE);
        SendMessage(g_ctx.hUser, WM_SETFONT, (WPARAM)bodyFont, TRUE);
        SendMessage(g_ctx.hPass, WM_SETFONT, (WPARAM)bodyFont, TRUE);
        SendMessage(g_ctx.hError, WM_SETFONT, (WPARAM)theme_font(FONT_SMALL), TRUE);

        /* set fonts + text for the static labels we created inline by enumerating children */
        HWND child = GetWindow(hwnd, GW_CHILD);
        int idx = 0;
        while (child) {
            char cls[32];
            GetClassNameA(child, cls, sizeof(cls));
            if (strcmp(cls, "Static") == 0 && child != g_ctx.hError) {
                SendMessage(child, WM_SETFONT, (WPARAM)bodyFont, TRUE);
            }
            child = GetWindow(child, GW_HWNDNEXT);
            idx++;
        }
        SetWindowTextA(hwnd, "CS Input - Masuk");
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        HWND ctl = (HWND)lp;
        SetBkMode(hdc, TRANSPARENT);
        if (ctl == g_ctx.hError) {
            SetTextColor(hdc, CLR_DANGER);
        } else {
            SetTextColor(hdc, CLR_TEXT_DARK);
        }
        return (LRESULT)theme_brush(CLR_CARD);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(0xFA, 0xFB, 0xFD));
        SetTextColor(hdc, CLR_TEXT_DARK);
        return (LRESULT)theme_brush(RGB(0xFA, 0xFB, 0xFD));
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis->CtlID == IDC_LOGIN_BTN) {
            int state = 0;
            if (dis->itemState & ODS_SELECTED) state = 2;
            else if (g_ctx.btnState == 1) state = 1;
            RECT rc = dis->rcItem;
            theme_draw_flat_button(dis->hDC, rc, "Masuk", CLR_ACCENT, RGB(255,255,255), state, 8);
            return TRUE;
        }
        return FALSE;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        RECT brandRc = rc; brandRc.right = BRAND_W;
        RECT formRc = rc; formRc.left = BRAND_W;

        HBRUSH brand = theme_brush(CLR_SIDEBAR);
        FillRect(hdc, &brandRc, brand);
        HBRUSH form = theme_brush(CLR_CARD);
        FillRect(hdc, &formRc, form);

        /* accent stripe */
        RECT stripe = { BRAND_W, 0, BRAND_W + 4, rc.bottom };
        FillRect(hdc, &stripe, theme_brush(CLR_ACCENT));

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_LIGHT);
        HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_TITLE));
        RECT titleRc = { 40, 190, BRAND_W - 20, 250 };
        DrawTextA(hdc, "CS Input", -1, &titleRc, DT_LEFT);
        SelectObject(hdc, theme_font(FONT_BODY));
        RECT subRc = { 40, 245, BRAND_W - 20, 340 };
        SetTextColor(hdc, RGB(0xB9,0xC3,0xDD));
        DrawTextA(hdc, "Aplikasi pengelolaan data customer PRIMAPER. Input, cari, dan pantau perkembangan customer dengan mudah.", -1, &subRc, DT_LEFT | DT_WORDBREAK);
        SelectObject(hdc, old);
        return TRUE;
    }

    case WM_MOUSEMOVE: {
        RECT rc; GetWindowRect(g_ctx.hBtn, &rc);
        POINT pt; GetCursorPos(&pt);
        int newState = PtInRect(&rc, pt) ? 1 : 0;
        if (newState != g_ctx.btnState) {
            g_ctx.btnState = newState;
            InvalidateRect(g_ctx.hBtn, NULL, FALSE);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_ctx.btnState = 0;
        InvalidateRect(g_ctx.hBtn, NULL, FALSE);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_LOGIN_BTN && HIWORD(wp) == BN_CLICKED) {
            DoLoginAttempt(hwnd);
            return 0;
        }
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_RETURN) { DoLoginAttempt(hwnd); return 0; }
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

/* subclass for Enter key inside edit controls */
static WNDPROC g_editOldProc;
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        DoLoginAttempt(GetParent(hwnd));
        return 0;
    }
    return CallWindowProcA(g_editOldProc, hwnd, msg, wp, lp);
}

BOOL ShowLoginWindow(HINSTANCE hInst, AppUser *outUser) {
    ZeroMemory(&g_ctx, sizeof(g_ctx));

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = LoginWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_LOGIN;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_APPICON));
    RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - WND_W) / 2, y = (sh - WND_H) / 2;

    HWND hwnd = CreateWindowExA(0, CLASS_LOGIN, "CS Input - Masuk",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, WND_W, WND_H, NULL, NULL, hInst, NULL);

    g_ctx.hMain = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetFocus(g_ctx.hUser);

    WNDPROC oldUser = (WNDPROC)SetWindowLongPtrA(g_ctx.hUser, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    g_editOldProc = oldUser;
    SetWindowLongPtrA(g_ctx.hPass, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB) {
            /* allow default tab handling */
        }
        if (!IsDialogMessageA(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            /* IsDialogMessage handled navigation; still allow WM_COMMAND etc via DispatchMessage already done */
        }
    }

    if (g_ctx.success && outUser) *outUser = g_ctx.user;
    return g_ctx.success;
}
