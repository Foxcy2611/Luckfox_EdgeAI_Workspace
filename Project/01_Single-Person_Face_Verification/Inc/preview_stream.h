#pragma once

#include "image.h"
#include "face_detector.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct FaceResult {
    FaceDetection face;
    float similarity = 0.0f;
    bool verified = false;
    bool detect_only = false;
};

// Asynchronous TCP preview. Only the newest frame is retained: slow viewers
// cannot stall the RK_MPI / NPU processing thread or accumulate old frames.
class PreviewServer {
public:
    PreviewServer() = default;
    PreviewServer(const PreviewServer&) = delete;
    PreviewServer& operator=(const PreviewServer&) = delete;
    ~PreviewServer();

    void start(int port);
    void publish(const Image& rgb, const std::vector<FaceResult>& faces, std::size_t index);

private:
    struct Packet {
        std::string header;
        std::vector<std::uint8_t> pixels;
    };

    void worker_loop() noexcept;
    void disconnect() noexcept; // worker thread only, except after join

    int listen_fd_ = -1;
    int client_fd_ = -1;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> connected_{false};
    bool pending_ = false;
    Packet newest_;
};
