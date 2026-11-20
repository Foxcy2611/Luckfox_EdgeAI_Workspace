# 🧩 Thư viện C++ & Cross-Compile trên LuckFox

> **Phạm vi:** LuckFox Pico Max (RV1106), Buildroot/uClibc, Ubuntu Host.  

## 📑 Mục lục

- [1. Ba bước: include, compile và link](#1-ba-bước-include-compile-và-link)
- [2. Cách A — Thư viện đã cross compile sẵn](#2-cách-a--thư-viện-đã-cross-compile-sẵn)
- [3. Cách B — Thư viện vẫn ở dạng source](#3-cách-b--thư-viện-vẫn-ở-dạng-source)
- [4. OpenCV, YOLO và RKNN khác nhau thế nào?](#4-opencv-yolo-và-rknn-khác-nhau-thế-nào)
- [5. Chạy thử trên LuckFox](#5-chạy-thử-trên-luckfox)
- [6. Những lỗi dễ nhầm](#6-những-lỗi-dễ-nhầm)
- [7. Nguồn tham khảo](#7-nguồn-tham-khảo)

## 1. Ba bước: include, compile và link

```text
main.cc                         OpenCV headers (.hpp)
   └────── #include ────────────────┘
                │
                ▼
       cross compile mã nguồn ứng dụng (ARM object files)
                │
                ▼
       Link với thư viện OpenCV (.a/.so cho ARM)
                │
                ▼
        app (ELF chạy trên LuckFox)
```

| Khái niệm | Vai trò |
|---|---|
| `#include <opencv2/opencv.hpp>` | Đọc **khai báo** hàm, class và kiểu dữ liệu, **không** tự biên dịch hoặc cài OpenCV. |
| Compile | Dùng compiler dành cho CPU đích để biến source thành object code. |
| Link | Ghép các object code và thư viện thành file chạy, phát hiện hàm nào chưa có phần định nghĩa. |
| `.a` / `.so` | Thư viện đã biên dịch, `.a` thường liên kết tĩnh, `.so` là shared library dùng tại runtime. |
| CMake + Ninja | Quản lý cấu hình, thứ tự build, đường dẫn header/library và lệnh gọi toolchain. |

**Lưu ý:** Header-only library là trường hợp đặc biệt có thể không cần link một binary thư viện riêng. OpenCV không phải trường hợp đó.

## 2. Cách A — Thư viện đã cross compile sẵn

Đây là cách nên ưu tiên lúc bắt đầu với **OpenCV-Mobile cho LuckFox**.

```text
opencv-mobile (headers + .a/.so tương thích LuckFox)
                      │
main.cc (chưa cross compile) ────┤
                      ▼
       CMake + SDK ARM toolchain
           │            │
    cross compile main.cc       Link OpenCV
           └──────┬─────┘
                  ▼
                 app
```

C++ **có thể `#include` ngay khi viết** dù chính `main.cc` chưa cross compile. Khi build, toolchain cross compile **main.cc**, rồi linker dùng **OpenCV đã được cross compile trước**, không cross compile lại OpenCV.

Ví dụ `src/main.cc`:

```cpp
#include <opencv2/opencv.hpp>

int main() {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar(0, 0, 0)),
    cv::Mat small,
    cv::resize(img, small, cv::Size(224, 224)),
    return small.empty() ? 1 : 0,
}
```

Ví dụ `CMakeLists.txt` (project C++ riêng):

```cmake
cmake_minimum_required(VERSION 3.16)
project(luckfox_opencv_demo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# OpenCV_DIR trỏ đến lib/cmake/opencv4 của gói OpenCV-Mobile
find_package(OpenCV REQUIRED CONFIG)

add_executable(luckfox_opencv_demo src/main.cc)
target_include_directories(luckfox_opencv_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(luckfox_opencv_demo PRIVATE ${OpenCV_LIBS})
```

cross compile trên Ubuntu với **toolchain file CMake dành cho LuckFox**. Ví dụ tạo `toolchains/luckfox-uclibc.cmake` ngay trong project:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(SDK "$ENV{HOME}/luckfox-pico")
set(TC "${SDK}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin")
set(CMAKE_C_COMPILER "${TC}/arm-rockchip830-linux-uclibcgnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${TC}/arm-rockchip830-linux-uclibcgnueabihf-g++")
```

Đây là cấu hình compiler tối thiểu theo SDK Pico, nếu project cần sysroot hoặc tùy chọn link riêng, phải bổ sung theo cấu hình SDK/firmware. Lúc cấu hình CMake, đường dẫn là:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/luckfox-uclibc.cmake \
  -DOpenCV_DIR=/path/to/opencv-mobile/lib/cmake/opencv4
cmake --build build
file build/luckfox_opencv_demo
```

> `luckfox-toolchain.cmake` phải khai báo compiler ARM/uClibc, sysroot/các tùy chọn cần thiết theo SDK. Không lấy compiler OpenCV của laptop x86_64 để link ứng dụng ARM. Gói OpenCV-Mobile phải tương thích SDK, ABI và libc của firmware đang chạy.

## 3. Cách B — Thư viện vẫn ở dạng source

Nếu OpenCV-Mobile thiếu module/API cần dùng, có thể clone OpenCV chính thức và **tự cross compile bản OpenCV phù hợp LuckFox**.

```text
OpenCV source (GitHub)       main.cc (project của ta)
           │                         │
           ▼                         ▼
 cross compile thư viện bằng ARM       cross compile ứng dụng bằng ARM
       toolchain                   toolchain
           │                         │
           └────────── Link ─────────┘
                       │
                       ▼
                   app cho ARM
```

Hai lựa chọn tổ chức build:

1. **Hai lần build rõ ràng (dễ debug):** cross compile OpenCV từ source → install vào một prefix ARM riêng → project `find_package(OpenCV)` → cross compile ứng dụng và link thư viện đã build.
2. **Một quy trình build được điều phối:** CMake cấu hình OpenCV như dependency (ví dụ `ExternalProject_Add` hoặc cơ chế build thích hợp) và bảo đảm OpenCV được build trước ứng dụng. **Chỉ `#include` không tự kích hoạt việc cross compile source OpenCV.**

Ví dụ hướng cấu hình OpenCV từ source (minh họa, **chưa phải lệnh đã kiểm thử trên board**):

```bash
# Source và SDK được giữ ngoài repo workspace
git clone https://github.com/opencv/opencv.git "$HOME/opencv-src"

cmake -S "$HOME/opencv-src" -B "$HOME/opencv-arm-build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/luckfox-toolchain.cmake \
  -DCMAKE_INSTALL_PREFIX="$HOME/opencv-arm-install" \
  -DBUILD_LIST=core,imgproc,imgcodecs \
  -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF \
  -DBUILD_EXAMPLES=OFF

cmake --build "$HOME/opencv-arm-build"
cmake --install "$HOME/opencv-arm-build"
```

Sau đó trỏ `OpenCV_DIR` của project C++ tới thư mục `lib/cmake/opencv4` **được tạo trong prefix install** (vị trí thực tế tùy build).

> OpenCV đầy đủ có dependency phức tạp, cần chọn module, cấu hình sysroot, codec và thư viện đúng với firmware. Không nhất thiết phải xây toàn bộ OpenCV cho board RAM hạn chế. Bản `.so` build được trên host vẫn có thể thiếu dependency trên target.

## 4. OpenCV, YOLO và RKNN khác nhau thế nào?

| Thành phần | C++ làm việc như thế nào? |
|---|---|
| **OpenCV / OpenCV-Mobile** | `#include` API xử lý ảnh, cross compile ứng dụng và link đúng thư viện OpenCV. |
| **RKMPI / RGA** | Dùng API multimedia/hardware của Rockchip (headers + thư viện tương thích SDK). |
| **RKNN Runtime** | `#include "rknn_api.h"`, link thư viện RKNN, nạp `model.rknn`, gọi inference và đọc tensor. |
| **YOLO** | Là kiến trúc model/dòng thuật toán, **không bắt buộc có thư viện `yolo`**. C++ nạp `yolo.rknn` qua RKNN và chạy code tiền/hậu xử lý phù hợp model (NMS, boxes...). |

```text
Camera / ảnh tĩnh
       ↓ RKMPI hoặc đọc ảnh
OpenCV / RGA: resize, đổi màu, chuẩn hóa
       ↓
RKNN Runtime → NPU chạy model.rknn
       ↓
C++ hậu xử lý YOLO / classification / face embedding
       ↓
UART / log / RTSP / MQTT
```

**Python trên Ubuntu:** chủ yếu chuẩn bị dataset, train, export/convert/quantize model. **C++ trên LuckFox:** chạy inference với runtime đã cross compile tương thích, không cần TensorFlow đầy đủ trên target.

## 5. Chạy thử trên LuckFox

Sau khi có `app` là binary ARM và các thư viện phù hợp, ưu tiên **SCP trước, đóng gói image sau**:

```bash
# Trên Ubuntu, app là tên chương trình đã build
scp build/luckfox_opencv_demo root@172.32.0.93:/root/
ssh root@172.32.0.93
```

Trên LuckFox:

```sh
chmod +x /root/luckfox_opencv_demo
/root/luckfox_opencv_demo
```

Kiểm tra file trước khi chuyển (Ubuntu):

```bash
file build/luckfox_opencv_demo
readelf -h build/luckfox_opencv_demo | grep -E 'Class:|Machine:'
readelf -d build/luckfox_opencv_demo | grep NEEDED || true
```

Nếu dùng **shared libraries (`.so`)**, LuckFox cần có đủ phiên bản `.so` và các dependency liên quan, có thể chuyển gói runtime phù hợp theo hướng dẫn demo, hoặc đóng gói vào image lúc release. Library static `.a` không cần copy sang target chỉ để chạy ứng dụng đã link tĩnh, nhưng binary vẫn có thể phụ thuộc các `.so` khác.

## 6. Những lỗi dễ nhầm

- `#include` **không** tự cài, tự cross compile hay link một thư viện.
- `.hpp` và `.a/.so` có vai trò khác nhau, thiếu header có thể lỗi compile, thiếu library có thể lỗi link/runtime.
- OpenCV cài qua `apt`/`pip` cho **Ubuntu host** không tự động dùng được cho **LuckFox ARM/uClibc**.
- Toolchain của SDK phải tương thích **kiến trúc CPU, libc, ABI và firmware target**.
- Có thể cross compile ứng dụng cùng source thư viện trong một build **khi build system được cấu hình rõ**, không phải compiler tự đọc mọi file trên GitHub.
- **Không cần build/nạp lại Linux image mỗi lần sửa `main.cc`**: cross compile lại app, SCP sang board và chạy thử.

**Tóm tắt:**

```text
Cách A: Lib đã cross compile → #include + cross compile main.cc + link → app
Cách B: Lib source  → cross compile lib + cross compile main.cc + link → app

Cả hai cách đều cần tạo app ARM cuối cùng.
```

## 7. Nguồn tham khảo

- [LuckFox — OpenCV-Mobile](https://wiki.luckfox.com/Luckfox-Pico-RV1106/opencv-mobile/)
- [LuckFox — RKMPI Examples](https://github.com/LuckfoxTECH/luckfox_pico_rkmpi_example)
- [LuckFox — RKNN Examples](https://github.com/LuckfoxTECH/luckfox_pico_rknn_example)
- [OpenCV — Build from Source](https://docs.opencv.org/5.0/tutorials/introduction/general_install/general_install.html)
- [CMake — Cross Compiling](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling)
