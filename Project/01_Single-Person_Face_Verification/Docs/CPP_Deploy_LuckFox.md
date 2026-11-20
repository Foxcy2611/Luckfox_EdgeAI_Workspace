# Triển khai Face Verification trên LuckFox Pico Max

> **Project:** `01_Single-Person_Face_Verification`  
> **Board:** LuckFox Pico Max (RV1106) · **Camera:** SC3336  
> **IP LuckFox:** `172.32.0.93` · **Tài khoản SSH:** `root`  
> **Thư mục chạy trên board:** `/root/FaceVerification`  
> **Nguyên tắc:** dùng **2 terminal** — một terminal **Ubuntu**, một terminal **SSH vào LuckFox**. Không cần script deploy bổ sung.

## 1. Quy ước tổ chức thư mục

### 1.1. Trên Ubuntu (mã nguồn và build)

```text
~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification/
├── CMakeLists.txt
├── CMakePresets.json
├── CMake/
│   └── toolchain-rv1106.cmake
├── Src/                         # Mã C++
├── Inc/                         # Header C++
├── Models/
│   ├── detectors/
│   │   ├── yunet.onnx           # Model gốc, dùng trên Ubuntu
│   │   └── yunet.rknn           # Gửi sang LuckFox
│   └── recognizers/
│       ├── sface.onnx           # Model gốc, dùng trên Ubuntu
│       └── sface.rknn           # Gửi sang LuckFox
├── Embeddings/
│   └── person_a.bin             # Vector tham chiếu 128D, float32
├── Deploy/
│   ├── run_camera.sh            # SCP sang LuckFox để chạy C++
│   └── view_face_verification.py  # Chạy trên Ubuntu để xem preview
├── Docs/
│   └── CPP_Deploy_LuckFox.md
└── build/
    └── FaceVerification        # Executable cross-compile ARM
```

### 1.2. Trên LuckFox (chỉ giữ file cần chạy)

```text
/root/FaceVerification/
├── FaceVerification
├── run_camera.sh
├── Models/
│   ├── detectors/
│   │   └── yunet.rknn
│   └── recognizers/
│       └── sface.rknn
└── Embeddings/
    └── person_a.bin
```

**Quy tắc quan trọng**

- **Không** SCP `Src/`, `Inc/`, ONNX, tập ảnh Enrollment hoặc Python viewer sang LuckFox.
- **Không** đặt cả `yunet.rknn`, `sface.rknn`, `person_a.bin` cùng cấp với executable: `run_camera.sh` đang dùng đúng các đường dẫn con trong sơ đồ.
- Toàn bộ lệnh SCP chạy **trên Ubuntu**, không chạy trong terminal đã SSH.
- Chỉ xóa `/root/FaceVerification` khi cần **triển khai sạch**. Khi sửa code về sau, chỉ SCP lại file thay đổi.
- Thư mục `/root` có thể nằm trên phân vùng hạn chế dung lượng/tính lưu trữ, kiểm tra `df -h /root` nếu có vấn đề sau reboot.

## 2. Build trên Ubuntu

**Terminal A — Ubuntu** (dấu nhắc dạng `chien@ngoc-chien:...$`): Tiến hành build dự án mã nguồn C++ bằng Cmake + Ninja

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification

# Chạy lần đầu hoặc khi cần cấu hình lại
cmake --preset FaceVerification

# Build với Ninja theo build preset
cmake --build --preset FaceVerification

# Kiểm tra executable cho ARM, không phải x86-64
file build/FaceVerification
```

Những lần sau, khi chỉ sửa `.cpp`/`.h`, thông thường chỉ cần:

```bash
cmake --build --preset FaceVerification
```

`ninja: no work to do.` nghĩa là không có mục tiêu nào cần biên dịch lại, không phải lỗi.

## 3. Triển khai sạch: làm **một lần**

### 3.1. Tạo thư mục trên LuckFox

**Terminal B — SSH vào LuckFox** (dấu nhắc dạng `[root@luckfox ...]#`):

