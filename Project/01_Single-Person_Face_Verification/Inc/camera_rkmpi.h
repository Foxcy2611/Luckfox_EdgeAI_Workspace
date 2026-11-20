#pragma once
#include "image.h"
#include <string>
class RkmpiCamera {
public:
    ~RkmpiCamera();
    void open(int width,int height,int channel,const std::string& iq_dir);
    bool capture(Image& output,int timeout_ms);
    void close() noexcept;
private:
    int width_=0, height_=0, channel_=0;
    bool isp_started_=false,sys_started_=false,dev_enabled_=false,channel_enabled_=false;
};
