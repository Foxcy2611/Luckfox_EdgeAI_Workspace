#include "preprocess.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>
#include <stdexcept>

Image crop_resize_rgb(const Image& input, Rect roi, int size) {
    if (!input.valid() || size <= 0 || size > 2048 || roi.w <= 0 || roi.h <= 0 ||
        roi.x < 0 || roi.y < 0 || roi.x + roi.w > input.width || roi.y + roi.h > input.height)
        throw std::runtime_error("ROI/anh dau vao khong hop le");
    Image result;
    result.width = size;
    result.height = size;
    result.rgb.resize(static_cast<std::size_t>(size) * size * 3);
    // Bilinear RGB. Gan giong resize OpenCV INTER_LINEAR; downsample lon OpenCV INTER_AREA khac.
    for (int y = 0; y < size; ++y) {
        const float sy = (static_cast<float>(y) + 0.5f) * roi.h / size - 0.5f + roi.y;
        const int y0 = std::max(roi.y, std::min(roi.y + roi.h - 1, static_cast<int>(std::floor(sy))));
        const int y1 = std::min(roi.y + roi.h - 1, y0 + 1);
        const float wy = std::max(0.0f, std::min(1.0f, sy - y0));
        for (int x = 0; x < size; ++x) {
            const float sx = (static_cast<float>(x) + 0.5f) * roi.w / size - 0.5f + roi.x;
            const int x0 = std::max(roi.x, std::min(roi.x + roi.w - 1, static_cast<int>(std::floor(sx))));
            const int x1 = std::min(roi.x + roi.w - 1, x0 + 1);
            const float wx = std::max(0.0f, std::min(1.0f, sx - x0));
            for (int c = 0; c < 3; ++c) {
                const auto sample = [&](int px, int py) {
                    return static_cast<float>(input.rgb[(static_cast<std::size_t>(py) * input.width + px) * 3 + c]);
                };
                const float a = (1.0f - wx) * sample(x0, y0) + wx * sample(x1, y0);
                const float b = (1.0f - wx) * sample(x0, y1) + wx * sample(x1, y1);
                const float value = (1.0f - wy) * a + wy * b;
                result.rgb[(static_cast<std::size_t>(y) * size + x) * 3 + c] =
                    static_cast<std::uint8_t>(std::lround(std::max(0.0f, std::min(255.0f, value))));
            }
        }
    }
    return result;
}


// Fixed-point bilinear interpolation, optimized for camera 640x480 ->
// YuNet 640x640. The camera and model share the same width, so there is
// no horizontal interpolation in that most common path.
// Keep geometry identical to the old half-pixel-coordinate implementation.
Image resize_rgb(const Image& input, int width, int height) {
    if (!input.valid() || width <= 0 || height <= 0 || width > 2048 || height > 2048)
        throw std::runtime_error("resize_rgb: kich thuoc khong hop le");

    Image output;
    output.width = width;
    output.height = height;
    output.rgb.resize(static_cast<std::size_t>(width) * height * 3);

    // A direct copy is always preferable to a redundant resize.
    if (input.width == width && input.height == height) {
        std::copy(input.rgb.begin(), input.rgb.end(), output.rgb.begin());
        return output;
    }

    // 16-bit fractional interpolation; one coordinate computation per row.
    // The +32768 term rounds back to uint8. Maximum intermediate is
    // 255 * 65536 + 32768, so uint32_t arithmetic cannot overflow.
    constexpr std::uint32_t kOne = 1u << 16;

    // Hot path in this project: 640x480 camera -> 640x640 YuNet input.
    if (input.width == width) {
        const std::size_t row_bytes = static_cast<std::size_t>(width) * 3;
        for (int y = 0; y < height; ++y) {
            const float src_y = (static_cast<float>(y) + 0.5f) * input.height /
                                height - 0.5f;
            const int y0 = std::max(0, std::min(input.height - 1,
                                              static_cast<int>(std::floor(src_y))));
            const int y1 = std::min(input.height - 1, y0 + 1);
            const float fraction = std::max(0.0f, std::min(1.0f, src_y - y0));
            const auto wy = static_cast<std::uint32_t>(std::lround(fraction * kOne));

            const auto* a = input.rgb.data() + static_cast<std::size_t>(y0) * row_bytes;
            const auto* b = input.rgb.data() + static_cast<std::size_t>(y1) * row_bytes;
            auto* dst = output.rgb.data() + static_cast<std::size_t>(y) * row_bytes;
            if (y0 == y1 || wy == 0) {
                std::memcpy(dst, a, row_bytes);
            } else if (wy == kOne) {
                std::memcpy(dst, b, row_bytes);
            } else {
                const std::uint32_t wa = kOne - wy;
                for (std::size_t i = 0; i < row_bytes; ++i) {
                    dst[i] = static_cast<std::uint8_t>(
                        (wa * a[i] + wy * b[i] + (kOne / 2)) >> 16);
                }
            }
        }
        return output;
    }

    // Generic fallback if the camera width changes: precompute horizontal
    // coordinates once per frame, not inside the inner pixel loop.
    struct XMap { int x0, x1; std::uint32_t wx; };
    std::vector<XMap> xm(static_cast<std::size_t>(width));
    for (int x = 0; x < width; ++x) {
        const float src_x = (static_cast<float>(x) + 0.5f) * input.width /
                            width - 0.5f;
        const int x0 = std::max(0, std::min(input.width - 1,
                                          static_cast<int>(std::floor(src_x))));
        const int x1 = std::min(input.width - 1, x0 + 1);
        const float fraction = std::max(0.0f, std::min(1.0f, src_x - x0));
        xm[static_cast<std::size_t>(x)] = {
            x0, x1, static_cast<std::uint32_t>(std::lround(fraction * kOne))};
    }

    const std::size_t input_row_bytes = static_cast<std::size_t>(input.width) * 3;
    const std::size_t output_row_bytes = static_cast<std::size_t>(width) * 3;
    for (int y = 0; y < height; ++y) {
        const float src_y = (static_cast<float>(y) + 0.5f) * input.height /
                            height - 0.5f;
        const int y0 = std::max(0, std::min(input.height - 1,
                                          static_cast<int>(std::floor(src_y))));
        const int y1 = std::min(input.height - 1, y0 + 1);
        const float fraction = std::max(0.0f, std::min(1.0f, src_y - y0));
        const auto wy = static_cast<std::uint32_t>(std::lround(fraction * kOne));
        const auto* row0 = input.rgb.data() + static_cast<std::size_t>(y0) * input_row_bytes;
        const auto* row1 = input.rgb.data() + static_cast<std::size_t>(y1) * input_row_bytes;
        auto* dst = output.rgb.data() + static_cast<std::size_t>(y) * output_row_bytes;

        for (int x = 0; x < width; ++x) {
            const auto& map = xm[static_cast<std::size_t>(x)];
            const std::uint32_t wx = map.wx;
            const std::uint32_t w0 = kOne - wx;
            for (int c = 0; c < 3; ++c) {
                const std::size_t i0 = static_cast<std::size_t>(map.x0) * 3 + c;
                const std::size_t i1 = static_cast<std::size_t>(map.x1) * 3 + c;
                const std::uint32_t v0 = (w0 * row0[i0] + wx * row0[i1] + kOne / 2) >> 16;
                const std::uint32_t v1 = (w0 * row1[i0] + wx * row1[i1] + kOne / 2) >> 16;
                dst[static_cast<std::size_t>(x) * 3 + c] = static_cast<std::uint8_t>(
                    ((kOne - wy) * v0 + wy * v1 + kOne / 2) >> 16);
            }
        }
    }
    return output;
}