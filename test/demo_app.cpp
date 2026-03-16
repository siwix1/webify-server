#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

static HWND g_edit = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Title label
            CreateWindowW(L"STATIC", L"Webify Demo App",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 10, 360, 30, hwnd, nullptr,
                ((LPCREATESTRUCT)lParam)->hInstance, nullptr);

            // Multi-line edit box
            g_edit = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                20, 50, 360, 300, hwnd, (HMENU)101,
                ((LPCREATESTRUCT)lParam)->hInstance, nullptr);

            // Set focus to edit box
            SetFocus(g_edit);
            return 0;
        }
        case WM_SIZE: {
            if (g_edit) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                MoveWindow(g_edit, 20, 50, rc.right - 40, rc.bottom - 70, TRUE);
            }
            return 0;
        }
        case WM_SETFOCUS:
            if (g_edit) SetFocus(g_edit);
            return 0;
        case WM_PRINT:
        case WM_PRINTCLIENT:
            // Support PrintWindow capture
            DefWindowProc(hwnd, WM_PAINT, wParam, lParam);
            return 0;
        case WM_DESTROY:
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
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"WebifyDemo", L"Webify Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 400,
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
