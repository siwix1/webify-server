#include "screen_capture.h"

#include <cstdio>
#include <cstring>

namespace webify {

ScreenCapture::ScreenCapture() = default;

ScreenCapture::~ScreenCapture() {
    stop();
}

bool ScreenCapture::start(const std::string& desktop_name, int width, int height, int fps) {
    if (running_) stop();

    width_ = width;
    height_ = height;

#ifdef _WIN32
    // Open the desktop for screen reading
    desktop_ = OpenDesktopA(desktop_name.c_str(), 0, FALSE, GENERIC_READ);
    if (!desktop_) {
        fprintf(stderr, "ScreenCapture: OpenDesktop failed: %lu\n", GetLastError());
        return false;
    }

    // We need to set this thread's desktop to the target desktop
    // in order to get a valid device context for it.
    // Save current desktop so we can restore if needed.
    HDESK old_desktop = GetThreadDesktop(GetCurrentThreadId());

    if (!SetThreadDesktop(desktop_)) {
        fprintf(stderr, "ScreenCapture: SetThreadDesktop failed: %lu\n", GetLastError());
        CloseDesktop(desktop_);
        desktop_ = nullptr;
        return false;
    }

    // Get the desktop's device context
    desktop_dc_ = GetDC(nullptr);  // nullptr = entire desktop
    if (!desktop_dc_) {
        fprintf(stderr, "ScreenCapture: GetDC failed\n");
        SetThreadDesktop(old_desktop);
        CloseDesktop(desktop_);
        desktop_ = nullptr;
        return false;
    }

    // Create a compatible memory DC and bitmap for capturing
    mem_dc_ = CreateCompatibleDC(desktop_dc_);
    bitmap_ = CreateCompatibleBitmap(desktop_dc_, width_, height_);
    old_bitmap_ = (HBITMAP)SelectObject(mem_dc_, bitmap_);

    running_ = true;
    fprintf(stdout, "ScreenCapture: started on desktop '%s' (%dx%d @ %d fps)\n",
            desktop_name.c_str(), width_, height_, fps);
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
    if (desktop_dc_) {
        ReleaseDC(nullptr, desktop_dc_);
        desktop_dc_ = nullptr;
    }
    if (desktop_) {
        CloseDesktop(desktop_);
        desktop_ = nullptr;
    }
#endif

    running_ = false;
    fprintf(stdout, "ScreenCapture: stopped\n");
}

bool ScreenCapture::capture_frame(FrameData& frame) {
    if (!running_) return false;

#ifdef _WIN32
    // BitBlt from desktop DC to our memory DC
    if (!BitBlt(mem_dc_, 0, 0, width_, height_, desktop_dc_, 0, 0, SRCCOPY)) {
        fprintf(stderr, "ScreenCapture: BitBlt failed: %lu\n", GetLastError());
        return false;
    }

    // Read pixels from the bitmap
    BITMAPINFOHEADER bmi = {};
    bmi.biSize = sizeof(bmi);
    bmi.biWidth = width_;
    bmi.biHeight = -height_;  // negative = top-down
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;      // BGRA
    bmi.biCompression = BI_RGB;

    frame.width = width_;
    frame.height = height_;
    frame.stride = width_ * 4;
    frame.pixels.resize(frame.stride * height_);

    int lines = GetDIBits(mem_dc_, bitmap_, 0, height_,
                          frame.pixels.data(),
                          reinterpret_cast<BITMAPINFO*>(&bmi),
                          DIB_RGB_COLORS);

    if (lines == 0) {
        fprintf(stderr, "ScreenCapture: GetDIBits failed: %lu\n", GetLastError());
        return false;
    }

    return true;
#else
    // Stub: generate a test pattern
    frame.width = width_;
    frame.height = height_;
    frame.stride = width_ * 4;
    frame.pixels.resize(frame.stride * height_, 0);

    // Simple gradient for testing
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            int offset = y * frame.stride + x * 4;
            frame.pixels[offset + 0] = static_cast<uint8_t>(x % 256);  // B
            frame.pixels[offset + 1] = static_cast<uint8_t>(y % 256);  // G
            frame.pixels[offset + 2] = 128;                             // R
            frame.pixels[offset + 3] = 255;                             // A
        }
    }
    return true;
#endif
}

} // namespace webify
