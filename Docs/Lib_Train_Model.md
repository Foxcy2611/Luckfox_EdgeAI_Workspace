# 🧠 Thư viện AI & Model — LuckFox EdgeAI Workspace

> **Phạm vi:** Các thư viện/công cụ thường dùng trên **Ubuntu Host** để xử lý dataset, train, đánh giá và chuyển model cho các project hiện tại (`01_Face_Recognition`) và tương lai (YOLO, classification, detection...). **Không cần cài tất cả từ đầu.**

## 📦 1. Python — Dataset, Training & Evaluation

| Thư viện | Chức năng | Mức dùng | Ghi chú |
|---|---|---|---|
| `numpy` | Mảng/tensor, xử lý số liệu | **Cơ bản** | Thường được các thư viện AI dùng chung. |
| `opencv-python` (`cv2`) | Đọc ảnh, crop, resize, xử lý frame | **Cơ bản** | Chạy trên **laptop**, khác OpenCV-Mobile trên LuckFox. |
| `Pillow` (`PIL`) | Đọc/ghi, biến đổi ảnh | **Cơ bản** | Dùng cùng hoặc thay OpenCV ở bước chuẩn bị dữ liệu. |
| `tensorflow` / `keras` | Xây dựng, train, fine-tune CNN; lưu `.keras` | **Project hiện tại** | MobileNetV2 C2 (pretrained), sau đó C1 (`weights=None`). |
| `scikit-learn` | Confusion matrix, precision/recall, đánh giá | **Nên có** | Cần dữ liệu kiểm thử tách biệt. |
| `matplotlib` | Biểu đồ loss, accuracy và kết quả | **Nên có** | Dùng khi đánh giá model. |
| `pandas` | Quản lý nhãn, metadata, thống kê dataset | Tùy chọn | Hữu ích khi dataset lớn. |
| `albumentations` | Augmentation ảnh có nhãn | Tùy chọn | Phù hợp khi cần biến đổi ảnh/box nhất quán. |
| `torch` / `torchvision` | Training mô hình PyTorch | **Project tương lai** | Không bắt buộc cho pipeline Keras. |
| `ultralytics` | Huấn luyện/xuất YOLO | **Project tương lai** | Cần kiểm tra model/ops có chuyển được sang RKNN hay không. |

## 🔄 2. Export & Quantization (trên Ubuntu)

| Công cụ / thư viện | Chức năng | Khi nào cần? | Ghi chú |
|---|---|---|---|
| Export Keras → ONNX (`tf2onnx` hoặc cách xuất tương thích) | Đưa model TensorFlow/Keras về `.onnx` | Sau khi train | Chốt cách xuất theo phiên bản TensorFlow/Keras thực tế. |
| `onnx` | Kiểm tra cấu trúc, shape và metadata model | Khi export | Không thực hiện inference trên NPU. |
| `onnxruntime` | Chạy thử model ONNX trên laptop | Trước chuyển RKNN | So sánh output Keras ↔ ONNX. |
| **RKNN-Toolkit2** | Chuyển ONNX → `.rknn`, hiệu chuẩn lượng tử INT8 | Khi deploy | Cài **môi trường riêng**, chọn wheel/Python hỗ trợ đúng phiên bản và RV1106. |
| Calibration dataset | Ảnh đại diện phân bố đầu vào | Khi quantization | **Không phải thư viện**; phải dùng đúng preprocessing của model. |

> **Lưu ý:** Không giả định `pip install rknn-toolkit2` trong môi trường TensorFlow hiện tại là chạy được. Tài liệu LuckFox Pico mô tả cách cài RKNN-Toolkit2 theo **wheel và Python tương thích**; nên tạo một `venv`/Conda riêng cho bước chuyển đổi.

## 🔧 3. C++ / LuckFox Target (không cài bằng pip trên laptop)

| Thành phần | Công dụng | Nguồn |
|---|---|---|
| **OpenCV-Mobile** | Xử lý frame trên LuckFox (resize, crop, màu, vẽ box) | Gói c-p ARM tương thích; link từ CMake. |
| **RKMPI / RKAIQ / RGA** | Camera SC3336, ISP, multimedia và xử lý tăng tốc | LuckFox SDK + thư viện/firmware tương thích. |
| **RKNN Runtime (`librknnrt`/`librknnmrt` tùy SDK)** | Nạp `.rknn`, gọi NPU, lấy kết quả | Header + thư viện ARM của SDK/demo tương thích. |
| **CMake + Ninja + cross-toolchain** | C-p C++ cho RV1106 và link các thư viện trên | Toolchain từ `luckfox-pico`. |
| **FFmpeg / ffplay** | Thu dataset từ RTSP và kiểm tra stream trên Ubuntu | Gói hệ thống (`apt`), không phải thư viện AI. |

## ⚡ 4. Thiết lập nhanh cho `01_Face_Recognition`

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Face_Recognition
python3.11 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install numpy opencv-python pillow matplotlib scikit-learn tensorflow
```

Nếu đã cấu hình **NVIDIA GPU** và phiên bản TensorFlow phù hợp, có thể cân nhắc `tensorflow[and-cuda]` trong môi trường mới thay vì lệnh cuối. **Không cần cài lại** nếu thư viện đã hoạt động trong môi trường riêng hiện có.

Kiểm tra:

```bash
python -c "import cv2, tensorflow as tf; print('OpenCV:', cv2.__version__); print('TF:', tf.__version__); print('GPU:', tf.config.list_physical_devices('GPU'))"
```

**Quy trình chính:** `Dataset → OpenCV/Pillow → TensorFlow/Keras → ONNX → RKNN-Toolkit2 → .rknn` và `C++ + OpenCV-Mobile/RKMPI/RKNN Runtime → c-p → SCP → LuckFox`.

## 🔗 Tham khảo

- [TensorFlow — Pip installation](https://www.tensorflow.org/install/pip)
- [OpenCV — Python](https://pypi.org/project/opencv-python/)
- [LuckFox — RKNN trên RV1106](https://wiki.luckfox.com/Luckfox-Pico-RV1106/RKNN/)
- [LuckFox — OpenCV-Mobile](https://wiki.luckfox.com/Luckfox-Pico-RV1106/opencv-mobile/)
- [LuckFox — RKMPI](https://wiki.luckfox.com/Luckfox-Pico-RV1106/RKMPI/)