#include "vdd_capture.h"
#include "screen_capture.h"  // for FrameData

#include <cstdio>
#include <cstring>

namespace webify {

VddCapture::VddCapture() = default;

VddCapture::~VddCapture() {
    stop();
}

bool VddCapture::start(int monitor_index, int width, int height) {
    if (running_) stop();

    monitor_index_ = monitor_index;
    width_ = width;
    height_ = height;

#ifdef _WIN32
    // Open the shared memory created by the driver
    char name[128];
    snprintf(name, sizeof(name), "Global\\WebifyVDD_Monitor_%d", monitor_index);

    h_mapped_file_ = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
    if (!h_mapped_file_) {
        fprintf(stderr, "VddCapture: OpenFileMapping('%s') failed: %lu\n", name, GetLastError());
        return false;
    }

    p_shared_mem_ = MapViewOfFile(h_mapped_file_, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
    if (!p_shared_mem_) {
        fprintf(stderr, "VddCapture: MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(h_mapped_file_);
        h_mapped_file_ = nullptr;
        return false;
    }

    last_frame_number_ = 0;
    running_ = true;
    fprintf(stdout, "VddCapture: attached to monitor %d (%dx%d) via '%s'\n",
            monitor_index, width, height, name);
#endif

    return true;
}

void VddCapture::stop() {
    if (!running_) return;

#ifdef _WIN32
    if (p_shared_mem_) {
        UnmapViewOfFile(p_shared_mem_);
        p_shared_mem_ = nullptr;
    }
    if (h_mapped_file_) {
        CloseHandle(h_mapped_file_);
        h_mapped_file_ = nullptr;
    }
#endif

    running_ = false;
    fprintf(stdout, "VddCapture: detached from monitor %d\n", monitor_index_);
}

bool VddCapture::capture_frame(FrameData& frame) {
    if (!running_) return false;

#ifdef _WIN32
    auto* header = static_cast<SharedFrameHeader*>(p_shared_mem_);

    // Check magic
    if (header->magic != SHARED_MAGIC) {
        return false;  // Driver hasn't written yet
    }

    // Check if there's a new frame
    if (header->frame_number == last_frame_number_ && header->ready == 0) {
        return false;  // No new frame
    }

    uint32_t w = header->width;
    uint32_t h = header->height;
    uint32_t stride = header->stride;

    if (w == 0 || h == 0 || w > MAX_WIDTH || h > MAX_HEIGHT) {
        return false;
    }

    // Read pixel data from after the header
    const uint8_t* pixels = reinterpret_cast<const uint8_t*>(header) + sizeof(SharedFrameHeader);

    frame.width = static_cast<int>(w);
    frame.height = static_cast<int>(h);
    frame.stride = static_cast<int>(w * 4);

    // Copy pixels, handling stride difference (driver stride may differ from w*4)
    frame.pixels.resize(frame.stride * frame.height);
    if (stride == w * 4) {
        memcpy(frame.pixels.data(), pixels, frame.stride * frame.height);
    } else {
        // Copy row by row
        for (uint32_t y = 0; y < h; y++) {
            memcpy(frame.pixels.data() + y * frame.stride,
                   pixels + y * stride,
                   w * 4);
        }
    }

    last_frame_number_ = header->frame_number;

    return true;
#else
    return false;
#endif
}

} // namespace webify
