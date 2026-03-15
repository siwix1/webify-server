#include "encoder.h"
#include "screen_capture.h"

#include <cstdio>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <mferror.h>
#include <codecapi.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#endif

namespace webify {

#ifdef _WIN32
struct Encoder::Impl {
    IMFTransform* transform = nullptr;
    IMFMediaType* input_type = nullptr;
    IMFMediaType* output_type = nullptr;
    DWORD input_stream_id = 0;
    DWORD output_stream_id = 0;
    int width = 0;
    int height = 0;
    int fps = 0;
    uint64_t frame_count = 0;
    bool mf_initialized = false;
};
#else
struct Encoder::Impl {
    int width = 0;
    int height = 0;
    int fps = 0;
    uint64_t frame_count = 0;
};
#endif

Encoder::Encoder() = default;

Encoder::~Encoder() {
    shutdown();
}

bool Encoder::init(int width, int height, int fps, int bitrate_kbps) {
    if (impl_) shutdown();

    impl_ = new Impl();
    impl_->width = width;
    impl_->height = height;
    impl_->fps = fps;

#ifdef _WIN32
    // Initialize Media Foundation
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        fprintf(stderr, "Encoder: MFStartup failed: 0x%lx\n", hr);
        delete impl_;
        impl_ = nullptr;
        return false;
    }
    impl_->mf_initialized = true;

    // Find the H.264 encoder MFT
    MFT_REGISTER_TYPE_INFO output_info = { MFMediaType_Video, MFVideoFormat_H264 };

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_ENCODER,
        MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
        nullptr,
        &output_info,
        &activates,
        &count
    );

    if (FAILED(hr) || count == 0) {
        fprintf(stderr, "Encoder: No H.264 encoder found: 0x%lx (count=%u)\n", hr, count);
        shutdown();
        return false;
    }

    // Activate the first encoder
    hr = activates[0]->ActivateObject(IID_IMFTransform, (void**)&impl_->transform);

    // Release activates
    for (UINT32 i = 0; i < count; i++) {
        activates[i]->Release();
    }
    CoTaskMemFree(activates);

    if (FAILED(hr)) {
        fprintf(stderr, "Encoder: ActivateObject failed: 0x%lx\n", hr);
        shutdown();
        return false;
    }

    // Set output type (H.264)
    hr = MFCreateMediaType(&impl_->output_type);
    if (SUCCEEDED(hr)) {
        impl_->output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        impl_->output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        impl_->output_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate_kbps * 1000);
        MFSetAttributeSize(impl_->output_type, MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(impl_->output_type, MF_MT_FRAME_RATE, fps, 1);
        impl_->output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeRatio(impl_->output_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

        hr = impl_->transform->SetOutputType(0, impl_->output_type, 0);
    }

    if (FAILED(hr)) {
        fprintf(stderr, "Encoder: SetOutputType failed: 0x%lx\n", hr);
        shutdown();
        return false;
    }

    // Set input type (NV12 — we'll convert BGRA to NV12 before encoding)
    hr = MFCreateMediaType(&impl_->input_type);
    if (SUCCEEDED(hr)) {
        impl_->input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        impl_->input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        MFSetAttributeSize(impl_->input_type, MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(impl_->input_type, MF_MT_FRAME_RATE, fps, 1);
        impl_->input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeRatio(impl_->input_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

        hr = impl_->transform->SetInputType(0, impl_->input_type, 0);
    }

    if (FAILED(hr)) {
        fprintf(stderr, "Encoder: SetInputType failed: 0x%lx\n", hr);
        shutdown();
        return false;
    }

    fprintf(stdout, "Encoder: initialized %dx%d @ %d fps, %d kbps\n",
            width, height, fps, bitrate_kbps);
#else
    fprintf(stdout, "Encoder: initialized (stub, non-Windows) %dx%d @ %d fps\n",
            width, height, fps);
#endif

    return true;
}

void Encoder::shutdown() {
    if (!impl_) return;

#ifdef _WIN32
    if (impl_->transform) { impl_->transform->Release(); impl_->transform = nullptr; }
    if (impl_->input_type) { impl_->input_type->Release(); impl_->input_type = nullptr; }
    if (impl_->output_type) { impl_->output_type->Release(); impl_->output_type = nullptr; }
    if (impl_->mf_initialized) { MFShutdown(); impl_->mf_initialized = false; }
#endif

    delete impl_;
    impl_ = nullptr;
}

// Simple BGRA -> NV12 conversion (Y plane + interleaved UV plane)
static void bgra_to_nv12(const uint8_t* bgra, uint8_t* nv12, int width, int height) {
    int y_size = width * height;
    uint8_t* y_plane = nv12;
    uint8_t* uv_plane = nv12 + y_size;

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int src = (row * width + col) * 4;
            uint8_t b = bgra[src + 0];
            uint8_t g = bgra[src + 1];
            uint8_t r = bgra[src + 2];

            // BT.601 RGB to Y
            uint8_t y = static_cast<uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            y_plane[row * width + col] = y;

            // Subsample UV (2x2 blocks)
            if ((row % 2 == 0) && (col % 2 == 0)) {
                uint8_t u = static_cast<uint8_t>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
                uint8_t v = static_cast<uint8_t>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
                int uv_idx = (row / 2) * width + col;
                uv_plane[uv_idx + 0] = u;
                uv_plane[uv_idx + 1] = v;
            }
        }
    }
}

