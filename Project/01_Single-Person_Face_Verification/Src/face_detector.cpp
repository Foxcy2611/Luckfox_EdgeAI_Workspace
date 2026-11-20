#include "face_detector.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
float iou(const FaceDetection& a, const FaceDetection& b) {
    const float x1=std::max(a.x,b.x), y1=std::max(a.y,b.y);
    const float x2=std::min(a.x+a.w,b.x+b.w), y2=std::min(a.y+a.h,b.y+b.h);
    const float overlap=std::max(0.f,x2-x1)*std::max(0.f,y2-y1);
    const float uni=a.w*a.h+b.w*b.h-overlap;
    return uni>0.f ? overlap/uni : 0.f;
}
const NamedTensor& output_for(const std::vector<NamedTensor>& out,const std::string& key,int fallback,std::size_t expected) {
    // RKNN often preserves original ONNX tensor names. Otherwise use OpenCV's order,
    // but refuse to decode tensors with incompatible element counts.
    const NamedTensor* p=nullptr;
    for(const auto& t:out) if(t.name==key || t.name.find(key)!=std::string::npos) {p=&t;break;}
    if(!p) {
        if(fallback>=static_cast<int>(out.size())) throw std::runtime_error("YuNet: thieu output "+key);
        p=&out[static_cast<std::size_t>(fallback)];
    }
    if(p->data.size()!=expected)
        throw std::runtime_error("YuNet: output "+key+" sai so phan tu ("+std::to_string(p->data.size())+" != "+std::to_string(expected)+"); can kiem tra tensor order/shape");
    return *p;
}
}
std::vector<FaceDetection> decode_yunet(const std::vector<NamedTensor>& outputs,int model_w,int model_h,float conf,float nms,int limit) {
    if(outputs.size()!=12 || model_w%32 || model_h%32) throw std::runtime_error("YuNet: yeu cau 12 outputs, input chia het 32");
    std::vector<FaceDetection> candidates;
    const int strides[3]={8,16,32};
    for(int s=0;s<3;s++) {
        const int stride=strides[s], cols=model_w/stride,rows=model_h/stride;
        const std::size_t n=static_cast<std::size_t>(cols)*rows;
        const auto& cls=output_for(outputs,"cls_"+std::to_string(stride),s,n).data;
        const auto& obj=output_for(outputs,"obj_"+std::to_string(stride),s+3,n).data;
        const auto& bbox=output_for(outputs,"bbox_"+std::to_string(stride),s+6,n*4).data;
        const auto& kps=output_for(outputs,"kps_"+std::to_string(stride),s+9,n*10).data;
        for(int r=0;r<rows;r++) for(int c=0;c<cols;c++) {
            const auto idx=static_cast<std::size_t>(r)*cols+c;
            if(!std::isfinite(cls[idx]) || !std::isfinite(obj[idx])) continue;
            const float score=std::sqrt(std::clamp(cls[idx],0.f,1.f)*std::clamp(obj[idx],0.f,1.f));
            if(score<conf) continue;
            const float* b=bbox.data()+idx*4;
            if(!std::isfinite(b[0])||!std::isfinite(b[1])||!std::isfinite(b[2])||!std::isfinite(b[3])||
               std::abs(b[2])>10.f||std::abs(b[3])>10.f) continue;
            FaceDetection f;
            f.w=std::exp(b[2])*stride; f.h=std::exp(b[3])*stride;
            f.x=(c+b[0])*stride-f.w*0.5f;
            f.y=(r+b[1])*stride-f.h*0.5f;
            f.score=score;
            if(f.w<=0||f.h<=0||!std::isfinite(f.x)||!std::isfinite(f.y)) continue;
            bool ok=true;
            for(int j=0;j<5;j++) {
                f.landmarks[j]={ (kps[idx*10+j*2]+c)*stride,(kps[idx*10+j*2+1]+r)*stride };
                ok=ok&&std::isfinite(f.landmarks[j].x)&&std::isfinite(f.landmarks[j].y);
            }
            if(ok) candidates.push_back(f);
        }
    }
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.score>b.score;});
    std::vector<FaceDetection> selected;
    for(const auto& f:candidates) {
        bool suppress=false;
        for(const auto& keep:selected) if(iou(f,keep)>nms) {suppress=true;break;}
        if(!suppress) selected.push_back(f);
        if(static_cast<int>(selected.size())>=limit) break;
    }
    return selected;
}
