#include "nv12.h"
#include <algorithm>
#include <stdexcept>

namespace {
inline std::uint8_t clamp255(int x) {
    return static_cast<std::uint8_t>(std::max(0, std::min(255, x)));
}
}

Image nv12_to_rgb(const std::uint8_t* src,
                  int width, int height,
                  int virtual_width, int virtual_height) {
    if (!src || width <= 0 || height <= 0 || (width & 1) || (height & 1) ||
        virtual_width < width || virtual_height < height ||
        virtual_width > 4096 || virtual_height > 4096)
        throw std::runtime_error("NV12: kich thuoc/stride khong hop le");

    // BT.601 limited range: phim chuan YUV tu ISP cua LuckFox.
    const std::uint8_t* yplane = src;
    const std::uint8_t* uvplane = src + static_cast<std::size_t>(virtual_width) * virtual_height;
    Image out;
    out.width = width;
    out.height = height;
    out.rgb.resize(static_cast<std::size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int yy = yplane[static_cast<std::size_t>(y) * virtual_width + x];
            const auto uv = static_cast<std::size_t>(y / 2) * virtual_width + (x & ~1);
            const int u = static_cast<int>(uvplane[uv]) - 128;
            const int v = static_cast<int>(uvplane[uv + 1]) - 128;
            const int c = std::max(0, yy - 16);
            const int r = (298 * c + 409 * v + 128) >> 8;
            const int g = (298 * c - 100 * u - 208 * v + 128) >> 8;
            const int b = (298 * c + 516 * u + 128) >> 8;
            const auto dst = (static_cast<std::size_t>(y) * width + x) * 3;
            out.rgb[dst] = clamp255(r);
            out.rgb[dst + 1] = clamp255(g);
            out.rgb[dst + 2] = clamp255(b);
        }
    }
    return out;
}
