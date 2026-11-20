#include "preview_stream.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>

namespace {
// Preview transport only; the camera and both RKNN models keep full resolution.
constexpr int kPreviewMaxWidth = 320;
constexpr int kPreviewMaxHeight = 240;
constexpr int kMaxSendMs = 250; // fail fast on a congested TCP viewer

bool send_before(int fd, const void* data, std::size_t count,
                 std::chrono::steady_clock::time_point deadline) {
    const auto* ptr = static_cast<const unsigned char*>(data);
    while (count > 0) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        const ssize_t n = ::send(fd, ptr, count, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (n > 0) {
            ptr += n;
            count -= static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) return false;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) return false;
        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLOUT;
        const int result = ::poll(&pfd, 1, static_cast<int>(remaining));
        if (result <= 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) return false;
    }
    return true;
}

int preview_width(int w, int h) {
    const double scale = std::min({1.0,
                                   static_cast<double>(kPreviewMaxWidth) / w,
                                   static_cast<double>(kPreviewMaxHeight) / h});
    return std::max(1, static_cast<int>(std::lround(w * scale)));
}
int preview_height(int w, int h) {
    const double scale = std::min({1.0,
                                   static_cast<double>(kPreviewMaxWidth) / w,
                                   static_cast<double>(kPreviewMaxHeight) / h});
    return std::max(1, static_cast<int>(std::lround(h * scale)));
}
} // namespace

PreviewServer::~PreviewServer() {
    stopping_.store(true);
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    disconnect();
    if (listen_fd_ >= 0) ::close(listen_fd_);
}

void PreviewServer::disconnect() noexcept {
    connected_.store(false);
    if (client_fd_ >= 0) ::close(client_fd_);
    client_fd_ = -1;
    std::lock_guard<std::mutex> lock(mutex_);
    newest_ = {};
    pending_ = false;
}

void PreviewServer::start(int port) {
    if (port <= 0 || port > 65535 || listen_fd_ >= 0)
        throw std::runtime_error("Preview port invalid/already open");
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("Preview socket failed");
    int yes = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        ::listen(listen_fd_, 1) < 0)
        throw std::runtime_error("Preview bind/listen failed (port in use?)");
    const int flags = ::fcntl(listen_fd_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("Preview fcntl failed");
    stopping_.store(false);
    worker_ = std::thread(&PreviewServer::worker_loop, this);
    std::cerr << "[PREVIEW] FVP2, asynchronous latest-frame TCP, port=" << port
              << ", max " << kPreviewMaxWidth << 'x' << kPreviewMaxHeight
              << " (NOT RTSP)\n";
}

void PreviewServer::publish(const Image& rgb,
                            const std::vector<FaceResult>& faces, std::size_t frame_index) {
    // No viewer: skip image conversion/copy altogether; inference never blocks.
    if (!connected_.load() || !rgb.valid()) return;

    const int pw = preview_width(rgb.width, rgb.height);
    const int ph = preview_height(rgb.width, rgb.height);
    const float sx = static_cast<float>(pw) / rgb.width;
    const float sy = static_cast<float>(ph) / rgb.height;
    Packet packet;
    packet.pixels.resize(static_cast<std::size_t>(pw) * ph * 3);
    // Cheap nearest-neighbor sampling; no dependency on OpenCV on the board.
    for (int y = 0; y < ph; ++y) {
        const int src_y = std::min(rgb.height - 1, y * rgb.height / ph);
        for (int x = 0; x < pw; ++x) {
            const int src_x = std::min(rgb.width - 1, x * rgb.width / pw);
            const std::size_t src = (static_cast<std::size_t>(src_y) * rgb.width + src_x) * 3;
            const std::size_t dst = (static_cast<std::size_t>(y) * pw + x) * 3;
            std::memcpy(packet.pixels.data() + dst, rgb.rgb.data() + src, 3);
        }
    }

    std::ostringstream line;
    line << "FVP2 " << pw << ' ' << ph << ' ' << frame_index << ' ' << faces.size()
         << std::fixed << std::setprecision(4);
    for (const auto& result : faces) {
        const auto& f = result.face;
        line << ' ' << f.x * sx << ' ' << f.y * sy
             << ' ' << f.w * sx << ' ' << f.h * sy
             << ' ' << f.score << ' ' << result.similarity << ' '
             << (result.detect_only ? 2 : (result.verified ? 1 : 0));
        for (const auto& p : f.landmarks)
            line << ' ' << p.x * sx << ' ' << p.y * sy;
    }
    line << '\n';
    packet.header = line.str();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_.load()) return; // viewer might have disconnected meanwhile
        newest_ = std::move(packet); // replace any frame not sent yet
        pending_ = true;
    }
    cv_.notify_one();
}

void PreviewServer::worker_loop() noexcept {
    using namespace std::chrono;
    while (!stopping_.load()) {
        if (client_fd_ < 0) {
            const int accepted = ::accept(listen_fd_, nullptr, nullptr);
            if (accepted < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                    std::cerr << "[PREVIEW] accept failed errno=" << errno << '\n';
                std::this_thread::sleep_for(milliseconds(20));
                continue;
            }
            client_fd_ = accepted;
            int yes = 1;
            ::setsockopt(client_fd_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
            // Small kernel queue so old frames cannot pile up for many seconds.
            int buffer_size = 64 * 1024;
            ::setsockopt(client_fd_, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
            connected_.store(true);
            std::cerr << "[PREVIEW] viewer connected, latest-only sending\n";
        }

        Packet packet;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, milliseconds(20), [this] {
                return stopping_.load() || pending_;
            });
            if (stopping_.load()) break;
            if (!pending_) continue;
            packet = std::move(newest_);
            pending_ = false;
        }
        const auto deadline = steady_clock::now() + milliseconds(kMaxSendMs);
        if (!send_before(client_fd_, packet.header.data(), packet.header.size(), deadline) ||
            !send_before(client_fd_, packet.pixels.data(), packet.pixels.size(), deadline)) {
            std::cerr << "[PREVIEW] viewer too slow/disconnected; drop connection (AI keeps running)\n";
            disconnect();
        }
    }
}
