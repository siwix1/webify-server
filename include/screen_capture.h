#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <cstdint>
#include <vector>
#include <string>

namespace webify {

struct FrameData {
    std::vector<uint8_t> pixels;  // BGRA format
    int width = 0;
    int height = 0;
    int stride = 0;
};

// Captures window contents by process ID using PrintWindow.
// Stays on the interactive desktop so GDI has a real display surface.
class ScreenCapture {
public:
    ScreenCapture();
    ~ScreenCapture();

    // Start capturing. process_id is the target app's PID.
    bool start(uint32_t process_id, int width, int height, int fps = 15);
    void stop();

    // Grab a single frame (synchronous).
    bool capture_frame(FrameData& frame);

    bool is_running() const { return running_; }

private:
    int width_ = 0;
    int height_ = 0;
    bool running_ = false;
    uint32_t target_pid_ = 0;

#ifdef _WIN32
    HDC mem_dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP old_bitmap_ = nullptr;
    void* dib_pixels_ = nullptr;
#endif
};

} // namespace webify
