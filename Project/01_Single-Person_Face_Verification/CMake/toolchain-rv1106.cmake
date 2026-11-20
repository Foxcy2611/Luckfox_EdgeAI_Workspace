# => Sử dụng Compiler nào ?

# Cross-compile C/C++ cho Luckfox RV1106 (Buildroot / uClibc).
# Khi configure, truyền: export LUCKFOX_SDK_PATH="$HOME/luckfox-pico"

set(CMAKE_SYSTEM_NAME Linux)    # Hệ điều hành mà chương trình chạy = LINUX
set(CMAKE_SYSTEM_PROCESSOR arm) # Kiến trúc CPU đích: ARM

# ENV{}: Đọc biến môi trường
if(NOT DEFINED ENV{LUCKFOX_SDK_PATH} OR "$ENV{LUCKFOX_SDK_PATH}" STREQUAL "")
# if(biến môi trường chưa đc khai báo || Biến tồn tại nhưng không có giá trị
    message(FATAL_ERROR "Can export LUCKFOX_SDK_PATH=/duong/dan/luckfox-pico")
endif()

# Set link môi trường cho 2 biến
# CROSS BIN: Thư mục chứa các compiler
# CROSS PREFIX: Tiền tố tên bộ công cụ biên dịch ARM
set(_CROSS_BIN "$ENV{LUCKFOX_SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin")
set(_CROSS_PREFIX "arm-rockchip830-linux-uclibcgnueabihf")

# Set link cho biên dịch C và C++
set(CMAKE_C_COMPILER "${_CROSS_BIN}/${_CROSS_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${_CROSS_BIN}/${_CROSS_PREFIX}-g++")

# Kiểm tra Compiler có tồn tại ?
if(NOT EXISTS "${CMAKE_C_COMPILER}" OR NOT EXISTS "${CMAKE_CXX_COMPILER}")
    message(FATAL_ERROR "Khong tim thay cross-compiler trong: ${_CROSS_BIN}")
endif()

# Yêu cầu CMake chỉ biên dịch mã nguồn thành thư viện tĩnh (static library) 
# thay vì tạo file thực thi (executable) trong giai đoạn kiểm tra trình biên dịch (try_compile).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)