#include "camera_rkmpi.h"
#include "face_alignment.h"
#include "face_detector.h"
#include "face_verifier.h"
#include "image.h"
#include "preprocess.h"
#include "preview_stream.h"
#include "rknn_engine.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <memory>

namespace {
volatile std::sig_atomic_t g_running=1;
void stop(int){g_running=0;}
struct Options {
    std::string yunet="Models/detectors/yunet.rknn";
    std::string sface="Models/recognizers/sface.rknn";
    std::string reference="Embeddings/person_a.bin";
    std::string iq_dir="/etc/iqfiles";
    std::string image;
    int width=640,height=480,channel=0,preview_port=8765,preview_every=3,max_frames=0,interval_ms=0,max_faces=3;
    float detect_threshold=0.7f,match_threshold=0.363f;
    bool detect_only=false,inspect=false;
};
void usage(const char* bin) {
    std::cout<<"Usage: "<<bin<<" [--camera] [--yunet PATH] [--sface PATH] [--reference PATH]\n"
             <<"  --camera (default)           SC3336 RK_MPI real-time\n"
             <<"  --image frame.ppm            Single PPM RGB image (for offline debugging)\n"
             <<"  --detect-only               YuNet only, SFace not loaded\n"
             <<"  --inspect-models            Print tensor details and exit (no camera)\n"
             <<"  --width 640 --height 480 --channel 0 --iq-dir /etc/iqfiles\n"
             <<"  --detect-threshold 0.7 --match-threshold 0.363 --max-faces 3\n"
             <<"  --preview-port 8765 (0 disables) --preview-every 3\n"
             <<"  --max-frames 0 (0=infinite) --interval-ms 0\n";
}
Options parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;i++) {
        const std::string opt=argv[i];
        auto val=[&]()->std::string {if(i+1>=argc)throw std::runtime_error("Missing value for "+opt);return argv[++i];};
        if(opt=="--help"||opt=="-h"){usage(argv[0]);std::exit(0);}
        else if(opt=="--camera"){}
        else if(opt=="--yunet")o.yunet=val();
        else if(opt=="--sface")o.sface=val();
        else if(opt=="--reference")o.reference=val();
        else if(opt=="--image")o.image=val();
        else if(opt=="--iq-dir")o.iq_dir=val();
        else if(opt=="--width")o.width=std::stoi(val());
        else if(opt=="--height")o.height=std::stoi(val());
        else if(opt=="--channel")o.channel=std::stoi(val());
        else if(opt=="--preview-port")o.preview_port=std::stoi(val());
        else if(opt=="--preview-every")o.preview_every=std::stoi(val());
        else if(opt=="--max-frames")o.max_frames=std::stoi(val());
        else if(opt=="--interval-ms")o.interval_ms=std::stoi(val());
        else if(opt=="--max-faces")o.max_faces=std::stoi(val());
        else if(opt=="--detect-threshold")o.detect_threshold=std::stof(val());
        else if(opt=="--match-threshold")o.match_threshold=std::stof(val());
        else if(opt=="--detect-only")o.detect_only=true;
        else if(opt=="--inspect-models")o.inspect=true;
        else throw std::runtime_error("Unknown option: "+opt);
    }
    if(o.width<32||o.height<32||o.width>1920||o.height>1080||(o.width&1)||(o.height&1)||
       o.preview_port<0||o.preview_port>65535||o.preview_every<1||o.max_frames<0||o.interval_ms<0||
       o.channel<0||o.channel>2||o.max_faces<1||o.max_faces>8||o.detect_threshold<=0||o.detect_threshold>=1||
       o.match_threshold< -1||o.match_threshold>1)
        throw std::runtime_error("Invalid CLI options");
    return o;
}
struct StageTimings {
    double resize_ms=0.0, yunet_run_ms=0.0, yunet_decode_ms=0.0, sface_ms=0.0;
};
inline double elapsed_ms(std::chrono::steady_clock::time_point a,
                         std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double,std::milli>(b-a).count();
}
std::vector<FaceResult> process(RknnEngine& detector,RknnEngine* recognizer,FaceVerifier* verifier,
                                const Image& src,const Options& options,std::size_t index,StageTimings* stage) {
    // Detection input matches the *fixed* converted YuNet model, e.g. 640x640.
    // Original frame may be 640x480. Map box/landmarks back to original coordinates.
    const auto a=std::chrono::steady_clock::now();
    const auto resized=resize_rgb(src,detector.width(),detector.height());
    const auto b=std::chrono::steady_clock::now();
    const auto netout=detector.run(resized,true); // Includes input copy, rknn_run(), output unpack
    const auto c=std::chrono::steady_clock::now();
    auto faces=decode_yunet(netout,detector.width(),detector.height(),options.detect_threshold,0.3f,options.max_faces);
    const auto d=std::chrono::steady_clock::now();
    if(stage) {
        stage->resize_ms+=elapsed_ms(a,b);
        stage->yunet_run_ms+=elapsed_ms(b,c);
        stage->yunet_decode_ms+=elapsed_ms(c,d);
    }
    const float sx=static_cast<float>(src.width)/detector.width();
    const float sy=static_cast<float>(src.height)/detector.height();
    std::vector<FaceResult> results;
    for(auto& face:faces) {
        face.x*=sx;face.w*=sx;face.y*=sy;face.h*=sy;
        for(auto& p:face.landmarks){p.x*=sx;p.y*=sy;}
        FaceResult result;result.face=face;result.detect_only=options.detect_only;
        if(recognizer && verifier) {
            const auto sface_start=std::chrono::steady_clock::now();
            try {
                const auto aligned=align_face_sface(src,face); // 112x112 RGB
                auto tensors=recognizer->run(aligned,false); // OpenCV SFace swaps BGR->RGB
                if(tensors.size()!=1 || tensors[0].data.size()!=128)
                    throw std::runtime_error("SFace expected one 128-D output");
                result.similarity=verifier->similarity(std::move(tensors[0].data));
                result.verified=verifier->accept(result.similarity,options.match_threshold);
            } catch(const std::exception& e) {
                std::cerr<<"[FACE] embedding error: "<<e.what()<<"\n";
            }
            if(stage) stage->sface_ms+=elapsed_ms(sface_start,std::chrono::steady_clock::now());
        }
        results.push_back(result);
    }
    // Do not flush stdout on every single frame: the preview already shows results.
    if(index==1 || index%10==0) {
        std::cout<<"[FRAME "<<index<<"] faces="<<results.size();
        for(const auto& f:results)
            std::cout<<" {det="<<std::fixed<<std::setprecision(3)<<f.face.score
                     <<", cos="<<f.similarity<<", "<<(options.detect_only?"DETECTED":(f.verified?"VERIFIED":"UNKNOWN"))<<"}";
        std::cout<<std::endl;
    }
    return results;
}
}
int main(int argc,char** argv) {
    try {
        auto opt=parse(argc,argv);
        std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
        RknnEngine detector;detector.load(opt.yunet);
        if(detector.width()%32||detector.height()%32)throw std::runtime_error("YuNet spatial size must be divisible by 32");
        std::unique_ptr<RknnEngine> recognizer;
        std::unique_ptr<FaceVerifier> verifier;
        if(!opt.detect_only) {
            recognizer=std::make_unique<RknnEngine>();
            recognizer->load(opt.sface);
            if(recognizer->width()!=112||recognizer->height()!=112)
                throw std::runtime_error("SFace RKNN must use 112x112 image input");
            verifier=std::make_unique<FaceVerifier>(opt.reference);
        }
        if(opt.inspect) {std::cout<<"[INFO] Models loaded; tensor metadata above.\n";return 0;}
        if(!opt.image.empty()) {
            auto image=read_ppm(opt.image);
            process(detector,recognizer.get(),verifier.get(),image,opt,1,nullptr);
            return 0;
        }
        RkmpiCamera camera;
        PreviewServer preview;
        if(opt.preview_port) preview.start(opt.preview_port);
        camera.open(opt.width,opt.height,opt.channel,opt.iq_dir);
        std::size_t count=0;int failures=0;
        using Clock=std::chrono::steady_clock;
        double capture_total_ms=0.0,infer_total_ms=0.0,preview_total_ms=0.0,loop_total_ms=0.0;
        double resize_total_ms=0.0,yunet_run_total_ms=0.0,decode_total_ms=0.0,sface_total_ms=0.0;
        int samples=0;
        while(g_running && (opt.max_frames==0||count<static_cast<std::size_t>(opt.max_frames))) {
            const auto t0=Clock::now();
            Image image;
            if(!camera.capture(image,1500)) {
                if(++failures>=5) throw std::runtime_error("RK_MPI VI frame timeout 5 times; check rkipc");
                continue;
            }
            const auto t1=Clock::now();
            failures=0;++count;
            StageTimings stage;
            auto results=process(detector,recognizer.get(),verifier.get(),image,opt,count,&stage);
            const auto t2=Clock::now();
            if(opt.preview_port && count%static_cast<std::size_t>(opt.preview_every)==0)
                preview.publish(image,results,count);
            const auto t3=Clock::now();
            if(opt.interval_ms>0) std::this_thread::sleep_for(std::chrono::milliseconds(opt.interval_ms));
            const auto t4=Clock::now();
            auto ms=[](Clock::time_point a, Clock::time_point b) {
                return std::chrono::duration<double,std::milli>(b-a).count();
            };
            capture_total_ms+=ms(t0,t1);
            infer_total_ms+=ms(t1,t2);
            resize_total_ms+=stage.resize_ms;
            yunet_run_total_ms+=stage.yunet_run_ms;
            decode_total_ms+=stage.yunet_decode_ms;
            sface_total_ms+=stage.sface_ms;
            preview_total_ms+=ms(t2,t3);
            loop_total_ms+=ms(t0,t4);
            ++samples;
            if(samples>=30) {
                std::cerr<<"[PERF] "<<samples<<" frames | capture_avg="
                         <<std::fixed<<std::setprecision(1)<<capture_total_ms/samples
                         <<" ms | inference_avg="<<infer_total_ms/samples
                         <<" ms | preview_submit_avg="<<preview_total_ms/samples
                         <<" ms | pipeline_fps="<<(loop_total_ms>0?1000.0*samples/loop_total_ms:0.0)
                         <<"\n";
                std::cerr<<"[STAGES] "<<samples<<" frames | resize_avg="<<resize_total_ms/samples
                         <<" ms | yunet_run_avg="<<yunet_run_total_ms/samples
                         <<" ms | decode_avg="<<decode_total_ms/samples
                         <<" ms | sface_avg="<<sface_total_ms/samples
                         <<" ms (yunet_run includes copy + NPU + output unpack)\n";
                resize_total_ms=yunet_run_total_ms=decode_total_ms=sface_total_ms=0.0;
                capture_total_ms=infer_total_ms=preview_total_ms=loop_total_ms=0.0;
                samples=0;
            }
        }
        std::cerr<<"[INFO] frames="<<count<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"[FATAL] "<<e.what()<<"\n";
        return 1;
    }
}