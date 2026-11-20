#pragma once
#include "image.h"
#include <array>
#include <vector>
struct Point2f { float x=0, y=0; };
struct FaceDetection {
    float x=0,y=0,w=0,h=0,score=0;
    // OpenCV YuNet order: right eye, left eye, nose, right mouth, left mouth
    std::array<Point2f,5> landmarks{};
};
struct NamedTensor { std::string name; std::vector<float> data; std::vector<std::uint32_t> shape; };
std::vector<FaceDetection> decode_yunet(const std::vector<NamedTensor>& outputs,int model_w,int model_h,float conf=0.7f,float nms=0.3f,int limit=10);
