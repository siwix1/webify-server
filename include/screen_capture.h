#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdint>
#include <vector>
#include <functional>
#include <string>

namespace webify {

struct FrameData {
    std::vector<uint8_t> pixels;  // BGRA format
    int width = 0;
    int height = 0;
    int stride = 0;
};

// Captures screen contents from a specific desktop using GDI.
// GDI capture is simpler than Desktop Duplication API and works
// on non-active desktops — perfect for the PoC.
class ScreenCapture {
public:
    ScreenCapture();
    ~ScreenCapture();

    // Start capturing a desktop by name. Captures at the given FPS.
    bool start(const std::string& desktop_name, int width, int height, int fps = 15);
    void stop();

    // Grab a single frame (synchronous). Returns false if capture failed.
    bool capture_frame(FrameData& frame);

    bool is_running() const { return running_; }

private:
#ifdef _WIN32
    HDESK desktop_ = nullptr;
    HDC desktop_dc_ = nullptr;
    HDC mem_dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP old_bitmap_ = nullptr;
#endif
    int width_ = 0;
    int height_ = 0;
    bool running_ = false;
};

} // namespace webify
