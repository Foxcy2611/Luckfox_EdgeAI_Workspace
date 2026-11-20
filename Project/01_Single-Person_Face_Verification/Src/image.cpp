#include "image.h"
#include <fstream>
#include <limits>
#include <stdexcept>
#include <cctype>

namespace {
std::string next_token(std::istream& in) {
    std::string result;
    while (true) {
        int c = in.peek();
        if (c == EOF) throw std::runtime_error("PPM: ket thuc header dot ngot");
        if (std::isspace(static_cast<unsigned char>(c))) { in.get(); continue; }
        if (c == '#') { in.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); continue; }
        break;
    }
    while (true) {
        const int c = in.peek();
        if (c == EOF || std::isspace(static_cast<unsigned char>(c)) || c == '#') break;
        result += static_cast<char>(in.get());
    }
    return result;
}
}

Image read_ppm(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Khong mo duoc anh PPM: " + path);
    if (next_token(in) != "P6") throw std::runtime_error("Chi ho tro PPM P6 RGB8 (khong ho tro JPG/PNG truc tiep)");
    int w = std::stoi(next_token(in));
    int h = std::stoi(next_token(in));
    const int maxval = std::stoi(next_token(in));
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || maxval != 255)
        throw std::runtime_error("PPM: W/H/maxval khong hop le (toi da 4096x4096, maxval=255)");
    const int separator = in.get();
    if (separator == EOF || !std::isspace(static_cast<unsigned char>(separator)))
        throw std::runtime_error("PPM: thieu whitespace sau maxval");
    Image img;
    img.width = w;
    img.height = h;
    img.rgb.resize(static_cast<std::size_t>(w) * h * 3);
    in.read(reinterpret_cast<char*>(img.rgb.data()), static_cast<std::streamsize>(img.rgb.size()));
    if (!in) throw std::runtime_error("PPM: pixel khong du");
    return img;
}