```bash
# Nếu chưa SSH, mở từ Ubuntu:
# ssh root@172.32.0.93

# CHỈ thực hiện khi chủ động triển khai sạch:
rm -rf /root/FaceVerification

mkdir -p /root/FaceVerification/Models/detectors
mkdir -p /root/FaceVerification/Models/recognizers
mkdir -p /root/FaceVerification/Embeddings

cd /root/FaceVerification
pwd
```

`pwd` cần trả về `/root/FaceVerification`. Không cần tạo lại thư mục sau mỗi lần build.

### 3.2. SCP đúng vị trí từ Ubuntu

**Quay lại Terminal A — Ubuntu** và bảo đảm đang ở thư mục gốc project:

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification
```

Chuyển executable và script chạy:

```bash
scp -O build/FaceVerification Deploy/run_camera.sh \
  root@172.32.0.93:/root/FaceVerification/
```

Chuyển hai model RKNN:

```bash
scp -O Models/detectors/yunet.rknn \
  root@172.32.0.93:/root/FaceVerification/Models/detectors/

scp -O Models/recognizers/sface.rknn \
  root@172.32.0.93:/root/FaceVerification/Models/recognizers/
```

Chuyển embedding tham chiếu:

```bash
scp -O Embeddings/person_a.bin \
  root@172.32.0.93:/root/FaceVerification/Embeddings/
```

`scp -O` sử dụng giao thức SCP cũ để tương thích firmware SSH/Dropbear của LuckFox.

### 3.3. Kiểm tra trên LuckFox

**Terminal B — LuckFox:**

```bash
cd /root/FaceVerification
ls -lh
ls -lh Models/detectors/ Models/recognizers/ Embeddings/
chmod +x FaceVerification
```

Phải có đủ **5 file**: `FaceVerification`, `run_camera.sh`, `yunet.rknn`, `sface.rknn`, `person_a.bin`.
> `person_a.bin` hiện có kích thước 512 byte (128 số float32).

## 4. Chạy demo realtime

### 4.1. Chạy trên LuckFox

**Terminal B — LuckFox:**

```bash
cd /root/FaceVerification

# Tùy chọn: kiểm tra model, không khởi động camera
sh run_camera.sh --inspect-models

# Chạy FULL: SC3336 + YuNet + SFace + cosine verification
sh run_camera.sh --full
```

`--full` cũng là chế độ mặc định của bản `run_camera.sh` hiện tại, nên có thể chạy ngắn gọn:

```bash
sh run_camera.sh
```

Script sẽ kiểm tra model, dừng `rkipc` nếu cần để giải phóng SC3336, rồi khởi động C++ và mở **TCP preview tại port `8765`**. RTSP mặc định của `rkipc` sẽ ngừng khi tiến trình đó bị dừng.

### 4.2. Xem camera trên Ubuntu

**Terminal A — Ubuntu** (giữ Terminal B chạy chương trình):

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification

python3 Deploy/view_face_verification.py
```

IP đã mặc định là `172.32.0.93`. Viewer nhận luồng **TCP FVP2**, không phải RTSP/VLC, inference thực hiện trên NPU LuckFox, Ubuntu chỉ hiển thị bbox, landmarks, cosine và `VERIFIED/UNKNOWN`.

Tùy chọn phóng lớn:

```bash
python3 Deploy/view_face_verification.py --window-width 1600
```

Trong viewer: `F` để bật/tắt fullscreen, `Q` hoặc `Esc` để đóng cửa sổ.

### 4.3. Dừng chương trình

- Đóng viewer Ubuntu bằng `Q` (chương trình trên LuckFox vẫn chạy).
- Trong **Terminal B — LuckFox**, nhấn **`Ctrl+C` một lần** để dừng inference và giải phóng camera/ISP.
- Chờ hiện lại dấu nhắc `[root@luckfox ...]#`, rồi mới chạy tiếp.
- Cơ chế shutdown đã được kiểm tra thành công trong lần chạy hiện tại, nếu bản firmware/code khác bị kẹt tại `ispStreamOff start`, **không** lặp `Ctrl+C` hoặc `kill -9` liên tục. Kiểm tra SSH/tiến trình và log shutdown trước khi thử lại.

