# Project 01 - Single-Person Face Verification

> **Xác minh khuôn mặt một người trên thiết bị nhúng, chạy trực tiếp bằng NPU của LuckFox Pico Max (RV1106) và camera CSI SC3336.**

**Stack:** C++17 · RK_MPI / RKAIQ · RKNN Runtime · YuNet · SFace · Python / OpenCV · CMake + Ninja  
**Nền tảng phát triển:** Ubuntu · **Thiết bị đích:** LuckFox Pico Max (RV1106)  
**Trạng thái:** Đã tích hợp camera, hai model RKNN INT8, xác minh embedding và preview realtime, tiếp tục tinh chỉnh ngưỡng/độ ổn định khi demo.

## Mục lục

1. [Giới thiệu](#1-giới-thiệu)
2. [Kiến trúc hệ thống](#2-kiến-trúc-hệ-thống)
3. [Cấu trúc project](#3-cấu-trúc-project)
4. [Model và dữ liệu](#4-model-và-dữ-liệu)
5. [Quy trình phát triển](#5-quy-trình-phát-triển)
6. [Chạy nhanh](#6-chạy-nhanh)
7. [Kết quả và giới hạn](#7-kết-quả-và-giới-hạn)
8. [Tài liệu chi tiết](#8-tài-liệu-chi-tiết)

## 1. Giới thiệu

Project triển khai bài toán **Single-Person Face Verification**: kiểm tra khuôn mặt xuất hiện trước camera có khớp với người đã đăng ký (`Person_A`) hay không. Hệ thống hiện tại sử dụng **hai model pretrained** với nguyên lý:

| Thành phần | Vai trò |
|---|---|
| **YuNet** | Phát hiện khuôn mặt, xuất bounding box và **5 landmarks** |
| **SFace** | Trích xuất **embedding 128 chiều** từ khuôn mặt đã căn chỉnh |
| **Cosine similarity** | So sánh embedding đang quan sát với `person_a.bin`, xuất `VERIFIED` / `UNKNOWN` |
| **SC3336 + RK_MPI** | Thu ảnh trực tiếp trên LuckFox, không cần gửi video về PC để chạy AI |
| **TCP preview + Python viewer** | Truyền ảnh/metadata và hiển thị bbox, landmarks, độ tương đồng trên Ubuntu |

**Không cần** train CNN riêng, classifier nhị phân hay bộ ảnh LFW để vận hành pipeline hiện tại. `UNKNOWN` là kết quả **không đạt ngưỡng cosine**, không phải một lớp được huấn luyện.

## 2. Kiến trúc hệ thống

```text
                UBUNTU - PHÁT TRIỂN / CHUẨN BỊ
  Enrollment/Person_A/*.jpg (thu sẵn từ SC3336)
                     │
             YuNet ONNX + SFace ONNX
                     │
             Embeddings/person_a.bin
                     │
           RKNN-Toolkit2: ONNX → INT8
                     │
       yunet.rknn + sface.rknn + person_a.bin
                     │
               CMake + Ninja
                     │
                  SCP / SSH
                     │
                     ▼
              LUCKFOX PICO MAX
  SC3336 → RK_MPI (NV12) → RGB → resize 640×640
                     │
                YuNet RKNN
                     │
          bbox + 5 landmarks + NMS
                     │
                faces > 0 ?
             ┌───────┴────────┐
            Không             Có
             │                │
        Frame tiếp       Align 112×112
             │                │
             │           SFace RKNN
             │                │
             │         Embedding 128D
             │                │
             │     Cosine với person_a.bin
             │                │
             │       VERIFIED / UNKNOWN
             └────────┬────────┘
                      │
            TCP preview (latest-only)
                      │
          Python viewer trên Ubuntu
```

**Quan trọng:** Model chạy **trên NPU của LuckFox**. Viewer trên Ubuntu **chỉ hiển thị kết quả**, không thực hiện inference. Preview hiện dùng **TCP/FVP2**, **không phải RTSP**.

## 3. Cấu trúc project

```text
01_Single-Person_Face_Verification/
├── CMakeLists.txt                  # Target C++ và thư viện LuckFox SDK
├── CMakePresets.json               # Configure / build preset FaceVerification
├── CMake/
│   └── toolchain-rv1106.cmake      # ARM cross-compiler
├── Enrollment/
│   └── Person_A/                   # Ảnh đăng ký thu từ SC3336
├── Models/
│   ├── detectors/
│   │   ├── yunet.onnx              # YuNet gốc
│   │   └── yunet.rknn              # YuNet INT8 cho RV1106
│   └── recognizers/
│       ├── sface.onnx              # SFace gốc
│       └── sface.rknn              # SFace INT8 cho RV1106
├── Embeddings/
│   ├── person_a.bin                # 128 float32 (512 byte)
│   └── person_a.json               # Metadata enrollment / hash model
├── Calibration/                    # Ảnh + danh sách calibration cho 2 model
├── Scripts/
│   ├── main.py                     # CLI: enroll, quantize, ...
│   ├── face_pipeline.py
│   ├── rknn_tools.py
│   ├── camera_io.py
│   ├── dataset_tools.py
│   ├── storage.py
│   └── README.md
├── Src/                            # Triển khai pipeline C++
│   ├── main.cpp
│   ├── camera_rkmpi.cpp
│   ├── nv12.cpp
│   ├── preprocess.cpp
│   ├── rknn_engine.cpp
│   ├── face_detector.cpp
│   ├── face_alignment.cpp
│   ├── face_verifier.cpp
│   ├── preview_stream.cpp
│   ├── camera.cpp
│   ├── image.cpp
│   └── README.md
├── Inc/                            # Header tương ứng với Src/
├── Deploy/
│   ├── run_camera.sh               # Chạy trên LuckFox (qua SSH)
│   └── view_face_verification.py   # Viewer trên Ubuntu
├── Docs/
│   ├── Download_ONNX_Models.md
│   └── CPP_Deploy_LuckFox.md
├── build/                          # Kết quả biên dịch (tự sinh)
│   └── FaceVerification
└── README.md                       # Tài liệu tổng quan (file này)
```

**Quy tắc:** `Models/*.onnx`, dữ liệu enrollment và script Python dùng trên Ubuntu, sang LuckFox chỉ cần **binary C++, hai `.rknn`, `person_a.bin`, `run_camera.sh`**. Không đưa cả workspace lên board.

## 4. Model và dữ liệu

| Hạng mục | Thiết lập đang sử dụng |
|---|---|
| Face detector | **YuNet 2023mar** (ONNX → RKNN INT8), input **640×640** |
| Face recognizer | **SFace 2021dec** (ONNX → RKNN INT8), input aligned **112×112** |
| Đặc trưng | **128D**, chuẩn hóa L2 trước khi so khớp cosine |
| Enrollment | Đã tạo embedding từ **20 ảnh hợp lệ** thu bằng SC3336 |
| File tham chiếu | `Embeddings/person_a.bin` - **512 byte**, float32 little-endian |
| Ngưỡng cosine | `0.363` đang dùng làm **ngưỡng tham khảo**, chưa hiệu chuẩn riêng |
| Backend | RKNN Runtime trên **RV1106 NPU** |

Ảnh gốc được lưu lại để có thể tái tạo embedding. Khi đổi phiên bản SFace hoặc cách tiền xử lý/căn chỉnh, cần **tạo hoặc xác nhận lại embedding tham chiếu**, không mặc định output ONNX và RKNN INT8 tương đương hoàn toàn.

Nguồn tải hai model: [Docs/Download_ONNX_Models.md](Docs/Download_ONNX_Models.md).

## 5. Quy trình phát triển

```text
Ảnh Person_A từ SC3336
    ↓
Python enrollment → person_a.bin / person_a.json
    ↓
Chuẩn bị calibration riêng cho YuNet và SFace
    ↓
RKNN-Toolkit2 v1.6.0 → yunet.rknn / sface.rknn
    ↓
C++ (RK_MPI + RKNN Runtime) → CMake + Ninja cross-compile
    ↓
SCP lên LuckFox → chạy camera và xác minh tại biên
    ↓
Ubuntu viewer nhận preview và hiển thị kết quả
```

Các lệnh Python đầy đủ, thông tin môi trường RKNN và calibration nằm trong [Scripts/README.md](Scripts/README.md). Kiến trúc chi tiết và vai trò từng `.cpp` nằm trong [Src/README.md](Src/README.md).

## 6. Chạy nhanh

### 6.1. Chuẩn bị embedding / RKNN (chỉ khi làm lại model)

Trên **Ubuntu**, từ thư mục gốc project:

```bash
# Enrollment: dùng Python có OpenCV và YuNet/SFace ONNX
python3 Scripts/main.py enroll

# Chuẩn bị calibration đúng kích thước YuNet 640×640
python3 Scripts/main.py quantize --stage prepare --yunet-size 640

# Lượng tử (dùng môi trường RKNN Toolkit2 đã cài)
source ~/Luckfox_EdgeAI_Workspace/Tools/.venv-rknn/bin/activate
python Scripts/main.py quantize --stage convert --yunet-size 640
```

Nếu `person_a.bin`, `yunet.rknn` và `sface.rknn` đã có thì **bỏ qua toàn bộ bước này**.

### 6.2. Build trên Ubuntu

```bash
cmake --preset FaceVerification                 # Lần đầu hoặc cấu hình lại
cmake --build --preset FaceVerification         # Build với Ninja
```

Sau lần configure đầu tiên, thường chỉ cần lệnh `cmake --build --preset FaceVerification`.

### 6.3. Deploy lên LuckFox (lần đầu)

Sử dụng **2 terminal**: một terminal Ubuntu và một terminal SSH vào LuckFox (`172.32.0.93` trong mạng của project).

**Terminal SSH - LuckFox:**

```bash
mkdir -p /root/FaceVerification/Models/detectors \
         /root/FaceVerification/Models/recognizers \
         /root/FaceVerification/Embeddings
```

**Terminal Ubuntu - tại thư mục gốc project:**

```bash
scp -O build/FaceVerification Deploy/run_camera.sh \
  root@172.32.0.93:/root/FaceVerification/

scp -O Models/detectors/yunet.rknn \
  root@172.32.0.93:/root/FaceVerification/Models/detectors/

scp -O Models/recognizers/sface.rknn \
  root@172.32.0.93:/root/FaceVerification/Models/recognizers/

scp -O Embeddings/person_a.bin \
  root@172.32.0.93:/root/FaceVerification/Embeddings/
```

### 6.4. Demo trực tiếp

**Terminal SSH - LuckFox:**

```bash
cd /root/FaceVerification
sh run_camera.sh --full
```

**Terminal Ubuntu - mở viewer:**

```bash
python3 Deploy/view_face_verification.py
```

Viewer hỗ trợ `Q`/`Esc` để đóng, `F` để fullscreen. Trên LuckFox, `Ctrl+C` dừng chương trình và giải phóng camera, nếu gặp lỗi driver ở `ispStreamOff`, xem hướng dẫn xử lý tại Docs thay vì cố kết thúc bằng `kill -9` liên tục.

**Cập nhật sau này:** sửa C++ → `cmake --build --preset FaceVerification` → SCP **chỉ** `build/FaceVerification` → chạy lại, không cần xóa project hay copy lại model nếu model không đổi.

Xem chi tiết từng terminal và xử lý `rkipc` tại [Docs/CPP_Deploy_LuckFox.md](Docs/CPP_Deploy_LuckFox.md).

## 7. Kết quả và giới hạn

### 7.1. Kết quả đo trên LuckFox Pico Max

Số liệu **chế độ detect-only, không bật preview**, với camera 640×480 và YuNet 640×640:

| Chỉ số | Trước tối ưu resize | Sau tối ưu resize |
|---|---:|---:|
| Resize CPU | ~142 ms | **~9–10 ms** |
| YuNet run (gồm copy/NPU/unpack) | ~46–47 ms | **~46–47 ms** |
| Decode/NMS | ~1,1 ms | **~1,1 ms** |
| Tổng inference | ~190 ms | **~57–59 ms** |
| FPS toàn pipeline | ~4,9 FPS | **~14–14,5 FPS** |

Bản tối ưu tập trung vào phép **resize ảnh trên CPU**. Inference YuNet trên NPU gần như giữ nguyên. Đây **không phải FPS được bảo đảm khi chạy Full + SFace + preview**, FPS chế độ đầy đủ còn phụ thuộc số mặt và tải hệ thống.

### 7.2. Phạm vi chưa hoàn thiện

- Cần đánh giá trên **ảnh/video khác enrollment**, gồm cả người không đăng ký.
- Ngưỡng cosine `0.363` chỉ mang tính tham khảo, cần hiệu chuẩn theo dữ liệu từ SC3336 và pipeline RKNN INT8.
- Chưa có chống giả mạo (presentation attack detection/liveness), không nên dùng để mở khóa hay bảo vệ truy cập quan trọng.
- Một số firmware LuckFox có thể phát sinh xung đột `rkipc` hoặc lỗi đóng ISP, cơ chế dừng phải được kiểm tra theo firmware thực tế.

## 8. Tài liệu chi tiết

| Tài liệu | Nội dung |
|---|---|
| [Scripts/README.md](Scripts/README.md) | Python enrollment, calibration, lượng tử hai model RKNN |
| [Src/README.md](Src/README.md) | Kiến trúc C++, chức năng từng module và xử lý realtime |
| [Docs/Download_ONNX_Models.md](Docs/Download_ONNX_Models.md) | Tải YuNet/SFace ONNX đúng tên và đúng vị trí |
| [Docs/CPP_Deploy_LuckFox.md](Docs/CPP_Deploy_LuckFox.md) | CMake/Ninja, cấu trúc trên board, SCP/SSH, chạy và xử lý lỗi |

---

**Ghi chú:** Các kết quả hiệu năng nêu trên là số đo của cấu hình thử nghiệm hiện tại, không phải benchmark chung cho mọi thiết bị RV1106.