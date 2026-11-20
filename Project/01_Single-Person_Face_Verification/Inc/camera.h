#pragma once
#include "image.h"
#include <cstdio>
bool read_raw_rgb_frame(std::FILE* stream, int width, int height, Image& output);
