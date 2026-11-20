# C++ Pipeline — Single-Person Face Verification

> **Project:** `01_Single-Person_Face_Verification`  
> **Thiết bị:** LuckFox Pico Max (RV1106) + camera SC3336  
> **Ngôn ngữ:** C++17 · **Build:** CMake + Ninja · **Inference:** RKNN Runtime trên NPU

Thư mục `Src/` chứa mã C++ chạy **trực tiếp trên LuckFox**. Chương trình nhận ảnh từ SC3336, phát hiện khuôn mặt bằng **YuNet**, trích xuất đặc trưng bằng **SFace** rồi so khớp với embedding đã đăng ký của `Person_A`. Ubuntu chỉ đóng vai trò biên dịch, chuyển file và hiển thị kết quả.

## 1. Sơ đồ luồng xử lý

```text
                  LUCKFOX PICO MAX (RV1106)
                            │
                      SC3336 camera
                            │
                 RK_MPI / RKAIQ (NV12)
                            │
                        RGB frame
                            │
                     resize 640×640
                            │
                 YuNet RKNN (detection)
                            │
              decode + NMS + 5 landmarks
                            │
              ┌─────────────┴────────────┐
              │                          │
           faces=0                    faces>0
              │                          │
       sang frame mới             align mặt 112×112
              │                          │
              │                    SFace RKNN
              │                          │
              │                 embedding 128D
              │                          │
              │            cosine với person_a.bin
              │                          │
              │                 VERIFIED / UNKNOWN
              │                          │
              └─────────────┬────────────┘
                            │
                 TCP preview (FVP2, async)
                            │
                  Ubuntu: Python viewer
```

**Hai chế độ:**

- **Full (mặc định):** YuNet → alignment → SFace → so khớp với `Person_A`.
- **Detect-only:** Chỉ YuNet, phù hợp đo tốc độ detector, **không chạy SFace**.

SFace chỉ suy luận khi YuNet phát hiện ít nhất một khuôn mặt. Vì vậy log `faces=0`, `sface_avg=0.0 ms` là bình thường.

## 2. Vai trò của các file `.cpp`

| File | Trách nhiệm chính |
|---|---|
| `main.cpp` | Điểm vào chương trình, đọc tham số CLI, nạp model, điều phối từng frame, log `[FRAME]`, `[PERF]`, `[STAGES]`, xử lý tín hiệu dừng. |
| `camera_rkmpi.cpp` | Khởi tạo ISP/RKAIQ và VI qua RK_MPI, lấy frame SC3336 (NV12), trả buffer, giải phóng camera khi thoát. |
| `nv12.cpp` | Chuyển NV12 từ camera thành `Image` RGB24 để các bước tiếp theo sử dụng. |
| `image.cpp` | Đọc ảnh PPM P6 RGB cho chế độ xử lý ảnh ngoại tuyến. |
| `camera.cpp` | Hàm hỗ trợ đọc luồng RGB thô, không phải đường lấy ảnh chính của chế độ SC3336 hiện tại. |
| `preprocess.cpp` | Resize ảnh cho YuNet, bản tối ưu dùng nội suy số nguyên, tránh tính toán lặp trên CPU. Có hàm crop/resize hỗ trợ. |
| `rknn_engine.cpp` | Đọc `.rknn`, kiểm tra input/output tensor, cấp bộ nhớ RKNN, gọi `rknn_run()`, giải lượng tử/đọc các output tensor. |
| `face_detector.cpp` | Giải mã **12 tensor** đầu ra của YuNet, tính bbox, điểm tin cậy, 5 landmarks, lọc bằng NMS. |
| `face_alignment.cpp` | Biến đổi hình học theo **5 landmarks**, đưa khuôn mặt về ảnh RGB `112×112` theo template SFace. |
| `face_verifier.cpp` | Đọc `person_a.bin` (128 float32), chuẩn hóa embedding, tính cosine similarity và quyết định theo threshold. |
| `preview_stream.cpp` | Tạo TCP preview FVP2, gửi ảnh/metadata bằng luồng riêng, ưu tiên frame mới, tránh chặn luồng inference khi viewer chậm. |

Các khai báo kiểu dữ liệu, class và hàm tương ứng nằm trong **`../Inc/`**. Khi thay đổi giao diện module, cập nhật cả `.h` và `.cpp`.

## 3. Dữ liệu và mô hình

```text
Models/detectors/yunet.rknn       # Face detection, input 640×640
Models/recognizers/sface.rknn     # Face embedding, input 112×112
Embeddings/person_a.bin           # Embedding tham chiếu: 128 float32 = 512 byte
```

- **YuNet:** model nhận ảnh theo thứ tự kênh **BGR**, mã gốc lưu frame dưới dạng RGB nên engine đổi thứ tự kênh khi nạp input YuNet.
- **SFace:** nhận khuôn mặt **RGB đã căn chỉnh**, output 128 chiều, được chuẩn hóa trước khi tính cosine.
- **Giải mã YuNet:** 12 tensor gồm các nhóm `cls`, `obj`, `bbox`, `kps` tại stride 8/16/32. Bbox/landmarks được đổi tỷ lệ từ kích thước input detector về tọa độ frame gốc.
- **Verification:** `cosine >= match_threshold` thì `VERIFIED`, ngược lại `UNKNOWN`. Ngưỡng mẫu `0.363` là giá trị khởi điểm, **không phải ngưỡng đã hiệu chuẩn riêng cho RKNN/SC3336**.

