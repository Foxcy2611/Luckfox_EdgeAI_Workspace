<div align="center">

# Project 02 — Custom Object Detection

**YOLOv5n · SC3336 · LuckFox Pico Max (RV1106)**

![Status](https://img.shields.io/badge/Status-Dataset%20Preparation-orange?style=flat-square)
![YOLO](https://img.shields.io/badge/YOLO-v5n-00A5CF?style=flat-square)
![PyTorch](https://img.shields.io/badge/PyTorch-Training-EE4C2C?style=flat-square&logo=pytorch&logoColor=white)
![RKNN](https://img.shields.io/badge/RKNN-INT8-5C6BC0?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-RV1106-3949AB?style=flat-square)
![Camera](https://img.shields.io/badge/Camera-SC3336-607D8B?style=flat-square)
![C++](https://img.shields.io/badge/Runtime-C%2B%2B17-00599C?style=flat-square&logo=cplusplus&logoColor=white)

*Fine-tuning trên dữ liệu tự thu → Đo trên NPU → Tối ưu kiến trúc có chọn lọc.*

</div>

## 1. Giới thiệu

Dự án **không train YOLO từ đầu**. Chúng ta dùng **YOLOv5n pretrained trên COCO** làm điểm khởi tạo, tập trung học fine-tuning, cấu trúc Backbone/Neck/Head, ONNX/RKNN và tối ưu dựa trên phép đo phần cứng.

## 2. Hai phương án thực nghiệm

| | **Model A — Fine-tuned** | **Model B — Hardware-Aware** |
|---|---|---|
| Kiến trúc | YOLOv5n gốc | YOLOv5n chỉnh sửa **một thành phần có căn cứ** |
| Khởi tạo | Pretrained COCO | Tái sử dụng trọng số tương thích |
| Dữ liệu train | Dataset SC3336 | **Cùng dataset SC3336** |
| Mục tiêu | Baseline chính | Đo lợi ích của thay đổi trên RV1106 |
| Đánh giá | mAP, latency, FPS, RAM | Cùng tập test và điều kiện benchmark |

**Thứ tự thực hiện:** fine-tune **A** → export ONNX/RKNN INT8 → đo bottleneck → quyết định chỉnh architecture → fine-tune **B** → benchmark A/B. Ứng viên chỉnh sửa đầu tiên là **SiLU → ReLU**, nhưng chỉ thử nếu có căn cứ từ quá trình chuyển đổi/đo đạc, **không giả định B nhanh hoặc chính xác hơn A**.

```text
Ảnh SC3336 → Annotation → Dataset YOLO ─┬→ YOLOv5n gốc + pretrained → Fine-tune A
                                        │                           │
                                        │                     ONNX → RKNN INT8
                                        │                           │
                                        │                     Benchmark RV1106
                                        │                           │
                                        └→ Điều chỉnh có chọn lọc ←─┘
                                                 │
                                 Weights tương thích + Fine-tune B
                                                 │
                                       ONNX → RKNN INT8
                                                 │
                                          So sánh A / B
```

## 3. Dataset v1

**4 class, 7 vật thể vật lý đang có:**

| ID | Class | Biến thể |
|---:|---|---|
| 0 | `mouse` | Có dây, không dây |
| 1 | `cell_phone` | Android, iPhone |
| 2 | `bowl` | Bát trắng |
| 3 | `spoon` | Thìa ngắn, thìa dài |

**Pilot:** khoảng **100–140 ảnh được chọn lọc** từ SC3336, mỗi ảnh có thể chứa nhiều vật. Thu theo session, thay đổi góc nhìn/phông nền/ánh sáng, tách train–val–test theo **session hoặc bối cảnh** để tránh trùng lặp frame. Bổ sung ảnh theo lỗi thực tế, không coi 140 ảnh là đủ để tổng quát hóa.

➡️ **[Hướng dẫn thu thập, gán nhãn và chia dataset](Docs/Dataset_Collection.md)**

## 4. Cấu trúc project

```text
~/Luckfox_EdgeAI_Workspace/
├── Tools/
│   ├── yolov5/                    # Source train/export, ngoài project
│   └── .venv-rknn/                # Chỉ dùng convert/quantize RKNN
└── Project/
    └── 02_Custom_Object_Detection/
        ├── README.md
        ├── Dataset/
        │   ├── data.yaml
        │   ├── raw/                 # Ảnh gốc theo session
        │   ├── images/{train,val,test}/
        │   └── labels/{train,val,test}/
        ├── Scripts/               # capture_frames.py, check_dataset.py
        ├── Training/              # Train/config (bổ sung sau)
        ├── Models/                # .pt/.onnx/.rknn (bổ sung sau)
        ├── Src/                   # C++ inference (bổ sung sau)
        ├── Inc/                   # C++ headers (bổ sung sau)
        ├── Deploy/                # Script chạy LuckFox (bổ sung sau)
        ├── Results/               # Metrics và benchmark
        └── Docs/                  # Tài liệu chuyên đề
```

## 5. Quick Start — Giai đoạn dataset

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/02_Custom_Object_Detection

# Trích tối đa 30 frame từ video, cách nhau 3 giây.
python3 Scripts/capture_frames.py \
  --source /path/to/video.mp4 \
  --output Dataset/raw/session_01 \
  --interval-sec 3 --max-frames 30

# Sau khi annotation và chia các tập:
python3 Scripts/check_dataset.py --classes 4
```

Có thể dùng nguồn RTSP SC3336 thay video. `capture_frames.py` hiện **thu tự động**, **chưa có nút bấm chụp thủ công** và **không gán nhãn**. Chi tiết xem tài liệu dataset, cập nhật trường `path` trong `Dataset/data.yaml` theo máy trước khi train.

## 6. Các mốc thực hiện

- [ ] **P1 — Dataset:** thu ảnh SC3336, annotate 4 class, tách train/val/test và kiểm tra nhãn.
- [ ] **P2 — Model A:** phân tích YOLOv5n, fine-tune từ COCO pretrained, đánh giá trên test độc lập.
- [ ] **P3 — Deploy A:** export ONNX → RKNN INT8, tích hợp C++/RK_MPI/SC3336, đo từng stage.
- [ ] **P4 — Model B:** điều chỉnh kiến trúc dựa trên bottleneck/operator support, nạp weights tương thích và fine-tune có kiểm soát.
- [ ] **P5 — So sánh:** mAP, độ lệch INT8, latency NPU, FPS end-to-end, RAM, kết luận nên dùng A hay B.

> **Trạng thái hiện tại:** mới có cấu trúc project và script dữ liệu, chưa có số liệu huấn luyện hay FPS YOLO trên RV1106.

## 7. Tham khảo

- [YOLOv5 source & architecture](https://github.com/ultralytics/yolov5) · [YOLOv5n YAML](https://github.com/ultralytics/yolov5/blob/master/models/yolov5n.yaml)
- [Rockchip RKNN Model Zoo — YOLOv5](https://github.com/airockchip/rknn_model_zoo/tree/main/examples/yolov5)
- [Project roadmap 5 phần](../../Docs/EdgeAI_5_Project_Roadmap.md)