#include "rknn_engine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void ck(int rc,const char* what) {
    if(rc!=RKNN_SUCC) throw std::runtime_error(std::string(what)+" RKNN="+std::to_string(rc));
}
std::vector<std::uint8_t> read_model(const std::string& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f) throw std::runtime_error("Missing model: "+path);
    const auto size=f.tellg();
    if(size<=0||size>128*1024*1024) throw std::runtime_error("Model size invalid");
    std::vector<std::uint8_t> v(static_cast<std::size_t>(size));
    f.seekg(0);f.read(reinterpret_cast<char*>(v.data()),v.size());
    if(!f) throw std::runtime_error("Cannot load model: "+path);
    return v;
}
float half_to_float(std::uint16_t bits) {
    const std::uint32_t sign=(static_cast<std::uint32_t>(bits & 0x8000))<<16;
    std::uint32_t exp=(bits>>10)&31,frac=bits&1023;
    std::uint32_t v=0;
    if(exp==0) {
        if(frac==0) v=sign;
        else {int e=-14;while(!(frac&1024)){frac<<=1;--e;}frac&=1023;v=sign|((e+127)<<23)|(frac<<13);}
    } else if(exp==31) v=sign|0x7f800000u|(frac<<13);
    else v=sign|((exp+112)<<23)|(frac<<13);
    float out;std::memcpy(&out,&v,4);return out;
}
std::size_t elem_size(rknn_tensor_type t) {
    switch(t) {
      case RKNN_TENSOR_INT8:case RKNN_TENSOR_UINT8:return 1;
      case RKNN_TENSOR_INT16:case RKNN_TENSOR_FLOAT16:return 2;
      case RKNN_TENSOR_INT32:case RKNN_TENSOR_FLOAT32:return 4;
      default:throw std::runtime_error("RKNN output dtype unsupported");
    }
}
float read_scalar(const std::uint8_t* ptr,rknn_tensor_type type,float scale,int zp) {
    switch(type) {
      case RKNN_TENSOR_INT8:return (static_cast<float>(*reinterpret_cast<const std::int8_t*>(ptr))-zp)*scale;
      case RKNN_TENSOR_UINT8:return (static_cast<float>(*ptr)-zp)*scale;
      case RKNN_TENSOR_INT16:{std::int16_t v;std::memcpy(&v,ptr,2);return (v-zp)*scale;}
      case RKNN_TENSOR_FLOAT16:{std::uint16_t v;std::memcpy(&v,ptr,2);return half_to_float(v);}
      case RKNN_TENSOR_FLOAT32:{float v;std::memcpy(&v,ptr,4);return v;}
      case RKNN_TENSOR_INT32:{std::int32_t v;std::memcpy(&v,ptr,4);return (v-zp)*scale;}
      default:throw std::runtime_error("Unknown tensor type");
    }
}
void dump_attr(const char* prefix,const rknn_tensor_attr& a) {
    std::cerr<<prefix<<" name='"<<a.name<<"' fmt="<<static_cast<int>(a.fmt)
             <<" type="<<static_cast<int>(a.type)<<" dims=[";
    for(std::uint32_t k=0;k<a.n_dims;k++) std::cerr<<(k?",":"")<<a.dims[k];
    std::cerr<<"] elems="<<a.n_elems<<" size="<<a.size
             <<" native_stride="<<a.size_with_stride<<" width_stride="<<a.w_stride
             <<" scale="<<a.scale<<" zp="<<a.zp<<"\n";
}
// Decode RV1106 zero-copy native format. NC1HWC2 is CHANNEL-BLOCKED:
// its padding cannot be removed by simply memcpy'ing logical n_elems values.
std::vector<float> unpack_native(const rknn_tensor_attr& native,
                                 const rknn_tensor_attr& logical,const rknn_tensor_mem* mem) {
    if(!mem||!mem->virt_addr) throw std::runtime_error("RKNN output buffer null");
    const auto* src=static_cast<const std::uint8_t*>(mem->virt_addr);
    const std::size_t unit=elem_size(native.type), total=logical.n_elems;
    if(total==0||total>1000000) throw std::runtime_error("Output element count invalid");
    std::vector<float> result(total);
    auto get=[&](std::size_t i)->float {
        if(i>mem->size/unit || i*unit+unit>mem->size) throw std::runtime_error("RKNN output memory overrun");
        return read_scalar(src+i*unit,native.type,native.scale,native.zp);
    };
    if(native.fmt==RKNN_TENSOR_NC1HWC2) {
        if(native.n_dims!=5) throw std::runtime_error("NC1HWC2 requires 5 dims: inspect RKNN IO logs");
        const std::size_t batch=native.dims[0], c1=native.dims[1],h=native.dims[2],w=native.dims[3],c2=native.dims[4];
        if(!batch||!c1||!h||!w||!c2||c2>128||batch!=1||total%(h*w)!=0)
            throw std::runtime_error("Unsupported NC1HWC2 tensor geometry");
        const std::size_t channels=total/(h*w);
        if(channels>c1*c2) throw std::runtime_error("NC1HWC2 channel mismatch");
        // First reconstruct an unpadded NCHW representation.
        for(std::size_t c=0;c<channels;c++) for(std::size_t yy=0;yy<h;yy++) for(std::size_t xx=0;xx<w;xx++) {
            const auto raw=((((c/c2)*h+yy)*w+xx)*c2+c%c2);
            result[(c*h+yy)*w+xx]=get(raw);
        }
        // For output exposed as NHWC logically, reshape NCHW into NHWC.
        if(logical.fmt==RKNN_TENSOR_NHWC) {
            std::vector<float> nhwc(total);
            for(std::size_t yy=0;yy<h;yy++) for(std::size_t xx=0;xx<w;xx++) for(std::size_t c=0;c<channels;c++)
                nhwc[(yy*w+xx)*channels+c]=result[(c*h+yy)*w+xx];
            return nhwc;
        }
    } else {
        // NCHW/NHWC/UNDEFINED native: non-padded contiguous values.
        if(total*unit>mem->size) throw std::runtime_error("Output buffer too small");
        for(std::size_t i=0;i<total;i++) result[i]=get(i);
    }
    return result;
}
}

