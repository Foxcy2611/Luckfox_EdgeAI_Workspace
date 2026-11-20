#pragma once
#include "image.h"
Image crop_resize_rgb(const Image& input,Rect roi,int size);
Image resize_rgb(const Image& input,int width,int height);