> `person_a.bin` được tạo ở giai đoạn Python Enrollment trên Ubuntu. **Không có classifier cần huấn luyện trên LuckFox**, chương trình so sánh embedding trực tiếp.

## 4. Vòng đời ứng dụng

1. `main.cpp` nạp YuNet RKNN, ở chế độ Full nạp thêm SFace và `person_a.bin`.
2. `preview_stream.cpp` mở cổng TCP `8765` nếu bật preview.
3. `camera_rkmpi.cpp` khởi tạo RKAIQ/ISP + RK_MPI/VI, lấy frame NV12 từ SC3336.
4. `nv12.cpp` chuyển NV12 → RGB, `preprocess.cpp` resize frame để đưa vào YuNet.
5. `rknn_engine.cpp` chạy YuNet, `face_detector.cpp` giải mã và NMS.
6. Với từng mặt hợp lệ: `face_alignment.cpp` → SFace RKNN → `face_verifier.cpp`.
7. Ghi log kết quả và đẩy frame + metadata sang preview bất đồng bộ, quay lại bước 3.
8. Khi dừng (ví dụ `Ctrl+C`), thoát vòng lặp và giải phóng preview, camera/ISP, RKNN. Phần teardown cần giữ đúng thứ tự phù hợp SDK/firmware thực tế.

**Preview là TCP FVP2, không phải RTSP.** Viewer ở Ubuntu (`../Deploy/view_face_verification.py`) vẽ bbox, landmarks, nhãn và similarity, suy luận AI vẫn diễn ra trên LuckFox.

## 5. Các điểm tối ưu hiệu năng

- Dùng **RKNN INT8** để chạy detector/recognizer trên NPU RV1106.
- Resize **640×480 → 640×640** bằng nhánh số nguyên tối ưu trong `preprocess.cpp`, hạn chế phép tính số thực trong vòng lặp pixel.
- Chỉ chạy SFace khi detector trả về mặt, không suy luận nhận diện trên frame trống.
- Preview dùng worker thread và hàng đợi **latest-only**: không giữ hàng dài các frame cũ, không buộc inference đợi client TCP.
- Log hiệu năng theo từng nhóm 30 frame để phân tích đúng nút thắt.

Kết quả đo **riêng chế độ detect-only, tắt preview** trên board của project (không phải hiệu năng Full cố định):

| Công đoạn | Thời gian trung bình đã đo |
|---|---:|
| Capture | ~12 ms |
| Resize | ~9–10 ms |
| YuNet run (gồm copy/NPU/unpack) | ~46–47 ms |
| Decode/NMS | ~1,1 ms |
| Pipeline | ~14–14,5 FPS |

FPS khi **Full + preview + có mặt** thay đổi theo số khuôn mặt và lượng xử lý SFace, không nên lấy 14 FPS detect-only làm số liệu hiệu năng cố định của toàn hệ thống.

## 6. Sửa code ở đâu?

| Muốn thay đổi | File ưu tiên |
|---|---|
| Thay điều kiện xử lý frame, ngưỡng, log FPS | `main.cpp` |
| Sửa camera SC3336, timeout, đóng ISP/VI | `camera_rkmpi.cpp` |
| Tối ưu resize ảnh | `preprocess.cpp` |
| Sửa chuẩn input/output tensor hoặc lỗi RKNN Runtime | `rknn_engine.cpp` |
| Chỉnh detection threshold/NMS/giải mã landmarks | `face_detector.cpp` (ngưỡng CLI ở `main.cpp`) |
| Chỉnh phép căn chỉnh khuôn mặt | `face_alignment.cpp` |
| Chỉnh cách so khớp embedding | `face_verifier.cpp` |
| Giảm lag preview, quản lý thread/socket | `preview_stream.cpp` |

**Nguyên tắc:** không thay detector, recognizer, ảnh chuẩn hóa hoặc threshold cùng lúc khi debug, mỗi thay đổi cần giữ khả năng đối chiếu kết quả với bản đang chạy tốt.

## 7. Build và chạy — tóm tắt

**Ubuntu** (trong thư mục gốc project):

```bash
cmake --preset FaceVerification          # Lần đầu/cấu hình lại
cmake --build --preset FaceVerification  # Các lần build
```

**LuckFox** (sau khi đã SCP binary, model, embedding theo tài liệu Deploy):

```bash
cd /root/FaceVerification
sh run_camera.sh --full
```

**Ubuntu — xem preview:**

```bash
python3 Deploy/view_face_verification.py
```

Chi tiết cấu trúc thư mục, các lệnh SCP, IP `172.32.0.93`, xử lý `rkipc` và cách cập nhật binary: **[Docs/CPP_Deploy_LuckFox.md](../Docs/CPP_Deploy_LuckFox.md)**.

---

**Phạm vi tài liệu:** README này mô tả kiến trúc và luồng C++ đã xây dựng. Phần chuẩn bị dataset, Enrollment và chuyển ONNX → RKNN thuộc `Scripts/README.md` và các tài liệu trong `Docs/`.
