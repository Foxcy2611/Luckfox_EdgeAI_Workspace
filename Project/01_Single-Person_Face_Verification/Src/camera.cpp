#include "camera.h"
#include <limits>
#include <stdexcept>

bool read_raw_rgb_frame(std::FILE* stream, int width, int height, Image& out) {
    if (!stream || width <= 0 || height <= 0 || width > 4096 || height > 4096)
        throw std::runtime_error("Kich thuoc raw frame khong hop le");
    const std::size_t bytes = static_cast<std::size_t>(width) * height * 3;
    out.width = width;
    out.height = height;
    out.rgb.resize(bytes);
    std::size_t filled = 0;
    while (filled < bytes) {
        const std::size_t n = std::fread(out.rgb.data() + filled, 1, bytes - filled, stream);
        if (n == 0) {
            if (std::ferror(stream)) throw std::runtime_error("Loi doc stdin RGB");
            if (filled != 0) throw std::runtime_error("Raw RGB frame bi cat giua chung");
            out = {};
            return false; // EOF sach
        }
        filled += n;
    }
    return true;
}
