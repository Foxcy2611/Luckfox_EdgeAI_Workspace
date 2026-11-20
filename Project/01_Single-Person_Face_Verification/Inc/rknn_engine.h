#pragma once
#include "image.h"
#include "face_detector.h"
#include <rknn_api.h>
#include <string>
#include <vector>
class RknnEngine {
public:
    ~RknnEngine();
    RknnEngine()=default;
    RknnEngine(const RknnEngine&)=delete;
    RknnEngine& operator=(const RknnEngine&)=delete;
    void load(const std::string& model_path);
    std::vector<NamedTensor> run(const Image& input_rgb,bool input_bgr);
    int width() const { return width_; }
    int height() const { return height_; }
    void close() noexcept;
private:
    rknn_context ctx_=0;
    rknn_tensor_attr input_attr_{};
    rknn_tensor_mem* input_mem_=nullptr;
    std::vector<rknn_tensor_attr> out_native_, out_logical_;
    std::vector<rknn_tensor_mem*> out_mem_;
    int width_=0,height_=0,row_stride_=0;
    bool ready_=false;
};
