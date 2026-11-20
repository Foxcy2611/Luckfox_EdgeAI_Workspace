# Python workflow — Enrollment → RKNN INT8

> **Project:** `01_Single-Person_Face_Verification`  
> **Host:** Ubuntu PC · **Thiết bị đích:** LuckFox Pico Max (**RV1106**) + camera **SC3336**  
> **Đầu vào có sẵn:** `Enrollment/Person_A/*.jpg` (20 ảnh đã thu từ SC3336)  
> **Đầu ra cần có:** `Embeddings/person_a.bin`, `Models/detectors/yunet.rknn`, `Models/recognizers/sface.rknn`.

## 1. Scripts dùng để làm gì?

| Tệp | Nhiệm vụ | Có chạy trực tiếp không? |
|---|---|---|
| `main.py` | CLI điều phối tất cả công việc | **Có** |
| `face_pipeline.py` | Phát hiện YuNet ONNX, căn chỉnh mặt, trích embedding SFace | Không |
| `storage.py` | Đọc/ghi embedding Float32 và metadata | Không |
| `rknn_tools.py` | Chuẩn bị calibration, lượng tử và xuất model RKNN | Không, `main.py quantize` gọi |
| `dataset_tools.py` | Duyệt/kiểm tra ảnh enrollment | Không |
| `camera_io.py` | Đọc webcam/RTSP từ Ubuntu khi cần | Không |

**Lưu ý:** `enroll` và `quantize` thực hiện trên Ubuntu. Chạy NPU và camera **trực tiếp trên LuckFox** là phần **C++/RKNN Runtime** riêng, không phải lệnh `live` của Python.

## 2. Cấu trúc tối thiểu

```text
01_Single-Person_Face_Verification/
├── Models/
│   ├── detectors/yunet.onnx
│   └── recognizers/sface.onnx
├── Enrollment/Person_A/              # Ảnh đã thu từ SC3336
├── Embeddings/
│   ├── person_a.bin                  # Sau enroll
│   └── person_a.json
├── Calibration/                      # Sinh tự động sau prepare
│   ├── yunet/
│   ├── sface/
│   ├── yunet.txt
│   ├── sface.txt
│   └── manifest.json
├── Scripts/
│   ├── main.py
│   ├── face_pipeline.py
│   ├── storage.py
│   ├── camera_io.py
│   ├── dataset_tools.py
│   ├── rknn_tools.py
│   └── README.md                     # Tài liệu này
└── Docs/Download_ONNX_Models.md
```

Nếu thiếu model ONNX, xem [`../Docs/Download_ONNX_Models.md`](../Docs/Download_ONNX_Models.md) để clone, đổi tên đúng và đối chiếu SHA-256.

## 3. Chạy nhanh: từ ảnh có sẵn đến INT8

