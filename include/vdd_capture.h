#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <cstdint>
#include <vector>
#include <string>
#include <atomic>

namespace webify {

struct FrameData;  // forward decl from screen_capture.h

// Captures frames from the IddCx virtual display driver via shared memory.
// Each monitor index maps to "Global\WebifyVDD_Monitor_N".
class VddCapture {
public:
    VddCapture();
    ~VddCapture();

    // Attach to a virtual monitor by index (0-3).
    bool start(int monitor_index, int width, int height);
    void stop();

    // Grab the latest frame from shared memory.
    bool capture_frame(FrameData& frame);

    bool is_running() const { return running_; }
    int monitor_index() const { return monitor_index_; }

private:
    int monitor_index_ = -1;
    int width_ = 0;
    int height_ = 0;
    bool running_ = false;

#ifdef _WIN32
    HANDLE h_mapped_file_ = nullptr;
    void* p_shared_mem_ = nullptr;
#endif

    // Match the driver's shared memory layout
    struct SharedFrameHeader {
        uint32_t magic;           // 'WBFY' = 0x59464257
        uint32_t width;
        uint32_t height;
        uint32_t stride;
        uint64_t frame_number;
        uint32_t ready;           // 1 = new frame available
        uint32_t reserved;
        // Pixel data follows immediately after header (BGRA32)
    };

    static constexpr uint32_t SHARED_MAGIC = 0x59464257;
    static constexpr size_t MAX_WIDTH = 1920;
    static constexpr size_t MAX_HEIGHT = 1080;
    static constexpr size_t SHARED_MEM_SIZE = sizeof(SharedFrameHeader) + (MAX_WIDTH * MAX_HEIGHT * 4);

    uint64_t last_frame_number_ = 0;
};

} // namespace webify
