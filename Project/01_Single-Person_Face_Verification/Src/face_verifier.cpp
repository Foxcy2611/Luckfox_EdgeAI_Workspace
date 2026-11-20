#include "face_verifier.h"
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace {
void normalize(std::vector<float>& x) {
    double norm=0;
    for(float v:x) {if(!std::isfinite(v)) throw std::runtime_error("SFace embedding non-finite");norm+=double(v)*v;}
    if(norm<1e-12) throw std::runtime_error("SFace embedding zero norm");
    const float inv=static_cast<float>(1.0/std::sqrt(norm));
    for(float& v:x) v*=inv;
}
}
FaceVerifier::FaceVerifier(const std::string& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in) throw std::runtime_error("Khong doc duoc embedding "+path);
    if(in.tellg()!=static_cast<std::streampos>(128*sizeof(float)))
        throw std::runtime_error("person_a.bin phai la 128 float32 LE = 512 bytes");
    reference_.resize(128);
    in.seekg(0);
    in.read(reinterpret_cast<char*>(reference_.data()),512);
    if(!in) throw std::runtime_error("Khong doc duoc 512 bytes reference");
    normalize(reference_);
}
float FaceVerifier::similarity(std::vector<float> current) const {
    if(current.size()!=reference_.size()) throw std::runtime_error("SFace output phai 128 chieu");
    normalize(current);
    double dot=0;
    for(std::size_t i=0;i<current.size();i++) dot+=double(current[i])*reference_[i];
    return std::clamp(static_cast<float>(dot),-1.f,1.f);
}
