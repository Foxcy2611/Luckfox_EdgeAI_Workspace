#include "camera_rkmpi.h"
#include "nv12.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

// Cung cap boi SDK LuckFox. Phan ISP quan trong: dung thu vien sample_comm
// de khoi tao camera SC3336 voi IQ files goc tren board.
#include <sample_comm.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_vi.h>
#include <rk_mpi_mb.h>

namespace {
void mpi_check(RK_S32 rc, const std::string& what) {
    if (rc != RK_SUCCESS)
        throw std::runtime_error(what + ": RK MPI error=" + std::to_string(rc));
}
}

RkmpiCamera::~RkmpiCamera() { close(); }

void RkmpiCamera::open(int width, int height, int channel, const std::string& iq_dir) {
    close();
    if (width <= 0 || height <= 0 || width > 1920 || height > 1080 ||
        (width & 1) || (height & 1) || channel < 0 || channel > 2)
        throw std::runtime_error("Camera: width/height phai chan; channel 0..2");
    width_ = width;
    height_ = height;
    channel_ = channel;
    try {
        // Camera ISP dung RKAIQ. rkipc phai duoc dung truoc do.
        const RK_BOOL multi_sensor = RK_FALSE;
        const rk_aiq_working_mode_t mode = RK_AIQ_WORKING_MODE_NORMAL;
        mpi_check(SAMPLE_COMM_ISP_Init(0, mode, multi_sensor, iq_dir.c_str()), "ISP Init");
        isp_started_ = true;
        mpi_check(SAMPLE_COMM_ISP_Run(0), "ISP Run");

        mpi_check(RK_MPI_SYS_Init(), "RK_MPI_SYS_Init");
        sys_started_ = true;

        VI_DEV_ATTR_S dev_attr{};
        RK_S32 rc = RK_MPI_VI_GetDevAttr(0, &dev_attr);
        if (rc == RK_ERR_VI_NOT_CONFIG)
            mpi_check(RK_MPI_VI_SetDevAttr(0, &dev_attr), "VI SetDevAttr");
        else
            mpi_check(rc, "VI GetDevAttr");

        if (RK_MPI_VI_GetDevIsEnable(0) != RK_SUCCESS) {
            mpi_check(RK_MPI_VI_EnableDev(0), "VI EnableDev");
            dev_enabled_ = true;
        }
        VI_DEV_BIND_PIPE_S bind{};
        bind.u32Num = 1;
        bind.PipeId[0] = 0;
        mpi_check(RK_MPI_VI_SetDevBindPipe(0, &bind), "VI SetDevBindPipe");

        VI_CHN_ATTR_S attr{};
        attr.stIspOpt.u32BufCount = 3;
        attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        attr.stSize.u32Width = static_cast<RK_U32>(width);
        attr.stSize.u32Height = static_cast<RK_U32>(height);
        attr.enPixelFormat = RK_FMT_YUV420SP; // NV12
        attr.enCompressMode = COMPRESS_MODE_NONE;
        attr.u32Depth = 2; // > 0 de GetChnFrame co the lay buffer.
        mpi_check(RK_MPI_VI_SetChnAttr(0, channel_, &attr), "VI SetChnAttr");
        mpi_check(RK_MPI_VI_EnableChn(0, channel_), "VI EnableChn");
        channel_enabled_ = true;
        std::cerr << "[CAM] SC3336 / RKMPI VI0:" << channel_
                  << " | " << width_ << "x" << height_ << " NV12 -> RGB24\n";
    } catch (...) {
        close();
        throw;
    }
}

bool RkmpiCamera::capture(Image& output, int timeout_ms) {
    if (!channel_enabled_) throw std::runtime_error("Camera chua mo");
    VIDEO_FRAME_INFO_S frame{};
    const RK_S32 rc = RK_MPI_VI_GetChnFrame(0, channel_, &frame, timeout_ms);
    if (rc != RK_SUCCESS) return false;  // timeout; main co the thu lai
    try {
        if (frame.stVFrame.enPixelFormat != RK_FMT_YUV420SP)
            throw std::runtime_error("Camera khong tra ve NV12; kiem tra RKMPI pixel format");
        if (static_cast<int>(frame.stVFrame.u32Width) != width_ ||
            static_cast<int>(frame.stVFrame.u32Height) != height_)
            throw std::runtime_error("Kich thuoc frame khac VI config");
        const auto* data = static_cast<const std::uint8_t*>(
            RK_MPI_MB_Handle2VirAddr(frame.stVFrame.pMbBlk));
        const int stride = static_cast<int>(frame.stVFrame.u32VirWidth);
        const int virtual_h = static_cast<int>(frame.stVFrame.u32VirHeight);
        // Sao chep YUV -> RGB TRUOC khi release frame, tranh dung DMA pointer cu.
        output = nv12_to_rgb(data, width_, height_, stride, virtual_h);
    } catch (...) {
        RK_MPI_VI_ReleaseChnFrame(0, channel_, &frame);
        throw;
    }
    mpi_check(RK_MPI_VI_ReleaseChnFrame(0, channel_, &frame), "VI ReleaseChnFrame");
    return true;
}

// RV1106 shutdown: when Ctrl+C is received, main exits its loop and the
// RkmpiCamera destructor calls close(). On some LuckFox ISP3.x SDK versions
// RK_MPI_VI_DisableChn() can block inside ispStreamOff while RKAIQ is running.
// LuckFox forum recommends stopping ISP before VI for repeated stop/start.
// Keep close idempotent, and log before / after EACH blocking SDK call.
void RkmpiCamera::close() noexcept {
    if (!isp_started_ && !channel_enabled_ && !dev_enabled_ && !sys_started_)
        return;

    std::fprintf(stderr, "[SHUTDOWN] Begin camera cleanup (ISP -> VI CHN -> VI DEV -> MPI SYS)\n");
    std::fflush(stderr);

    if (isp_started_) {
        std::fprintf(stderr, "[SHUTDOWN] Stopping RKAIQ ISP...\n");
        std::fflush(stderr);
        SAMPLE_COMM_ISP_Stop(0);
        isp_started_ = false;
        std::fprintf(stderr, "[SHUTDOWN] RKAIQ ISP stopped\n");
        std::fflush(stderr);
    }

    if (channel_enabled_) {
        std::fprintf(stderr, "[SHUTDOWN] Disabling VI channel %d...\n", channel_);
        std::fflush(stderr);
        const RK_S32 rc = RK_MPI_VI_DisableChn(0, channel_);
        channel_enabled_ = false;
        std::fprintf(stderr, "[SHUTDOWN] VI channel disable returned %d\n", static_cast<int>(rc));
        std::fflush(stderr);
    }

    if (dev_enabled_) {
        std::fprintf(stderr, "[SHUTDOWN] Disabling VI device 0...\n");
        std::fflush(stderr);
        const RK_S32 rc = RK_MPI_VI_DisableDev(0);
        dev_enabled_ = false;
        std::fprintf(stderr, "[SHUTDOWN] VI device disable returned %d\n", static_cast<int>(rc));
        std::fflush(stderr);
    }

    if (sys_started_) {
        std::fprintf(stderr, "[SHUTDOWN] Exiting RK_MPI SYS...\n");
        std::fflush(stderr);
        const RK_S32 rc = RK_MPI_SYS_Exit();
        sys_started_ = false;
        std::fprintf(stderr, "[SHUTDOWN] RK_MPI SYS exit returned %d\n", static_cast<int>(rc));
        std::fflush(stderr);
    }

    std::fprintf(stderr, "[SHUTDOWN] Camera cleanup finished\n");
    std::fflush(stderr);
}