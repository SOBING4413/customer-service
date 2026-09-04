#include "ui_forms.h"
#include "theme.h"
#include "resource.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================ shared helpers ============================ */

static BOOL looks_like_date(const char *s) {
    if (strlen(s) != 10) return FALSE;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) { if (s[i] != '-') return FALSE; }
        else if (!isdigit((unsigned char)s[i])) return FALSE;
    }
    return TRUE;
}

/* ============================ Add/Edit form ============================ */

static const char *CLASS_FORM = "CSInputFormWnd";

typedef struct {
    HWND hWilCust, hWilCabang, hSales, hTanggal, hNomor, hCatatan;
    HWND hSave, hCancel, hError;
    HWND hOwner;
    CustomerStore *store;
    Customer *editing; /* NULL = add new */
    BOOL saved;
    int saveState, cancelState;
} FormCtx;

static FormCtx g_form;

static void CreateLabeledEdit(HWND hwnd, const char *label, int x, int y, int w, HWND *outEdit, int id, DWORD extraStyle) {
    CreateWindowExA(0, "STATIC", label, WS_CHILD | WS_VISIBLE, x, y, w, 20, hwnd, NULL, NULL, NULL);
    *outEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | extraStyle,
        x, y + 22, w, extraStyle & ES_MULTILINE ? 60 : 30, hwnd, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessage(*outEdit, WM_SETFONT, (WPARAM)theme_font(FONT_BODY), TRUE);
}

static void FormDoSave(HWND hwnd) {
    Customer c;
    memset(&c, 0, sizeof(c));
    GetWindowTextA(g_form.hWilCust, c.wilayah_customer, sizeof(c.wilayah_customer));
    GetWindowTextA(g_form.hWilCabang, c.wilayah_cabang, sizeof(c.wilayah_cabang));
    GetWindowTextA(g_form.hSales, c.sales, sizeof(c.sales));
    GetWindowTextA(g_form.hTanggal, c.tanggal, sizeof(c.tanggal));
    GetWindowTextA(g_form.hNomor, c.customer_number, sizeof(c.customer_number));
    GetWindowTextA(g_form.hCatatan, c.catatan, sizeof(c.catatan));

    if (!strlen(c.wilayah_customer) || !strlen(c.wilayah_cabang) || !strlen(c.sales) ||
        !strlen(c.tanggal) || !strlen(c.customer_number)) {
        SetWindowTextA(g_form.hError, "Semua field wajib diisi, kecuali catatan.");
        ShowWindow(g_form.hError, SW_SHOW);
        return;
    }
    if (!looks_like_date(c.tanggal)) {
        SetWindowTextA(g_form.hError, "Format tanggal harus YYYY-MM-DD, contoh: 2026-09-04");
        ShowWindow(g_form.hError, SW_SHOW);
        return;
    }

    if (g_form.editing) {
        store_update(g_form.store, g_form.editing->id, &c);
    } else {
        store_add(g_form.store, &c);
    }
    g_form.saved = TRUE;
    DestroyWindow(hwnd);
}

