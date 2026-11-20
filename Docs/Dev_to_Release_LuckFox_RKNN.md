# 🚀 Quy trình phát triển và triển khai Edge AI trên LuckFox

## 📑 Mục lục

- [1. Tổng quan pipeline](#1-tổng-quan-pipeline)
- [2. Vai trò của từng thành phần](#2-vai-trò-của-từng-thành-phần)
- [3. Giai đoạn DEV — thử nghiệm bằng SCP](#3-giai-đoạn-dev--thử-nghiệm-bằng-scp)
- [4. Giai đoạn RELEASE — đóng gói firmware](#4-giai-đoạn-release--đóng-gói-firmware)
- [5. Khi nào cần build lại image?](#5-khi-nào-cần-build-lại-image)

## 1. Tổng quan pipeline

```text
LAPTOP UBUNTU                                   LUCKFOX PICO MAX (RV1106)

Dataset
   │
   ▼
Train model (TensorFlow/Keras)
   │
   └── model.keras
           │ Export sang định dạng tương thích
           ▼
        model.onnx
           │ RKNN-Toolkit2 + calibration/quantization
           ▼
        model.rknn ──────────────────────────────┐
                                                 │
C++ source (camera + preprocess + RKNN + output) │
   │                                             │
   ▼ Cross-compile cho ARM                       │
hand_detect (ELF binary) ────────────────────────┤
                                                 ▼
                          DEV: SCP sang board → chạy → debug
                                                 │
                                                 ▼
                         RELEASE: đưa binary + model + startup
                                  vào SDK → đóng gói firmware
                                                 │
                                                 ▼
                         Nạp thẻ SD → cấp nguồn → Linux boot
                                     → app tự chạy
```

**Nguyên tắc:** `.rknn` **không được biên dịch cùng** code C++. Hai thành phần được tạo riêng: bộ chuyển đổi model tạo `.rknn`, cross-compiler tạo binary C++. Chương trình C++ **nạp** model `.rknn` lúc chạy.

## 2. Vai trò của từng thành phần

| Thành phần | Vai trò |
|---|---|
| `model.keras` | Model gốc sau huấn luyện trên laptop, có thể lưu cùng code và metadata training. |
| `model.onnx` | Định dạng trung gian **nếu export tương thích**, một số layer/operator cần chỉnh khi chuyển đổi. |
| `model.rknn` | Đồ thị, trọng số và metadata/tensor cho RKNN Runtime, với RV1106 thường dùng lượng tử hóa INT8. |
| `hand_detect` | File ELF được cross-compile từ C/C++, điều phối camera, tiền xử lý, inference và kết quả. |
| Thư viện runtime | RKNN runtime, media/RKMPI, libc và những `.so` mà app cần, phải tương thích với firmware. |
| `RkLunch.sh` hoặc startup service | Khởi chạy ứng dụng sau khi Linux boot, nếu đã tích hợp đúng cách. |
| `luckfox-pico` SDK | Build kernel/rootfs và đóng gói image, **không phải** nơi bắt buộc phải sửa app mỗi lần test. |

Pipeline lúc ứng dụng chạy:

```text
Camera SC3336 → driver/ISP → RKMPI (VI/VPSS) hoặc API phù hợp
              → resize / đổi màu / normalize theo đúng input model
              → RKNN Runtime → NPU → output tensors
              → hậu xử lý (argmax, threshold, NMS... tùy bài toán)
              → UART / MQTT / RTSP / log
```

- `rknn_run()` chỉ là một bước trong chuỗi `rknn_init` → cấu hình input → `rknn_run` → đọc/release output → hủy context.
- Kích thước `224×224`, RGB/BGR, NHWC/NCHW, mean/std và kiểu tensor **phải lấy theo model thực tế**, không mặc định.
- Với model INT8, hậu xử lý phải chú ý `scale` và `zero_point` khi giải lượng tử tensor đầu ra (nếu cần).

Hiểu đơn giản có thể kết luận như
- `SDK` cung cấp nền tảng và các thư viện
- `C++` điều phối camera, NPU và kết quả
- Toolchain có sẵn trong `SDK` giúp compile `C++` cho Luckfox (cross-compile)
- SCP dùng để thử nghiệm, `.sh` để khởi chạy

## 3. Giai đoạn DEV — thử nghiệm bằng SCP

**Mục đích:** Sau khi image nền đã boot tốt và SSH hoạt động, **không build/nạp image lại cho mỗi thay đổi code hoặc model**.

1. Train/export/chuyển model trên Ubuntu → `model.rknn`.
2. Cross-compile ứng dụng C++ với toolchain của SDK, link đúng RKNN/media libraries → `hand_detect`.
3. Chuyển model và binary sang vùng **có quyền ghi** trên board qua SCP.
4. Chạy thủ công qua SSH, xem log và kết quả, sửa và lặp lại.

Ví dụ (dùng IP USB RNDIS đã cấu hình trong `Build_Image.md`):

```bash
# TRÊN UBUNTU — xác nhận /userdata tồn tại và ghi được trước khi dùng
ssh root@172.32.0.93 'ls -ld /userdata, df -h /userdata'

# Tạo nơi thử nghiệm trên target (nếu /userdata có thể ghi)
ssh root@172.32.0.93 'mkdir -p /userdata/hand_gesture'

# Gửi hai artifact đã tạo, đường dẫn local thay theo project thực tế
scp ./build/hand_detect root@172.32.0.93:/userdata/hand_gesture/
scp ./models/model.rknn root@172.32.0.93:/userdata/hand_gesture/

# Chạy thủ công trên target
ssh root@172.32.0.93
cd /userdata/hand_gesture
chmod +x hand_detect
./hand_detect ./model.rknn
```

> Lệnh cuối **giả định** `hand_detect` nhận đường dẫn model ở tham số đầu tiên. Nếu code dùng đường dẫn hard-code, sửa lệnh theo chương trình. Không phải firmware nào cũng có `/userdata`, hãy kiểm tra mount và quyền ghi, hoặc chọn nơi lưu trữ phù hợp.

**Khi cần cập nhật:**

- Sửa C++ → **cross-compile lại** binary rồi SCP binary mới (dừng ứng dụng đang chạy trước khi thay).
- Train/quantize lại → SCP `model.rknn` mới, khởi động lại ứng dụng để nạp model mới.
- Chỉ đổi file config / ngưỡng xử lý → SCP config mới, không cần compile nếu app hỗ trợ đọc config.
- Đổi startup script → có thể SCP để kiểm tra, nhưng **không đồng nghĩa** image trên thẻ SD đã chứa thay đổi đó.

**Debug nhanh** trên target:

```bash
file /userdata/hand_gesture/hand_detect  # nếu firmware có lệnh file
ldd /userdata/hand_gesture/hand_detect   # nếu firmware có ldd
ps | grep hand_detect
free -h
```

Nếu không có `file`/`ldd` trên Buildroot tối giản, kiểm tra ELF và dependency trên Ubuntu bằng `file`, `readelf` hoặc công cụ tương ứng của cross-toolchain.

## 4. Giai đoạn RELEASE — đóng gói firmware

Chỉ thực hiện **sau khi app + model đã chạy ổn định bằng SCP**.

1. Chốt binary đã cross-compile, `model.rknn`, config, các thư viện cần thiết (nếu chưa có trên image) và script startup.
2. Đưa các file vào **đúng vị trí đóng gói** của SDK: rootfs overlay hoặc cơ chế đóng gói OEM tương ứng với cấu hình board.
3. Xác nhận thứ tự khởi động và xung đột với dịch vụ camera mặc định (ví dụ `rkipc`).
4. Đóng gói image theo SDK, với bản `luckfox-pico` hỗ trợ lệnh `./build.sh firmware` để đóng gói lại từ artifact đã build, nhưng phải chắc chắn thay đổi đã được đưa vào **đúng input đóng gói**. Nếu kernel/rootfs/app SDK thay đổi, có thể cần build lại thành phần tương ứng trước.
5. Nạp thẻ SD đúng phương pháp của `docs/Build_Image.md`, boot và kiểm tra ứng dụng **tự chạy không cần SSH**.

Ví dụ *ý tưởng* lệnh trong startup script (không ghi đè nguyên file `RkLunch.sh` sẵn có nếu chưa kiểm tra):

```sh
# Chèn vào vị trí phù hợp của script startup đang được firmware sử dụng
/oem/usr/bin/hand_detect /oem/usr/share/models/model.rknn \
  >> /userdata/hand_detect.log 2>&1 &
```

**Điều kiện:** `/oem/usr/bin/hand_detect` và model phải thực sự tồn tại, app có quyền thực thi, `/userdata` phải ghi được, dependency đầy đủ và camera không bị dịch vụ khác chiếm dụng. Tên/đường dẫn script startup phải xem trực tiếp trong image/SDK đang dùng.

```text
SDK nền + binary + model + startup/config (+ runtime libraries nếu thiếu)
                     ↓
              Đóng gói firmware
                     ↓
           Nạp SD → Cấp nguồn → Boot
                     ↓
            App tự khởi động và chạy
```

## 5. Khi nào cần build lại image?

| Thay đổi | Cross-compile C++? | SCP để test? | Đóng gói/nạp image lại? |
|---|:---:|:---:|:---:|
| Model `.rknn` | Không | ✅ | Không, trừ bản release |
| Logic C++ / hậu xử lý | ✅ | ✅ | Không, trừ bản release |
| File config / threshold | Không* | ✅ | Không, trừ bản release |
| Startup script | Không | ✅** | Chỉ khi muốn tích hợp vào firmware |
| Thêm runtime `.so` | Có thể cần link lại | Có thể | Khi cần tích hợp bền vững vào image |
| Kernel / driver / Device Tree | Theo thành phần | SCP đơn thuần không đủ | Thường cần build và cập nhật image/phân vùng liên quan |
| Phát hành thẻ SD tự chạy | App đã build | Không bắt buộc | ✅ |

\* Nếu app đọc config từ file.  
\** Chỉ test được nếu file nằm trên phân vùng ghi được và cơ chế khởi động thật sự đọc script đó.