#pragma once
#include <cstdint>
#include <string>
#include <vector>
struct Image {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgb;
    bool valid() const { return width > 0 && height > 0 && rgb.size() == static_cast<std::size_t>(width) * height * 3; }
};
struct Rect { int x=0, y=0, w=0, h=0; };
Image read_ppm(const std::string& path);
