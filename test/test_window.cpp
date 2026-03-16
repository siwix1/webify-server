// Minimal Win32 window app for testing desktop capture.
// Creates a visible window with colored background and text.

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

// Shared paint logic — used by both WM_PAINT and WM_PRINTCLIENT
void PaintContent(HWND hwnd, HDC hdc) {
    RECT r;
    GetClientRect(hwnd, &r);

    // Green background
    HBRUSH bg = CreateSolidBrush(RGB(0, 180, 80));
    FillRect(hdc, &r, bg);
    DeleteObject(bg);

    // White text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    HFONT font = CreateFontA(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Arial");
    HFONT old = (HFONT)SelectObject(hdc, font);
    DrawTextA(hdc, "Hello from webify!", -1, &r,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(font);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintContent(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_PRINTCLIENT: {
            // PrintWindow sends this — paint into the provided DC
            PaintContent(hwnd, (HDC)wParam);
            return 0;
        }
        case WM_PRINT: {
            // Also handle WM_PRINT explicitly
            PaintContent(hwnd, (HDC)wParam);
            // Let DefWindowProc handle non-client area if requested
            if (lParam & PRF_NONCLIENT)
                break;
            return 0;
        }
        case WM_ERASEBKGND:
            // We paint the full background in PaintContent, skip default erase
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "WebifyTestWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("WebifyTestWindow", "Webify Test",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               50, 50, 800, 600,
                               nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

#else
int main() { return 0; }
#endif
