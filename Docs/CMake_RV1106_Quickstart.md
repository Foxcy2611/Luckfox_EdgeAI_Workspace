# CMake + Ninja cho LuckFox RV1106 — ghi chú ngắn

## 1. Ba thành phần cần có để cross-compile

| Thành phần | Dùng để làm gì? | Đường dẫn đã tìm được trên Ubuntu |
|---|---|---|
| **Cross-compiler** | Biên dịch C/C++ trên Ubuntu x86_64 thành binary ARM/uClibc | `~/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/` |
| **Header (`rknn_api.h`)** | Khai báo kiểu dữ liệu và các hàm RKNN (`rknn_init`, `rknn_run`, ...) | `~/luckfox-pico/project/app/rk_smart_door/smart_door/common/face/algo/rknn_api.h` |
| **Library (`librknnmrt.so`)** | Liên kết các hàm RKNN Runtime cho ARM | `~/luckfox-pico/output/out/rootfs_uclibc_rv1106/oem/usr/lib/librknnmrt.so` |

- Compiler C: `arm-rockchip830-linux-uclibcgnueabihf-gcc`.
- Compiler C++: `arm-rockchip830-linux-uclibcgnueabihf-g++`.
- Trên **board**, runtime nằm tại `/oem/usr/lib/librknnmrt.so` và đã xác nhận **phiên bản 1.6.0**. Bản `.so` trong SDK cần kiểm tra thêm phiên bản/kiến trúc trước khi liên kết.
- **RKNN-Toolkit2** trong `.venv-rknn` dùng để chuyển `ONNX → .rknn`, **không** phải ARM compiler hoặc RKNN Runtime cho C++.

## 2. Vị trí project và 3 file cấu hình

```text
/home/chien/
├── luckfox-pico/                       # SDK độc lập
└── Luckfox_EdgeAI_Workspace/
    ├── Docs/
    │   └── CMake_RV1106_Quickstart.md  # Tài liệu này
    └── Project/
        └── 01_Single-Person_Face_Verification/
            ├── CMakeLists.txt          # Build file nào, link library nào?
            ├── CMakePresets.json        # Ninja, thư mục build, biến SDK
            ├── CMake/
            │   └── toolchain-rv1106.cmake  # ARM GCC/G++
            ├── Inc/                    # Header tự viết
            └── Src/                    # Các file .cpp/.c
                └── main.cpp
```

| File | Nhiệm vụ |
|---|---|
| `CMakeLists.txt` | Khai báo target (`face_verify` hoặc `${PROJECT_NAME}`), các `.cpp`, `Inc/`, `rknn_api.h` và `librknnmrt.so`. |
| `CMake/toolchain-rv1106.cmake` | Chọn Linux ARM và đường dẫn `gcc`/`g++` trong SDK. |
| **`CMakePresets.json`** | Lưu sẵn **Ninja**, `build/`, `Release`, đường dẫn SDK/toolchain và các giá trị `-D`; giúp build bằng 2 lệnh ngắn. |

> Linux phân biệt chữ hoa/thường: ở project hiện tại thư mục là `CMake/` (chữ **C** hoa), không phải `cmake/`. Không cần đặt project vào trong SDK.

## 3. Lệnh `find` tìm 3 thành phần

Chạy **trên Ubuntu**, không phải trong SSH LuckFox để tìm 3 file sau:
- `rknn_api.h`: Khai báo các hàm giao tiếp với NPU LuckFox
- `arm...-g++`: Biên dịch C++ thành chương trình ARM
- `librknnmrt.so`: Cung cấp phần triển khai các hàm RKNN

```bash
find ~/luckfox-pico -type f \
  \( -name 'rknn_api.h' \ 
  -o -name 'librknnmrt.so' \
  -o -name 'arm-rockchip830-linux-uclibcgnueabihf-g++' \) \
  2>/dev/null
```

Các đường dẫn đã chọn nằm ở **mục 1**. Vì SDK có **nhiều** `librknnmrt.so`, kiểm tra file định dùng:

```bash
SDK="$HOME/luckfox-pico"
RKNN_SO="$SDK/output/out/rootfs_uclibc_rv1106/oem/usr/lib/librknnmrt.so"
file "$RKNN_SO"                          # Phải là ELF ARM, không phải x86_64
strings "$RKNN_SO" | grep -m1 'librknnmrt version'
```

