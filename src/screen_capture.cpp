#include "screen_capture.h"

#include <cstdio>
#include <cstring>

namespace webify {

ScreenCapture::ScreenCapture() = default;

ScreenCapture::~ScreenCapture() {
    stop();
}

bool ScreenCapture::start(uint32_t process_id, int width, int height, int fps) {
    if (running_) stop();

    width_ = width;
    height_ = height;

#ifdef _WIN32
    target_pid_ = process_id;

    // Stay on the interactive desktop — do NOT call SetThreadDesktop.
    // This ensures our memory DC has a real display surface behind it.

    // Create memory DC from the screen DC (interactive desktop has a real display)
    HDC screen_dc = GetDC(nullptr);
    mem_dc_ = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    bitmap_ = CreateDIBSection(mem_dc_, &bmi, DIB_RGB_COLORS,
                                &dib_pixels_, nullptr, 0);
    ReleaseDC(nullptr, screen_dc);

    if (!bitmap_) {
        fprintf(stderr, "ScreenCapture: CreateDIBSection failed: %lu\n", GetLastError());
        DeleteDC(mem_dc_);
        mem_dc_ = nullptr;
        return false;
    }
    old_bitmap_ = (HBITMAP)SelectObject(mem_dc_, bitmap_);

    running_ = true;
    fprintf(stdout, "ScreenCapture: started for PID %lu (%dx%d @ %d fps)\n",
            (unsigned long)target_pid_, width_, height_, fps);
#else
    running_ = true;
    fprintf(stdout, "ScreenCapture: started (stub, non-Windows)\n");
#endif

    return true;
}

void ScreenCapture::stop() {
    if (!running_) return;

#ifdef _WIN32
    if (mem_dc_) {
        SelectObject(mem_dc_, old_bitmap_);
        DeleteDC(mem_dc_);
        mem_dc_ = nullptr;
    }
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    dib_pixels_ = nullptr;
#endif

    running_ = false;
    fprintf(stdout, "ScreenCapture: stopped\n");
}

bool ScreenCapture::capture_frame(FrameData& frame) {
    if (!running_) return false;

#ifdef _WIN32
    // Clear to desktop background color
    HBRUSH bg_brush = CreateSolidBrush(RGB(58, 110, 165));
    RECT bg_rect = {0, 0, width_, height_};
    FillRect(mem_dc_, &bg_rect, bg_brush);
    DeleteObject(bg_brush);

    // Find all top-level windows belonging to our target process
    struct EnumData {
        HDC dc;
        int width;
        int height;
        int count;
        int total;
        DWORD pid;
        bool log;
    };

    static bool first_frame = true;
    EnumData data = { mem_dc_, width_, height_, 0, 0, target_pid_, first_frame };

    // Use EnumWindows (enumerates the calling thread's desktop = interactive desktop)
    // to find windows owned by our target PID
    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        auto* d = (EnumData*)param;

        // Check if this window belongs to our target process
        DWORD wnd_pid = 0;
        GetWindowThreadProcessId(hwnd, &wnd_pid);
        if (wnd_pid != d->pid) return TRUE;

        d->total++;

        if (d->log) {
            char title[256] = {};
            GetWindowTextA(hwnd, title, sizeof(title));
            char cls[256] = {};
            GetClassNameA(hwnd, cls, sizeof(cls));
            BOOL vis = IsWindowVisible(hwnd);
            RECT r;
            GetWindowRect(hwnd, &r);
            fprintf(stderr, "DEBUG: PID match HWND=%p class='%s' title='%s' visible=%d rect=(%ld,%ld,%ld,%ld)\n",
                    (void*)hwnd, cls, title, vis, r.left, r.top, r.right, r.bottom);
        }

        // Skip IsWindowVisible check — in session 0 (SSM/service) windows
        // are never "visible" but PrintWindow can still capture them
        RECT wr;
        GetWindowRect(hwnd, &wr);
        int w = wr.right - wr.left;
        int h = wr.bottom - wr.top;
        if (w <= 0 || h <= 0) return TRUE;

        // Create a temp DC/bitmap for this window
        HDC win_dc = CreateCompatibleDC(d->dc);
        HBITMAP win_bmp = CreateCompatibleBitmap(d->dc, w, h);
        HBITMAP old = (HBITMAP)SelectObject(win_dc, win_bmp);

        // Try PW_RENDERFULLCONTENT first, fall back to basic PrintWindow
        // PW_RENDERFULLCONTENT can return black on servers without full DWM
        BOOL pw_ok = PrintWindow(hwnd, win_dc, 0);
        if (!pw_ok) pw_ok = PrintWindow(hwnd, win_dc, PW_RENDERFULLCONTENT);
        if (pw_ok) {
            // Position the window in our virtual desktop at (0,0) for now
            // (since the app may be positioned anywhere on the real desktop)
            int dx = 0, dy = 0;
            int cw = (w < d->width) ? w : d->width;
            int ch = (h < d->height) ? h : d->height;
            BitBlt(d->dc, dx, dy, cw, ch, win_dc, 0, 0, SRCCOPY);
            d->count++;
        } else if (d->log) {
            fprintf(stderr, "DEBUG: PrintWindow failed for HWND=%p err=%lu\n",
                    (void*)hwnd, GetLastError());
        }

        SelectObject(win_dc, old);
        DeleteObject(win_bmp);
        DeleteDC(win_dc);

        return TRUE;
    }, (LPARAM)&data);

    static int debug_counter = 0;
    if (debug_counter < 30) {  // Log first 30 frames (~3 seconds)
        fprintf(stderr, "DEBUG: EnumWindows total_for_pid=%d, captured=%d (pid=%lu)\n",
                data.total, data.count, (unsigned long)target_pid_);
        debug_counter++;
    }
    if (first_frame) first_frame = false;

    if (data.count == 0) {
        SetBkMode(mem_dc_, TRANSPARENT);
        SetTextColor(mem_dc_, RGB(255, 255, 255));
        HFONT font = CreateFontA(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        HFONT old_font = (HFONT)SelectObject(mem_dc_, font);
        const char* msg = "Waiting for application...";
        RECT tr = {0, 0, width_, height_};
        DrawTextA(mem_dc_, msg, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mem_dc_, old_font);
        DeleteObject(font);
    }

    GdiFlush();

    frame.width = width_;
    frame.height = height_;
    frame.stride = width_ * 4;
    frame.pixels.resize(frame.stride * height_);
    memcpy(frame.pixels.data(), dib_pixels_, frame.stride * height_);

    return true;
#else
    // Stub: generate a test pattern
    frame.width = width_;
    frame.height = height_;
    frame.stride = width_ * 4;
    frame.pixels.resize(frame.stride * height_, 0);
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            int offset = y * frame.stride + x * 4;
            frame.pixels[offset + 0] = static_cast<uint8_t>(x % 256);
            frame.pixels[offset + 1] = static_cast<uint8_t>(y % 256);
            frame.pixels[offset + 2] = 128;
            frame.pixels[offset + 3] = 255;
        }
    }
    return true;
#endif
}

} // namespace webify