### Bước 0 — Vào project, kiểm tra đường dẫn

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification
python3 Scripts/main.py check
```

`check` chỉ xem model và dataset đã tồn tại chưa. Hai file ONNX bắt buộc là `yunet.onnx` và `sface.onnx`.

### Bước 1 — Enrollment (môi trường Python có OpenCV)

```bash
python3 Scripts/main.py enroll
```

Luồng xử lý:

```text
Enrollment/Person_A/*.jpg
        ↓
YuNet ONNX: detect + 5 landmarks
        ↓
SFace: alignCrop 112×112 → feature 128D
        ↓
L2 normalize từng embedding → mean → L2 normalize
        ↓
Embeddings/person_a.bin  +  person_a.json
```

Output:

- `person_a.bin`: **128 số Float32 little-endian**, chuẩn hóa L2, có thể đọc bằng C++.
- `person_a.json`: số mẫu hợp lệ, SHA-256 model, định dạng và kích thước embedding.

**Project hiện đã enroll thành công 20 ảnh:** nếu `person_a.bin` và `person_a.json` đã có, không cần chạy lại bước này, miễn là hai model ONNX giữ nguyên.

### Bước 2 — Tạo calibration (chạy bằng Python/OpenCV hiện tại)

```bash
python3 Scripts/main.py quantize --stage prepare --yunet-size 640
```

Tự động sinh:

```text
Calibration/
├── yunet/              # Ảnh frame từ SC3336, resize 320×320
├── sface/              # Ảnh khuôn mặt đã alignCrop 112×112
├── yunet.txt           # Danh sách đường dẫn ảnh chuẩn bị cho RKNN Toolkit2
├── sface.txt
└── manifest.json       # Ghi hash model và cấu hình calibration
```

Mặc định dùng tối đa **100 ảnh**, nếu dataset chỉ có 20 ảnh, script sẽ dùng tối đa 20 ảnh đó. Muốn giới hạn số ảnh:

```bash
python3 Scripts/main.py quantize --stage prepare --max-images 20
```

> YuNet và SFace có input khác nhau nên dùng **hai tập calibration riêng**. Script V3 dùng YuNet 320×320 (BGR), SFace 112×112 đã align (RGB). Nếu thay ONNX hoặc `--yunet-size`, phải chạy lại `prepare`.

### Bước 3 — Convert / lượng tử INT8

Trong môi trường `rknn160`:

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification

python Scripts/main.py quantize --stage convert --yunet-size 640
```

`rknn_tools.py` sẽ thực hiện với từng model:

```text
ONNX → rknn.load_onnx → rknn.build(do_quantization=True, dataset=...)
     → rknn.export_rknn → .rknn
```

**Kết quả kỳ vọng nếu build thành công:**

```text
Models/detectors/yunet.rknn
Models/recognizers/sface.rknn
```

Có thể convert riêng khi cần dò lỗi:

```bash
python Scripts/main.py quantize --stage convert --model yunet
python Scripts/main.py quantize --stage convert --model sface
```

Hoặc thực hiện `prepare + convert` liên tiếp nếu cùng một môi trường có đầy đủ OpenCV/ONNX/RKNN:

```bash
python Scripts/main.py quantize --stage all
```

> File `.rknn` xuất ra **chưa đồng nghĩa chạy đúng trên board**. YuNet/SFace có thể gặp lỗi toán tử, shape hoặc sai khác INT8. Không nên đổi hash model hoặc chỉnh calibration để bỏ qua thông báo lỗi.

## 4. Lệnh tóm tắt (mặc định project đã có ảnh & model)

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification

# 1. Enrollment (đã có person_a.bin thì bỏ qua)
python3 Scripts/main.py enroll

# 2. Calibration (Python Ubuntu có cv2 + FaceDetectorYN/FaceRecognizerSF)
python3 Scripts/main.py quantize --stage prepare --yunet-size 640

# 3. Mở môi trường đã cài RKNN Toolkit2 v1.6.0
conda activate rknn160

# 4. Build 2 model RKNN
python Scripts/main.py quantize --stage convert --yunet-size 640
```

## 5. Tiếp theo: demo chạy thật trên LuckFox Pico Max

Python ở đây **không** chạy inference trên NPU board. Sau khi có hai model RKNN:

1. Đưa `yunet.rknn`, `sface.rknn` và `person_a.bin` sang LuckFox.
2. C++ lấy frame SC3336 qua `camera_rkmpi.cpp` / RK_MPI, xử lý NV12 → model input.
3. C++ gọi YuNet RKNN → decode box/landmarks → căn chỉnh SFace → SFace RKNN → cosine similarity.
4. Hiển thị bounding box, similarity, `VERIFIED`/`UNKNOWN` lên RTSP hoặc log, dùng CMake + Ninja cross-compile bằng LuckFox SDK.

**Đặc biệt:** `person_a.bin` hiện tạo bằng **SFace ONNX**. Trước khi dùng xác minh thật, phải đối chiếu embedding RKNN và ONNX, xác nhận cùng thứ tự kênh màu, face alignment và ngưỡng quyết định. Nếu không tương thích, tạo lại reference bằng chính pipeline RKNN.

## 6. Lỗi thường gặp

| Hiện tượng | Hướng xử lý |
|---|---|
| `FileNotFoundError: ...yunet.onnx / sface.onnx` | Xem `Docs/Download_ONNX_Models.md`, kiểm tra tên/đường dẫn. |
| `Embeddings/person_a.bin` không có | Chạy `python3 Scripts/main.py enroll`. |
| `No module named rknn` | Kích hoạt `rknn160`, kiểm tra wheel đã cài. |
| `Calibration/manifest.json` không có | Chạy `quantize --stage prepare` trước. |
| `model hash khong khop calibration` | Model đã thay đổi, chạy lại `prepare`, kiểm tra SHA và reference. |
| `load_onnx` / `build` lỗi | Ghi nguyên log RKNN, thường liên quan shape/opset/operator, không phải lỗi thiếu ảnh. |
| File `.rknn` có nhưng chạy board lỗi | Kiểm tra phiên bản Runtime, input layout, quant params, output tensor/postprocess. |

**Tham khảo:** [RKNN-Toolkit2](https://github.com/airockchip/rknn-toolkit2) · [OpenCV Zoo](https://github.com/opencv/opencv_zoo) · [`Docs/Download_ONNX_Models.md`](../Docs/Download_ONNX_Models.md).