Trường hợp cần lấy bản runtime đúng từ board:

```bash
# Chạy trên Ubuntu (không cần nếu bản trong SDK đã đúng)
scp root@172.32.0.93:/oem/usr/lib/librknnmrt.so ./librknnmrt.so
```

## 4. Cách viết CMake đơn giản

**`CMake/toolchain-rv1106.cmake`** — chỉ định nền tảng đích và compiler:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(_BIN "$ENV{LUCKFOX_SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin")
set(CMAKE_C_COMPILER "${_BIN}/arm-rockchip830-linux-uclibcgnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${_BIN}/arm-rockchip830-linux-uclibcgnueabihf-g++")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

**`CMakeLists.txt`** — khai báo source, header và library:

```cmake
cmake_minimum_required(VERSION 3.16)
project(FaceVerification LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(face_verify Src/main.cpp)
target_include_directories(face_verify PRIVATE Inc "${RKNN_INCLUDE_DIR}")
target_link_libraries(face_verify PRIVATE "${RKNN_LIBRARY}")
```

**`CMakePresets.json`** — ghi nhớ cấu hình, không phải trình biên dịch:

```json
{
  "version": 3,
  "configurePresets": [{
    "name": "rv1106",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/build",
    "toolchainFile": "${sourceDir}/CMake/toolchain-rv1106.cmake",
    "environment": {
      "LUCKFOX_SDK_PATH": "$penv{HOME}/luckfox-pico"
    },
    "cacheVariables": {
      "CMAKE_BUILD_TYPE": "Release",
      "RKNN_INCLUDE_DIR": "$env{LUCKFOX_SDK_PATH}/project/app/rk_smart_door/smart_door/common/face/algo",
      "RKNN_LIBRARY": "$env{LUCKFOX_SDK_PATH}/output/out/rootfs_uclibc_rv1106/oem/usr/lib/librknnmrt.so"
    }
  }],
  "buildPresets": [{
    "name": "rv1106",
    "configurePreset": "rv1106"
  }]
}
```

- `generator: Ninja` = CMake sinh `build.ninja`; **Ninja thực hiện build**, còn ARM GCC/G++ mới biên dịch.
- `LUCKFOX_SDK_PATH` = tự trỏ đến `~/luckfox-pico`, không cần `export` mỗi lần.
- `cacheVariables` = truyền sẵn các giá trị tương đương `-D...` cho `CMakeLists.txt`.
- Mẫu ngắn ở trên để hiểu nguyên lý; giữ các phần **kiểm tra đường dẫn/báo lỗi** đã viết trong file CMake thực tế. Thêm `.cpp` mới vào `add_executable(...)` khi triển khai.

## 5. Build chỉ bằng 2 lệnh

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification
cmake --preset FaceVerification # Set vị trí CMake mặc định ví dụ cho prj 01
# Build
cmake --build --preset FaceVerification
```

Kiểm tra file sau khi build:

```bash
file build/face_verify
```

Nếu từng cấu hình `build/` bằng cache/đường dẫn cũ, có thể xóa **riêng** thư mục build rồi chạy lại 2 lệnh:

```bash
rm -rf build
```

> Cần có `Src/main.cpp` hợp lệ và đúng `.so` ARM để build thành công. File ARM sinh ra chạy trên LuckFox, **không chạy trực tiếp trên Ubuntu x86_64**.

## 6. Ghi nhớ và tài liệu liên quan

- **CMake** tổ chức build → **Ninja** thực thi → **ARM GCC/G++** compile/link → binary chạy trên **LuckFox**.
- File `.h` được `#include` vào `.c/.cpp`, không biên dịch độc lập.
- Giai đoạn đầu chỉ link RKNN; khi thêm camera/RGA/OpenCV mới bổ sung những library cần thiết.
- [LuckFox — hướng dẫn cross-compiler](https://wiki.luckfox.com/Luckfox-Pico-Plus-Mini/Cross-Compile/)
- [LuckFox SDK — repository chứa toolchain](https://github.com/LuckfoxTECH/luckfox-pico)