bool Encoder::encode(const FrameData& frame, EncodedPacket& packet) {
    if (!impl_) return false;

#ifdef _WIN32
    // Convert BGRA to NV12
    int nv12_size = frame.width * frame.height * 3 / 2;
    std::vector<uint8_t> nv12(nv12_size);
    bgra_to_nv12(frame.pixels.data(), nv12.data(), frame.width, frame.height);

    // Create input sample
    IMFSample* input_sample = nullptr;
    IMFMediaBuffer* input_buffer = nullptr;
    HRESULT hr;

    hr = MFCreateMemoryBuffer(nv12_size, &input_buffer);
    if (FAILED(hr)) return false;

    BYTE* buf_ptr = nullptr;
    hr = input_buffer->Lock(&buf_ptr, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
        memcpy(buf_ptr, nv12.data(), nv12_size);
        input_buffer->Unlock();
        input_buffer->SetCurrentLength(nv12_size);
    }

    hr = MFCreateSample(&input_sample);
    if (SUCCEEDED(hr)) {
        input_sample->AddBuffer(input_buffer);

        // Set timestamp
        LONGLONG duration = 10000000LL / impl_->fps;  // 100ns units
        LONGLONG timestamp = impl_->frame_count * duration;
        input_sample->SetSampleTime(timestamp);
        input_sample->SetSampleDuration(duration);

        // Request keyframe if needed
        if (keyframe_requested_) {
            // This would require ICodecAPI — simplified for PoC
            keyframe_requested_ = false;
        }

        // Feed to encoder
        hr = impl_->transform->ProcessInput(0, input_sample, 0);
    }

    if (input_sample) input_sample->Release();
    if (input_buffer) input_buffer->Release();

    if (FAILED(hr)) {
        fprintf(stderr, "Encoder: ProcessInput failed: 0x%lx\n", hr);
        return false;
    }

    // Try to get output
    MFT_OUTPUT_DATA_BUFFER output_data = {};
    DWORD status = 0;

    // Check if we need to allocate our own output buffer
    MFT_OUTPUT_STREAM_INFO stream_info = {};
    hr = impl_->transform->GetOutputStreamInfo(0, &stream_info);

    if (!(stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
        IMFSample* output_sample = nullptr;
        IMFMediaBuffer* output_buffer = nullptr;

        MFCreateMemoryBuffer(stream_info.cbSize, &output_buffer);
        MFCreateSample(&output_sample);
        output_sample->AddBuffer(output_buffer);
        output_data.pSample = output_sample;

        if (output_buffer) output_buffer->Release();
    }

    hr = impl_->transform->ProcessOutput(0, 1, &output_data, &status);

    if (SUCCEEDED(hr) && output_data.pSample) {
        IMFMediaBuffer* out_buf = nullptr;
        hr = output_data.pSample->ConvertToContiguousBuffer(&out_buf);

        if (SUCCEEDED(hr)) {
            BYTE* data = nullptr;
            DWORD length = 0;
            hr = out_buf->Lock(&data, nullptr, &length);
            if (SUCCEEDED(hr)) {
                packet.data.assign(data, data + length);
                packet.timestamp_ms = (impl_->frame_count * 1000) / impl_->fps;
                packet.is_keyframe = (impl_->frame_count % (impl_->fps * 2) == 0);
                out_buf->Unlock();
            }
            out_buf->Release();
        }

        output_data.pSample->Release();
    } else {
        if (output_data.pSample) output_data.pSample->Release();
        // MF_E_TRANSFORM_NEED_MORE_INPUT is normal — encoder needs more frames
    }

    impl_->frame_count++;
    return true;
#else
    // Stub: generate fake encoded data
    packet.data.resize(100, 0);
    packet.timestamp_ms = (impl_->frame_count * 1000) / impl_->fps;
    packet.is_keyframe = (impl_->frame_count % 30 == 0);
    impl_->frame_count++;
    return true;
#endif
}

void Encoder::request_keyframe() {
    keyframe_requested_ = true;
}

} // namespace webify
