#include "jpeg_encoder.h"

#include <cstdio>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace webify {

std::vector<uint8_t> encode_jpeg(const uint8_t* bgra, int width, int height, int quality) {
    std::vector<uint8_t> result;

    // Encode as BMP instead of JPEG to avoid WIC issues in session 0.
    // BMP is larger but guaranteed correct.
    {
        int stride = width * 4;
        int pixel_size = stride * height;
        int file_size = 54 + pixel_size;  // BMP header + pixels

        result.resize(file_size);
        uint8_t* p = result.data();

        // BMP file header (14 bytes)
        p[0] = 'B'; p[1] = 'M';
        *(uint32_t*)(p + 2) = file_size;
        *(uint32_t*)(p + 6) = 0;
        *(uint32_t*)(p + 10) = 54;  // pixel data offset

        // DIB header (40 bytes)
        *(uint32_t*)(p + 14) = 40;
        *(int32_t*)(p + 18) = width;
        *(int32_t*)(p + 22) = -height;  // top-down
        *(uint16_t*)(p + 26) = 1;       // planes
        *(uint16_t*)(p + 28) = 32;      // bpp
        *(uint32_t*)(p + 30) = 0;       // no compression
        *(uint32_t*)(p + 34) = pixel_size;
        *(uint32_t*)(p + 38) = 2835;    // h res
        *(uint32_t*)(p + 42) = 2835;    // v res
        *(uint32_t*)(p + 46) = 0;
        *(uint32_t*)(p + 50) = 0;

        memcpy(p + 54, bgra, pixel_size);
        return result;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IWICImagingFactory* factory = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IStream* stream = nullptr;
    IPropertyBag2* props = nullptr;

    HRESULT hr;

    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&factory));
    if (FAILED(hr)) goto cleanup;

    // Create in-memory stream
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    if (FAILED(hr)) goto cleanup;

    // Create JPEG encoder
    hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
    if (FAILED(hr)) goto cleanup;

    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) goto cleanup;

    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) goto cleanup;

    // Set JPEG quality
    {
        PROPBAG2 option = {};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT val;
        VariantInit(&val);
        val.vt = VT_R4;
        val.fltVal = quality / 100.0f;
        props->Write(1, &option, &val);
    }

    hr = frame->Initialize(props);
    if (FAILED(hr)) goto cleanup;

    hr = frame->SetSize(width, height);
    if (FAILED(hr)) goto cleanup;

    {
        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
        hr = frame->SetPixelFormat(&fmt);
        if (FAILED(hr)) goto cleanup;

        UINT stride = width * 4;
        UINT bufSize = stride * height;
        hr = frame->WritePixels(height, stride, bufSize, const_cast<BYTE*>(bgra));
        if (FAILED(hr)) goto cleanup;
    }

    hr = frame->Commit();
    if (FAILED(hr)) goto cleanup;

    hr = encoder->Commit();
    if (FAILED(hr)) goto cleanup;

    // Read the stream into our result vector
    {
        STATSTG stat;
        stream->Stat(&stat, STATFLAG_NONAME);
        ULONG size = (ULONG)stat.cbSize.QuadPart;
        result.resize(size);

        LARGE_INTEGER zero = {};
        stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG read = 0;
        stream->Read(result.data(), size, &read);
        result.resize(read);
    }

cleanup:
    if (frame) frame->Release();
    if (props) props->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();

    return result;
}

} // namespace webify

#else

namespace webify {

std::vector<uint8_t> encode_jpeg(const uint8_t* bgra, int width, int height, int quality) {
    // Stub — no JPEG encoding on non-Windows
    fprintf(stderr, "encode_jpeg: not implemented on this platform\n");
    return {};
}

} // namespace webify

#endif