static LRESULT CALLBACK FormWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        int x = 30, y = 24, w = 380;
        CreateLabeledEdit(hwnd, "Wilayah Customer *", x, y, w, &g_form.hWilCust, IDC_FORM_WILCUST, 0);
        y += 62;
        CreateLabeledEdit(hwnd, "Wilayah Cabang *", x, y, w, &g_form.hWilCabang, IDC_FORM_WILCABANG, 0);
        y += 62;
        CreateLabeledEdit(hwnd, "Sales / Kepala Operasional *", x, y, w, &g_form.hSales, IDC_FORM_SALES, 0);
        y += 62;
        CreateLabeledEdit(hwnd, "Tanggal (YYYY-MM-DD) *", x, y, w, &g_form.hTanggal, IDC_FORM_TANGGAL, 0);
        y += 62;
        CreateLabeledEdit(hwnd, "Nomor Customer (HP) *", x, y, w, &g_form.hNomor, IDC_FORM_NOMOR, 0);
        y += 62;
        CreateWindowExA(0, "STATIC", "Catatan (opsional)", WS_CHILD | WS_VISIBLE, x, y, w, 20, hwnd, NULL, NULL, NULL);
        y += 22;
        g_form.hCatatan = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_MULTILINE,
            x, y, w, 60, hwnd, (HMENU)(INT_PTR)IDC_FORM_CATATAN, NULL, NULL);
        SendMessage(g_form.hCatatan, WM_SETFONT, (WPARAM)theme_font(FONT_BODY), TRUE);
        y += 74;

        g_form.hError = CreateWindowExA(0, "STATIC", "", WS_CHILD, x, y, w, 34, hwnd, (HMENU)(INT_PTR)0, NULL, NULL);
        SendMessage(g_form.hError, WM_SETFONT, (WPARAM)theme_font(FONT_SMALL), TRUE);
        y += 40;

        g_form.hCancel = CreateWindowExA(0, "BUTTON", "Batal", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x, y, 180, 40, hwnd, (HMENU)(INT_PTR)IDC_FORM_CANCEL, NULL, NULL);
        g_form.hSave = CreateWindowExA(0, "BUTTON", "Simpan", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x + 200, y, 200, 40, hwnd, (HMENU)(INT_PTR)IDC_FORM_SAVE, NULL, NULL);

        HWND child = GetWindow(hwnd, GW_CHILD);
        while (child) {
            char cls[32]; GetClassNameA(child, cls, sizeof(cls));
            if (strcmp(cls, "Static") == 0 && child != g_form.hError)
                SendMessage(child, WM_SETFONT, (WPARAM)theme_font(FONT_BODY_BOLD), TRUE);
            child = GetWindow(child, GW_HWNDNEXT);
        }

        if (g_form.editing) {
            Customer *e = g_form.editing;
            SetWindowTextA(g_form.hWilCust, e->wilayah_customer);
            SetWindowTextA(g_form.hWilCabang, e->wilayah_cabang);
            SetWindowTextA(g_form.hSales, e->sales);
            SetWindowTextA(g_form.hTanggal, e->tanggal);
            SetWindowTextA(g_form.hNomor, e->customer_number);
            SetWindowTextA(g_form.hCatatan, e->catatan);
            SetWindowTextA(hwnd, "Ubah Data Customer");
        } else {
            SetWindowTextA(hwnd, "Tambah Customer Baru");
        }
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme_brush(CLR_CARD));
        RECT header = { 0, 0, rc.right, 4 };
        FillRect(hdc, &header, theme_brush(CLR_ACCENT));
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp; HWND ctl = (HWND)lp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, ctl == g_form.hError ? CLR_DANGER : CLR_TEXT_DARK);
        return (LRESULT)theme_brush(CLR_CARD);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(0xFA,0xFB,0xFD));
        SetTextColor(hdc, CLR_TEXT_DARK);
        return (LRESULT)theme_brush(RGB(0xFA,0xFB,0xFD));
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        int state = (dis->itemState & ODS_SELECTED) ? 2 : 0;
        if (dis->CtlID == IDC_FORM_SAVE) {
            if (state == 0 && g_form.saveState == 1) state = 1;
            theme_draw_flat_button(dis->hDC, dis->rcItem, "Simpan", CLR_ACCENT, RGB(255,255,255), state, 8);
            return TRUE;
        } else if (dis->CtlID == IDC_FORM_CANCEL) {
            if (state == 0 && g_form.cancelState == 1) state = 1;
            theme_draw_flat_button(dis->hDC, dis->rcItem, "Batal", RGB(0xEC,0xEE,0xF3), CLR_TEXT_DARK, state, 8);
            return TRUE;
        }
        return FALSE;
    }
    case WM_MOUSEMOVE: {
        RECT rcS, rcC; GetWindowRect(g_form.hSave, &rcS); GetWindowRect(g_form.hCancel, &rcC);
        POINT pt; GetCursorPos(&pt);
        int ns = PtInRect(&rcS, pt) ? 1 : 0;
        int nc = PtInRect(&rcC, pt) ? 1 : 0;
        if (ns != g_form.saveState) { g_form.saveState = ns; InvalidateRect(g_form.hSave, NULL, FALSE); }
        if (nc != g_form.cancelState) { g_form.cancelState = nc; InvalidateRect(g_form.hCancel, NULL, FALSE); }
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            if (LOWORD(wp) == IDC_FORM_SAVE) { FormDoSave(hwnd); return 0; }
            if (LOWORD(wp) == IDC_FORM_CANCEL) { DestroyWindow(hwnd); return 0; }
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        EnableWindow(g_form.hOwner, TRUE);
        SetForegroundWindow(g_form.hOwner);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

BOOL ShowCustomerFormDialog(HINSTANCE hInst, HWND owner, CustomerStore *store, Customer *existing) {
    ZeroMemory(&g_form, sizeof(g_form));
    g_form.store = store;
    g_form.editing = existing;
    g_form.hOwner = owner;

    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSA wc = {0};
        wc.lpfnWndProc = FormWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = CLASS_FORM;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_APPICON));
        RegisterClassA(&wc);
        registered = TRUE;
    }

    int w = 460, h = 560;
    RECT orc; GetWindowRect(owner, &orc);
    int x = orc.left + ((orc.right - orc.left) - w) / 2;
    int y = orc.top + ((orc.bottom - orc.top) - h) / 2;

    EnableWindow(owner, FALSE);
    HWND hwnd = CreateWindowExA(WS_EX_DLGMODALFRAME, CLASS_FORM, "Form Customer",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, w, h, owner, NULL, hInst, NULL);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetFocus(g_form.hWilCust);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsWindow(hwnd)) break;
        if (!IsDialogMessageA(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!IsWindow(hwnd)) break;
    }
    return g_form.saved;
}

/* ============================ Detail view ============================ */

static const char *CLASS_DETAIL = "CSInputDetailWnd";

typedef struct {
    HWND hOwner;
    HWND hClose;
    Customer c;
    int closeState;
} DetailCtx;

