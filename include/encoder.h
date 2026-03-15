#pragma once

#include <cstdint>
#include <vector>
#include <functional>

namespace webify {

struct FrameData;

struct EncodedPacket {
    std::vector<uint8_t> data;
    bool is_keyframe = false;
    uint64_t timestamp_ms = 0;
};

// Software H.264 encoder using Media Foundation (Windows).
// For the PoC this wraps the built-in Windows Media Foundation H.264 encoder
// which is available on all Windows 10/Server 2016+ without additional installs.
class Encoder {
public:
    Encoder();
    ~Encoder();

    // Initialize encoder for given resolution and framerate.
    bool init(int width, int height, int fps = 15, int bitrate_kbps = 2000);
    void shutdown();

    // Encode a BGRA frame. Returns encoded H.264 NAL units.
    bool encode(const FrameData& frame, EncodedPacket& packet);

    // Request a keyframe on the next encode call.
    void request_keyframe();

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool keyframe_requested_ = false;
};

} // namespace webify
