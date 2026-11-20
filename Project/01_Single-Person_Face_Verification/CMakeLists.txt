#[[
- CMakeLists: Cần biên dịch những gì?
- CMakePresets: Biên dịch với cấu hình nào?
- Toolchain: Dùng compiler nào, biên dịch cho kiến trúc nào?
]]


cmake_minimum_required(VERSION 3.16)        # Bản Cmake min là 3.16
project(FaceVerification LANGUAGES C CXX)   # Khai báo tên chương trình và ngôn ngữ lựa chọn

set(CMAKE_C_STANDARD 11)            # Chuẩn C 11
set(CMAKE_CXX_STANDARD 17)          # Chuẩn C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Yêu cầu compiler hỗ trợ C++17

# CMakePresets.json cung cấp LUCKFOX_SDK_PATH => SDK nằm NGOÀI workspace.
# $ENV{}: Dùng để đọc biến môi trường
set(SDK_PATH "$ENV{LUCKFOX_SDK_PATH}")
if(NOT IS_DIRECTORY "${SDK_PATH}")
    message(FATAL_ERROR "LUCKFOX_SDK_PATH khong hop le. Kiem tra CMakePresets.json va chay: cmake --preset FaceVerification")
endif()

# RKNN Runtime 1.6.0, ARM/uClibc.
set(RKNN_INCLUDE_DIR    # Chỉ đến file chứa: rknn_api.h chứa các API chạy suy luận
    "${SDK_PATH}/project/app/rk_smart_door/smart_door/common/face/algo")
set(RKNN_LIBRARY        # Chỉ đến file librknnmrt.so chứa RKNN Runtime thông qua driver và NPU
    "${SDK_PATH}/output/out/rootfs_uclibc_rv1106/oem/usr/lib/librknnmrt.so")

# LƯU Ý: Không dùng `find` tự động cho sample_comm.h.
# Thư mục media/out/rkisp_demo/demo/sample có một sample_comm.h KHÁC,
# trong khi libsample_comm.a của project này đến từ media/samples/example/common.

# Khai báo ISP và RKAIQ
set(RKMPI_INCLUDE_DIR           # Cấu hình camera RK_MPI là giao diện để tương tác vs thành phần media của Rockchip
    "${SDK_PATH}/media/rockit/rockit/mpi/sdk/include")
set(SAMPLE_COMM_INCLUDE_DIR     # Set đường daaxann đến sample_comm.h,
    "${SDK_PATH}/media/samples/example/common")
set(SAMPLE_COMM_ISP_INCLUDE_DIR # ISP phụ trách những công đoạn xử lý tín hiệu hình ảnh từ cảm biến
    "${SDK_PATH}/media/samples/example/common/isp3.x")
set(SAMPLE_COMM_LIBRARY         # Bộ helper từ SDK, giúp đơn giản hóa việc khởi tạo và điều khiển media
    "${SDK_PATH}/media/samples/example/common/lib/libsample_comm.a")

# RKAIQ là bộ lib và API điều khiển ISP của ROCKCHIP
set(RKAIQ_ROOT "${SDK_PATH}/media/out/include/rkaiq")
set(RKAIQ_INCLUDE_DIRS
    "${RKAIQ_ROOT}"
    "${RKAIQ_ROOT}/uAPI2"
    "${RKAIQ_ROOT}/common"
    "${RKAIQ_ROOT}/xcore"
    "${RKAIQ_ROOT}/algos"
    "${RKAIQ_ROOT}/iq_parser"
    "${RKAIQ_ROOT}/iq_parser_v2"
    "${RKAIQ_ROOT}/smartIr"
    "${RKAIQ_ROOT}/isp"
)

# Kiểm tra bộ header và static lib NHẤT QUÁN, báo lỗi ngay khi configure.
foreach(_FILE
    "${RKNN_INCLUDE_DIR}/rknn_api.h"
    "${RKNN_LIBRARY}"
    "${RKMPI_INCLUDE_DIR}/rk_mpi_vi.h"
    "${SAMPLE_COMM_INCLUDE_DIR}/sample_comm.h"
    "${SAMPLE_COMM_ISP_INCLUDE_DIR}/sample_comm_isp.h"
    "${SAMPLE_COMM_LIBRARY}")
    if(NOT EXISTS "${_FILE}")
        message(FATAL_ERROR "SDK thieu: ${_FILE}")
    endif()
endforeach()
if(NOT EXISTS "${RKAIQ_ROOT}/algos/a3dlut/rk_aiq_uapi_a3dlut_int.h")
    message(WARNING "Khong thay a3dlut header o duong dan du kien. Kiem tra bang find SDK/media.")
endif()