RknnEngine::~RknnEngine(){close();}
void RknnEngine::load(const std::string& path) {
    close();
    try {
        auto bytes=read_model(path);
        ck(rknn_init(&ctx_,bytes.data(),static_cast<uint32_t>(bytes.size()),0,nullptr),"rknn_init");
        rknn_input_output_num io{};
        ck(rknn_query(ctx_,RKNN_QUERY_IN_OUT_NUM,&io,sizeof(io)),"query IO count");
        if(io.n_input!=1||io.n_output==0||io.n_output>16)
            throw std::runtime_error("Expected 1 input / 1..16 output, got "+std::to_string(io.n_input)+" / "+std::to_string(io.n_output));
        input_attr_.index=0;
        ck(rknn_query(ctx_,RKNN_QUERY_INPUT_ATTR,&input_attr_,sizeof(input_attr_)),"query input attr");
        dump_attr("[RKNN INPUT]",input_attr_);
        // Search for two large spatial dimensions. Handles logical NCHW and NHWC.
        std::vector<int> spatial;
        for(std::uint32_t i=0;i<input_attr_.n_dims;i++) if(input_attr_.dims[i]>16&&input_attr_.dims[i]<=2048) spatial.push_back(input_attr_.dims[i]);
        if(spatial.size()!=2) throw std::runtime_error("Unrecognized input shape (expected RGB NHWC/NCHW)");
        height_=spatial[0];width_=spatial[1];
        const std::size_t expected=static_cast<std::size_t>(width_)*height_*3;
        if(input_attr_.n_elems!=expected) throw std::runtime_error("Model not 3-channel image input");
        out_native_.resize(io.n_output);out_logical_.resize(io.n_output);out_mem_.assign(io.n_output,nullptr);
        for(std::uint32_t i=0;i<io.n_output;i++) {
            out_native_[i].index=i;
            out_logical_[i].index=i;
            ck(rknn_query(ctx_,RKNN_QUERY_NATIVE_OUTPUT_ATTR,&out_native_[i],sizeof(rknn_tensor_attr)),"query native output");
            ck(rknn_query(ctx_,RKNN_QUERY_OUTPUT_ATTR,&out_logical_[i],sizeof(rknn_tensor_attr)),"query logical output");
            dump_attr("[RKNN OUT native]",out_native_[i]);
            dump_attr("[RKNN OUT logical]",out_logical_[i]);
        }
        row_stride_=std::max(width_,static_cast<int>(input_attr_.w_stride));
        if(row_stride_>4096) throw std::runtime_error("Invalid input row stride");
        // RV1106: uses zero-copy rknn_set_io_mem; normal rknn_outputs_get is unavailable.
        input_attr_.type=RKNN_TENSOR_UINT8;
        input_attr_.fmt=RKNN_TENSOR_NHWC;
        const std::size_t bytes_needed=static_cast<std::size_t>(row_stride_)*height_*3;
        const auto mem_size=std::max(bytes_needed,static_cast<std::size_t>(input_attr_.size_with_stride));
        input_mem_=rknn_create_mem(ctx_,static_cast<uint32_t>(mem_size));
        if(!input_mem_) throw std::runtime_error("rknn_create_mem input failed");
        ck(rknn_set_io_mem(ctx_,input_mem_,&input_attr_),"rknn_set_io_mem input");
        for(std::size_t i=0;i<out_mem_.size();i++) {
            const auto& a=out_native_[i];
            const auto bytes_out=std::max(static_cast<std::size_t>(a.size_with_stride),static_cast<std::size_t>(a.size));
            if(bytes_out==0) throw std::runtime_error("output size zero");
            out_mem_[i]=rknn_create_mem(ctx_,static_cast<uint32_t>(bytes_out));
            if(!out_mem_[i]) throw std::runtime_error("rknn_create_mem output failed");
            ck(rknn_set_io_mem(ctx_,out_mem_[i],&out_native_[i]),"rknn_set_io_mem output");
        }
        ready_=true;
        std::cerr<<"[RKNN] loaded "<<path<<" inputs="<<width_<<"x"<<height_<<" outputs="<<out_mem_.size()<<"\n";
    } catch(...) {close();throw;}
}
std::vector<NamedTensor> RknnEngine::run(const Image& rgb,bool input_bgr) {
    if(!ready_||!rgb.valid()||rgb.width!=width_||rgb.height!=height_)
        throw std::runtime_error("RKNN input size mismatch, expected "+std::to_string(width_)+"x"+std::to_string(height_));
    auto* dst=static_cast<std::uint8_t*>(input_mem_->virt_addr);
    if(!dst) throw std::runtime_error("input virt_addr null");
    std::memset(dst,0,input_mem_->size);
    for(int y=0;y<height_;y++) {
        for(int x=0;x<width_;x++) {
            const auto si=(static_cast<std::size_t>(y)*width_+x)*3;
            const auto di=(static_cast<std::size_t>(y)*row_stride_+x)*3;
            if(input_bgr) {dst[di]=rgb.rgb[si+2];dst[di+1]=rgb.rgb[si+1];dst[di+2]=rgb.rgb[si];}
            else {dst[di]=rgb.rgb[si];dst[di+1]=rgb.rgb[si+1];dst[di+2]=rgb.rgb[si+2];}
        }
    }
    ck(rknn_run(ctx_,nullptr),"rknn_run");
    std::vector<NamedTensor> outputs;
    for(std::size_t i=0;i<out_mem_.size();i++) {
        NamedTensor t;
        t.name=out_logical_[i].name;
        if(t.name.empty()) t.name=out_native_[i].name;
        t.data=unpack_native(out_native_[i],out_logical_[i],out_mem_[i]);
        for(std::uint32_t d=0;d<out_logical_[i].n_dims;d++) t.shape.push_back(out_logical_[i].dims[d]);
        outputs.push_back(std::move(t));
    }
    return outputs;
}
void RknnEngine::close() noexcept {
    for(auto* p:out_mem_) if(p && ctx_) rknn_destroy_mem(ctx_,p);
    out_mem_.clear();out_native_.clear();out_logical_.clear();
    if(input_mem_&&ctx_) rknn_destroy_mem(ctx_,input_mem_);
    input_mem_=nullptr;
    if(ctx_) rknn_destroy(ctx_);
    ctx_=0;ready_=false;row_stride_=width_=height_=0;
}
