#pragma once

#include <cstdint>
#include <vector>

namespace webify {

// Encode BGRA pixels to JPEG using Windows Imaging Component (WIC).
// Returns empty vector on failure.
std::vector<uint8_t> encode_jpeg(const uint8_t* bgra, int width, int height, int quality = 50);

} // namespace webify