static DetailCtx g_detail;

static void DrawDetailRow(HDC hdc, int x, int y, int w, const char *label, const char *value) {
    RECT rl = { x, y, x + 170, y + 22 };
    SetTextColor(hdc, CLR_TEXT_MUTED);
    HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_SMALL));
    DrawTextA(hdc, label, -1, &rl, DT_LEFT);
    SelectObject(hdc, theme_font(FONT_BODY_BOLD));
    RECT rv = { x, y + 20, x + w, y + 44 };
    SetTextColor(hdc, CLR_TEXT_DARK);
    DrawTextA(hdc, value, -1, &rv, DT_LEFT | DT_WORDBREAK);
    SelectObject(hdc, old);
}

static LRESULT CALLBACK DetailWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_detail.hClose = CreateWindowExA(0, "BUTTON", "Tutup", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            30, 400, 340, 40, hwnd, (HMENU)(INT_PTR)IDC_DETAIL_CLOSE, NULL, NULL);
        return 0;
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme_brush(CLR_CARD));
        RECT header = { 0, 0, rc.right, 4 };
        FillRect(hdc, &header, theme_brush(CLR_ACCENT));

        char buf[64];
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_DARK);
        HFONT old = (HFONT)SelectObject(hdc, theme_font(FONT_SUBTITLE));
        RECT tr = { 30, 20, rc.right - 20, 50 };
        DrawTextA(hdc, "Detail Customer", -1, &tr, DT_LEFT);
        SelectObject(hdc, old);

        int y = 70, x = 30, w = 340;
        DrawDetailRow(hdc, x, y, w, "WILAYAH CUSTOMER", g_detail.c.wilayah_customer); y += 56;
        DrawDetailRow(hdc, x, y, w, "WILAYAH CABANG", g_detail.c.wilayah_cabang); y += 56;
        DrawDetailRow(hdc, x, y, w, "SALES / KEPALA OPERASIONAL", g_detail.c.sales); y += 56;
        DrawDetailRow(hdc, x, y, w, "TANGGAL", g_detail.c.tanggal); y += 56;
        DrawDetailRow(hdc, x, y, w, "NOMOR CUSTOMER", g_detail.c.customer_number); y += 56;
        DrawDetailRow(hdc, x, y, w, "CATATAN", strlen(g_detail.c.catatan) ? g_detail.c.catatan : "-"); y += 56;

        sprintf(buf, "ID Internal: #%d", g_detail.c.id);
        SetTextColor(hdc, CLR_TEXT_MUTED);
        HFONT old2 = (HFONT)SelectObject(hdc, theme_font(FONT_SMALL));
        RECT idr = { x, y, x + w, y + 20 };
        DrawTextA(hdc, buf, -1, &idr, DT_LEFT);
        SelectObject(hdc, old2);
        return TRUE;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        int state = (dis->itemState & ODS_SELECTED) ? 2 : g_detail.closeState;
        theme_draw_flat_button(dis->hDC, dis->rcItem, "Tutup", CLR_ACCENT, RGB(255,255,255), state, 8);
        return TRUE;
    }
    case WM_MOUSEMOVE: {
        RECT rc; GetWindowRect(g_detail.hClose, &rc);
        POINT pt; GetCursorPos(&pt);
        int ns = PtInRect(&rc, pt) ? 1 : 0;
        if (ns != g_detail.closeState) { g_detail.closeState = ns; InvalidateRect(g_detail.hClose, NULL, FALSE); }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_DETAIL_CLOSE && HIWORD(wp) == BN_CLICKED) { DestroyWindow(hwnd); return 0; }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        EnableWindow(g_detail.hOwner, TRUE);
        SetForegroundWindow(g_detail.hOwner);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void ShowCustomerDetailDialog(HINSTANCE hInst, HWND owner, Customer *c) {
    ZeroMemory(&g_detail, sizeof(g_detail));
    g_detail.hOwner = owner;
    g_detail.c = *c;

    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSA wc = {0};
        wc.lpfnWndProc = DetailWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = CLASS_DETAIL;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_APPICON));
        RegisterClassA(&wc);
        registered = TRUE;
    }

    int w = 420, h = 480;
    RECT orc; GetWindowRect(owner, &orc);
    int x = orc.left + ((orc.right - orc.left) - w) / 2;
    int y = orc.top + ((orc.bottom - orc.top) - h) / 2;

    EnableWindow(owner, FALSE);
    HWND hwnd = CreateWindowExA(WS_EX_DLGMODALFRAME, CLASS_DETAIL, "Detail Customer",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, w, h, owner, NULL, hInst, NULL);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsWindow(hwnd)) break;
        if (!IsDialogMessageA(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!IsWindow(hwnd)) break;
    }
}

/* ============================ Confirm dialog ============================ */

BOOL ShowConfirmDialog(HWND owner, const char *title, const char *message) {
    int r = MessageBoxA(owner, message, title, MB_YESNO | MB_ICONQUESTION);
    return r == IDYES;
}
