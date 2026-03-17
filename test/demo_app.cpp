#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

static HWND g_edit = nullptr;
static HFONT g_titleFont = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            // Create a bold font for the title
            g_titleFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

            // Multi-line edit box
            g_edit = CreateWindowW(L"EDIT", L"Type here...",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                20, 60, 360, 300, hwnd, (HMENU)101, hInst, nullptr);

            HFONT editFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");
            SendMessage(g_edit, WM_SETFONT, (WPARAM)editFont, TRUE);
            SetFocus(g_edit);
            return 0;
        }
        case WM_SIZE: {
            if (g_edit) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                MoveWindow(g_edit, 20, 60, rc.right - 40, rc.bottom - 80, TRUE);
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // White background
            HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);

            // Green title text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 160, 0));
            HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
            RECT titleRect = {20, 15, rc.right - 20, 55};
            DrawTextW(hdc, L"Hello from Webify", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, oldFont);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;  // Handled in WM_PAINT
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcCtl = (HDC)wParam;
            SetBkColor(hdcCtl, RGB(255, 255, 255));
            SetTextColor(hdcCtl, RGB(0, 0, 0));
            static HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            return (LRESULT)whiteBrush;
        }
        case WM_SETFOCUS:
            if (g_edit) SetFocus(g_edit);
            return 0;
        case WM_DESTROY:
            if (g_titleFont) DeleteObject(g_titleFont);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WebifyDemo";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"WebifyDemo", L"Webify Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 450,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