# Ưu tiên lib trong rootfs ARM/uClibc dùng cho firmware đang chạy.
function(sdk_find_library OUT LIBNAME)
    foreach(base
        "${SDK_PATH}/output/out/rootfs_uclibc_rv1106/oem/usr/lib"
        "${SDK_PATH}/output/out/rootfs_uclibc_rv1106/usr/lib"
        "${SDK_PATH}/media/out/lib"
        "${SDK_PATH}/media/isp/out/lib")
        foreach(ext .so .a)
            if(EXISTS "${base}/lib${LIBNAME}${ext}")
                set(${OUT} "${base}/lib${LIBNAME}${ext}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()
    message(FATAL_ERROR "Khong tim thay lib${LIBNAME} trong cac thu muc SDK cho phep")
endfunction()

sdk_find_library(ROCKIT_LIBRARY rockit)
sdk_find_library(RKAIQ_LIBRARY rkaiq)
sdk_find_library(ROCKCHIP_MPP_LIBRARY rockchip_mpp)

# Core C++ pipeline: SC3336 -> YuNet RKNN -> SFace RKNN -> verification.
# classifier.cpp (old sigmoid model) is intentionally excluded.
set(FACE_VERIFICATION_SOURCES
    Src/main.cpp            # Luồng chính
    Src/image.cpp           # Biểu diễn và chuyển đổi ảnh
    Src/preprocess.cpp      # Resize
    Src/nv12.cpp            # Biểu diễn và chuyển đổi ảnh
    Src/camera.cpp          # Nhận dữ liệu ảnh
    Src/camera_rkmpi.cpp    # Nhận dữ liệu ảnh
    Src/rknn_engine.cpp     # Inference NPU
    Src/face_detector.cpp   # Xử lý output YuNet
    Src/face_alignment.cpp  # Căn chỉnh khuôn mặt
    Src/face_verifier.cpp   # Cosine similarity
    Src/preview_stream.cpp  # Truyền kết quả về Ubuntu
)

# Kiểm tra source
foreach(_SOURCE IN LISTS FACE_VERIFICATION_SOURCES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_SOURCE}")
        message(FATAL_ERROR "Thieu source ${_SOURCE}. Hay chep du bo Src/ va Inc/ cua patch C++.")
    endif()
endforeach()

# Tạo executable: Tạo một executable từ toàn bộ source đã khai báo.
add_executable(${PROJECT_NAME} ${FACE_VERIFICATION_SOURCES})

# Khai báo macro cho compiler
target_compile_definitions(${PROJECT_NAME} PRIVATE
    RV1106_1103 
    ISP_HW_V30 
    RKPLATFORM=ON 
    UAPI2
    _LARGEFILE_SOURCE 
    _LARGEFILE64_SOURCE 
    _FILE_OFFSET_BITS=64
)

# Khai báo thư mục header
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Inc"
    "${RKNN_INCLUDE_DIR}"
    "${RKMPI_INCLUDE_DIR}"
    "${SAMPLE_COMM_INCLUDE_DIR}"
    "${SAMPLE_COMM_ISP_INCLUDE_DIR}"
    "${SDK_PATH}/media/out/include"
    ${RKAIQ_INCLUDE_DIRS}
)

# Liên kết thư viện
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${RKNN_LIBRARY}"
    "${SAMPLE_COMM_LIBRARY}"
    "${ROCKIT_LIBRARY}"
    "${ROCKCHIP_MPP_LIBRARY}"
    "${RKAIQ_LIBRARY}"
    pthread rt dl m
)

# Cấu hình đường dẫn cho linker
get_filename_component(
    ROCKIT_LIB_DIR 
    "${ROCKIT_LIBRARY}" 
    DIRECTORY
)
get_filename_component(
    RKAIQ_LIB_DIR 
    "${RKAIQ_LIBRARY}" 
    DIRECTORY
)
get_filename_component(
    MPP_LIB_DIR 
    "${ROCKCHIP_MPP_LIBRARY}" 
    DIRECTORY
)
target_link_options(${PROJECT_NAME} PRIVATE
    "-Wl,-rpath-link,${ROCKIT_LIB_DIR}"
    "-Wl,-rpath-link,${RKAIQ_LIB_DIR}"
    "-Wl,-rpath-link,${MPP_LIB_DIR}"
)

# In thông tin khi Configure
message(STATUS "RKNN runtime   : ${RKNN_LIBRARY}")
message(STATUS "RKMPI headers  : ${RKMPI_INCLUDE_DIR}")
message(STATUS "sample_comm.h : ${SAMPLE_COMM_INCLUDE_DIR}/sample_comm.h")
message(STATUS "ISP3 helper    : ${SAMPLE_COMM_ISP_INCLUDE_DIR}/sample_comm_isp.h")
message(STATUS "sample_comm.a : ${SAMPLE_COMM_LIBRARY}")
message(STATUS "RKAIQ headers  : ${RKAIQ_ROOT}")