## 5. Các lần cập nhật sau (không xóa thư mục)

**Ví dụ chỉ sửa code C++:**

**Terminal A — Ubuntu:**

```bash
cmake --build --preset FaceVerification

# Dừng chương trình trên LuckFox trước khi chép đè executable
scp -O build/FaceVerification \
  root@172.32.0.93:/root/FaceVerification/
```

**Terminal B — LuckFox:**

```bash
cd /root/FaceVerification
sh run_camera.sh --full
```

Nếu chỉ sửa `run_camera.sh`, SCP lại **duy nhất script**:

```bash
scp -O Deploy/run_camera.sh root@172.32.0.93:/root/FaceVerification/
```

Nếu lượng tử lại model hoặc enrollment lại Person_A, SCP lại **đúng file `.rknn` hoặc `.bin` đã thay đổi**, không xóa toàn bộ project.

## 6. Lệnh tùy chọn và xử lý lỗi

| Mục đích | Lệnh trên LuckFox |
|---|---|
| Chạy đầy đủ | `sh run_camera.sh --full` |
| Chỉ kiểm tra YuNet, không dùng SFace | `sh run_camera.sh --detect-only` |
| Chạy 60 frame rồi tự dừng | `sh run_camera.sh --full --max-frames 60` |
| Chỉ đo inference, không truyền preview | `sh run_camera.sh --detect-only --no-preview --max-frames 60` |
| Giảm ngưỡng detection để thử | `sh run_camera.sh --full --detect-threshold 0.5` |
| Hiển thị trợ giúp script | `sh run_camera.sh --help` |

**Các lỗi dễ gặp:**

- **`Missing YuNet RKNN model`**: kiểm tra `Models/detectors/yunet.rknn`, **không** để file nằm cùng cấp với `run_camera.sh`.
- **`rkipc is still running`**: camera vẫn bị tiến trình mặc định giữ hoặc tiến trình tự khởi động lại. Xem `pidof rkipc`, `ps | grep rkipc`, không tùy tiện dừng toàn bộ dịch vụ khởi động vì có thể ảnh hưởng mạng/SSH.
- **`No route to host` / không SSH được**: kiểm tra cáp, kết nối và `ping -c 3 172.32.0.93` từ Ubuntu. Ping được không bảo đảm SSH đang hoạt động.
- **`ninja: no work to do`**: bình thường — build đã cập nhật.
- **`faces=0` / `sface_avg=0`**: YuNet chưa phát hiện mặt, nên SFace chưa chạy ở frame đó.
- **FPS thấp**: phiên bản tối ưu `preprocess.cpp` đã giảm `resize_avg` từ khoảng **142 ms xuống 9–10 ms**, không thay bằng bản preprocess cũ. Tham chiếu số đo đã kiểm thử: **~14–14,5 FPS** ở chế độ detection-only, không preview, full pipeline phụ thuộc số mặt và SFace.

## 7. Tóm tắt 2 terminal

```text
TERMINAL A — UBUNTU                      TERMINAL B — LUCKFOX (SSH)
────────────────────────                 ─────────────────────────────
cmake --build --preset ...                mkdir -p /root/FaceVerification/...
scp -O binary / models / embedding  ──►  ls -lh, kiểm tra file
                                         sh run_camera.sh --full
python3 Deploy/view_face_verification.py ◄── TCP FVP2 :8765
(Q đóng viewer)                           (Ctrl+C dừng inference)
```

**Trạng thái hiện tại:** Camera SC3336, YuNet/SFace RKNN và viewer đã chạy thực tế. Bản resize tối ưu cho ~14 FPS ở bài đo YuNet-only, sau cập nhật shutdown đã có lần `Ctrl+C` thoát thành công. Đây không phải cam kết rằng mọi lần chạy/firmware sẽ có cùng FPS hoặc không gặp lỗi driver.