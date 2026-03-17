#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <cstdio>
#include <ctime>

static HWND g_edit = nullptr;
static HWND g_button = nullptr;
static HFONT g_titleFont = nullptr;
static int g_clickCount = 0;

#define ID_EDIT 101
#define ID_BUTTON 102

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            // Create a bold font for the title
            g_titleFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

            HFONT btnFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

            // Button
            g_button = CreateWindowW(L"BUTTON", L"Click Me!",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 55, 150, 40, hwnd, (HMENU)ID_BUTTON, hInst, nullptr);
            SendMessage(g_button, WM_SETFONT, (WPARAM)btnFont, TRUE);

            // Multi-line edit box
            g_edit = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_READONLY,
                20, 105, 360, 260, hwnd, (HMENU)ID_EDIT, hInst, nullptr);

            HFONT editFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");
            SendMessage(g_edit, WM_SETFONT, (WPARAM)editFont, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BUTTON && HIWORD(wParam) == BN_CLICKED) {
                g_clickCount++;
                // Get current time
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);
                wchar_t line[256];
                swprintf(line, 256, L"[%02d:%02d:%02d] Click #%d - Session %lu\r\n",
                    t->tm_hour, t->tm_min, t->tm_sec,
                    g_clickCount, GetCurrentProcessId());
                // Append to edit control
                int len = GetWindowTextLengthW(g_edit);
                SendMessageW(g_edit, EM_SETSEL, len, len);
                SendMessageW(g_edit, EM_REPLACESEL, FALSE, (LPARAM)line);
            }
            return 0;
        }
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (g_button) MoveWindow(g_button, 20, 55, 150, 40, TRUE);
            if (g_edit) MoveWindow(g_edit, 20, 105, rc.right - 40, rc.bottom - 125, TRUE);
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
            RECT titleRect = {20, 12, rc.right - 20, 50};
            DrawTextW(hdc, L"Hello from Webify", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, oldFont);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcCtl = (HDC)wParam;
            SetBkColor(hdcCtl, RGB(255, 255, 255));
            SetTextColor(hdcCtl, RGB(0, 0, 0));
            static HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            return (LRESULT)whiteBrush;
        }
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
