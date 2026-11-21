# LuckFox EdgeAI Workspace — Roadmap 5 dự án (Project 01–05)

> **Mục đích:** Từ triển khai AI có sẵn trên thiết bị biên → tự thu và gán nhãn dữ liệu → fine-tune → tự thiết kế mạng học sâu → thực nghiệm chuỗi thời gian có dữ liệu thiếu → nghiên cứu mô hình dự báo.  

> **Nền tảng đang dùng:** Ubuntu (Python dùng chung, GPU cho training), RKNN-Toolkit2 trong `~/Luckfox_EdgeAI_Workspace/Tools/.venv-rknn/`, LuckFox Pico Max (RV1106) + camera CSI SC3336, LuckFox SDK tại `~/luckfox-pico` (ngoài workspace).  

> **Nguyên tắc:** Mỗi project độc lập trong `~/Luckfox_EdgeAI_Workspace/Project/`. Chỉ đưa model lên RV1106 sau khi xác nhận các toán tử, định dạng input/output, phiên bản toolkit/runtime và ngân sách RAM/CMA tương thích. Không coi export ONNX/RKNN thành công là bằng chứng inference đúng.

## 1. Toàn cảnh

| Project | Tên thư mục | Trọng tâm | Mức độ gợi ý | Sản phẩm cuối |
|---|---|---|---|---|
| **01 — Hoàn thành pipeline chính** | `01_Single-Person_Face_Verification` | YuNet + SFace pretrained; RKNN INT8; camera RK_MPI; C++ | Đã thực hiện | Nhận diện và xác minh Person_A qua SC3336, preview trên Ubuntu |
| **02 — Ưu tiên tiếp theo** | `02_Custom_Object_Detection` | YOLOv5n, fine-tune dữ liệu tự gán nhãn, YOLO postprocess | Dễ → trung bình | Detect đồ vật trên SC3336; so sánh FP32 và RKNN INT8 |
| **03** | `03_Human_Activity_Recognition` | CNN1D, LSTM, GRU, TCN; thiết kế lớp | Trung bình | Phân loại hoạt động từ IMU và báo cáo so sánh mô hình |
| **04** | `04_Missing_Data_Time_Series` | GRU/GRU-D, BRITS; missingness mask & time gaps | Trung bình → khó | Phân tích hiệu quả khi chuỗi có dữ liệu thiếu |
| **05** | `05_Time_Series_Forecasting` | TCN, Transformer/PatchTST, baseline dự báo | Khó | Mô hình dự báo đa biến, biểu đồ sai số và thử nghiệm edge nếu khả thi |

**Trình tự đề xuất:** 01 → 02 → 03 → 04 → 05. Project 03–05 **không phụ thuộc bắt buộc** vào LuckFox; ưu tiên hiểu model trên Ubuntu, sau đó mới xem khả năng triển khai edge.

## 2. Nguyên tắc chung áp dụng cho cả 5 project

1. **Đặt bài toán trước khi chọn model:** input, output, nhãn, ứng dụng, metric và điều kiện chạy.
2. **Phân chia dữ liệu tránh rò rỉ (data leakage):** ảnh/video cùng phiên thu nên nằm cùng tập; dữ liệu IMU chia theo người; chuỗi thời gian forecasting chia theo mốc thời gian.
3. **Có baseline và tập test riêng:** so sánh với phương án đơn giản; không chọn ngưỡng trên tập test.
4. **Ghi lại thí nghiệm:** seed, phiên bản thư viện, commit model, cấu hình input, biểu đồ loss/metric, checkpoint, số lượng sample và cấu hình phần cứng.
5. **Tách quy trình:** `Dataset/` → `Training/`/`Scripts/` → `Models/` → `Src/` → `Deploy/` → `Results/` → `Docs/`.
6. **Đo cả độ chính xác lẫn hiệu năng:** accuracy/F1 hoặc mAP, kích thước file, RAM/CMA, thời gian tiền xử lý, inference, hậu xử lý và FPS end-to-end.
7. **Không ép mọi model chạy NPU:** GRU-D/Transformer custom graph có thể không được RKNN Runtime 1.6.0 hỗ trợ; CPU hoặc Ubuntu là một kết quả hợp lệ trong giai đoạn học kiến trúc.

## 3. Project 01 — Single-Person Face Verification ✅

### Mục tiêu và cách tiếp cận
- Camera **SC3336** → khung RGB → **YuNet RKNN** (bbox + 5 landmarks) → căn chỉnh **112×112** → **SFace RKNN** (embedding **128D**) → cosine similarity với `person_a.bin` → `VERIFIED/UNKNOWN`.
- Enrollment bằng Python/OpenCV trên Ubuntu; model pretrained ONNX → RKNN INT8 qua Toolkit2; inference thật bằng C++/RK_MPI trên RV1106.
- Preview TCP lên viewer Python (chỉ hiển thị, **không** chạy AI trên Ubuntu).

### Kết quả đã đo
- Bản resize CPU cũ: ~**142 ms**; sau tối ưu: **~9–10 ms**.
- YuNet (gồm copy + NPU + unpack theo log hiện tại): **~46–47 ms**; decode ~**1,1 ms**.
- **~14–14,5 FPS chỉ ở chế độ detect-only, không preview** trong các log đã đo; không mặc định ghi con số này là FPS full YuNet + SFace.
- Đã triển khai CMake Presets + Ninja, SCP tới LuckFox, chạy `run_camera.sh` và viewer Ubuntu.

### Kiến thức thu được
C++ cross-compile; RKMPI/ISP/NV12; RKNN Runtime; đọc tensor; calibration INT8; resize CPU; thiết kế pipeline và debug vòng đời camera/ISP.

### Điểm nên giữ lại để tái sử dụng
- **Tham khảo ý tưởng** quản lý camera, xử lý NV12, preview, CMake; **không copy nguyên project 01 rồi đổi tên** vì YOLO khác shape, output và NMS.
- Các script trong `Docs/CPP_Deploy_LuckFox.md` của Project 01 là mẫu quy trình 2 terminal (Ubuntu và SSH LuckFox).

## 4. Project 02 — Custom Object Detection (YOLOv5n) ⭐

### Câu hỏi kỹ thuật
1. Có thể tự tạo dataset nhỏ, gán nhãn đúng chuẩn YOLO và fine-tune được mAP tốt trên ảnh **chưa từng xuất hiện trong training** không?
2. Model **YOLOv5n** export sang ONNX/RKNN INT8 có tương thích **RV1106 + Runtime 1.6.0** không?
3. Sau khi giảm kích thước/tối ưu hậu xử lý, có thể nhận diện đồ vật từ SC3336 với độ trễ chấp nhận được không?

### Phạm vi v1 (khuyến nghị)
- **3 class đầu tiên:** `phone`, `mouse`, `cup` (dễ thu ảnh và dễ gán nhãn; cả ba đã có đại diện trong COCO — giai đoạn này học **custom data + fine-tuning**, chưa phải nghiên cứu novel class).
- **Dữ liệu thử nghiệm:** khởi đầu 300–600 ảnh *đa dạng thực sự* trên nhiều phiên thu, khoảng cách, phông nền và ánh sáng; có ảnh nhiều đồ vật cùng lúc và ảnh âm tính không có 3 class.
- Có thể thu từ SC3336 qua RTSP mặc định của board, hoặc từ video trên Ubuntu/điện thoại; gán nhãn với **CVAT** hoặc công cụ tương đương.
- Chia train/val/test **theo phiên thu**, ví dụ 70/15/15 (đề xuất, không phải quy tắc cứng); tránh để frame gần nhau lọt vào tập test.

### Kiến trúc
`SC3336 → RK_MPI → NV12/RGB → letterbox → YOLOv5n RKNN → decode heads → NMS → boxes + class + confidence → preview`.

### Pipeline phát triển
1. **Baseline:** lấy YOLOv5n pretrained COCO, chạy ảnh tĩnh trên Ubuntu, xác nhận input/output và nhãn.
2. **Dataset:** chốt class; thu ảnh; annotation; kiểm tra label YOLO `class x_center y_center width height` chuẩn hóa `[0,1]`.
3. **Fine-tune:** train trên Ubuntu GPU; lưu `best.pt`, cấu hình hyperparameters và biểu đồ train/val.
4. **Đánh giá:** precision/recall, mAP@0.5, mAP@0.5:0.95, confusion matrix và ảnh lỗi; so sánh với pretrained baseline.
5. **Export:** `best.pt → best.onnx` với batch=1, shape cố định nếu cần; kiểm thử ONNX trước khi chuyển đổi.
6. **RKNN:** dùng `.venv-rknn` **riêng cho convert**, calibration bằng ảnh đại diện; xác nhận graph và số output. Không dùng chung model postprocess 80 class cho model 3 class.
7. **C++:** áp dụng RK_MPI + RKNN + postprocess YOLO (số class = 3), preview; đo inference vs preprocessing vs NMS riêng.
8. **Demo:** phát hiện cùng lúc nhiều vật trên bàn; README, log, video ngắn và kết quả so sánh.

### Chú ý tương thích RV1106
- LuckFox có **YOLOv5 demo chính thức**, vì vậy có đường kiểm chứng phần cứng trước khi đưa model tự train.
- **YOLOv5n không mặc nhiên tương thích với demo YOLOv5s/relu.** Nếu bản `n` lỗi chuyển đổi/đầu ra, ưu tiên thử **model ONNX trong ví dụ RV1106** để xác nhận toàn pipeline rồi mới tùy biến v5n; không tự ý đổi phiên bản Runtime trên board.
- File nhãn, số class và logic decode phải đồng bộ với checkpoint. Tài liệu LuckFox cảnh báo segmentation fault khi cấu hình class không khớp.

### Tiêu chí hoàn thành
- Dataset có nguồn gốc, class definition, nhãn đã kiểm tra, test tách độc lập.
- Train có checkpoint tái lập và metric mAP cùng ảnh minh họa lỗi.
- Model RKNN load + suy luận đúng trên RV1106; không chỉ là convert thành công.
- Demo SC3336 có bbox/tên class/score và báo cáo FPS thực tế khi bật preview.
- Ghi nhận giới hạn: đa dạng vật thể, điều kiện ánh sáng, tốc độ và kích thước model.

**Tài liệu tiếp theo:** `Project/02_Custom_Object_Detection/README.md`.

## 5. Project 03 — Human Activity Recognition (LSTM/GRU/TCN)

### Bài toán
Phân loại hoạt động từ dữ liệu IMU 3 trục gia tốc + 3 trục con quay. **Không cần đeo STM32 + MPU6050 để bắt đầu.** Trước hết dùng **UCI HAR** (30 người, 6 hoạt động, 50 Hz; cửa sổ 128 mẫu, 2,56 giây) rồi mới chọn thử phần cứng.

### Thí nghiệm khuyến nghị
- Dữ liệu: `(batch, timesteps=128, features=6)` nếu dùng sáu kênh quán tính phù hợp; thống nhất cách chọn 6 kênh và chuẩn hóa.
- Baselines: feature statistics + Random Forest, CNN1D.
- Models tự xây dựng: `LSTM(32)`, `GRU(32)`, `CNN1D+GRU`, **TCN**.
- Giai đoạn học sâu hơn: tự viết **GRU Cell** bằng các phép nhân ma trận, update/reset gates; so sánh với `keras.layers.GRU`.
- Đánh giá: macro-F1, per-class recall, confusion matrix, tham số, thời gian infer CPU; split theo **subject**, không split ngẫu nhiên các cửa sổ chồng nhau.
- Demo phần cứng (tùy chọn): gắn MPU6050 vào hộp đeo thắt lưng; giữ vị trí/trục và cách lấy mẫu tương đồng dataset.

### Khó khăn cần chủ động xử lý
- Dữ liệu công khai có thể khác cảm biến thật (domain shift).
- Truyền dữ liệu thời gian thực cần sliding buffer + sample-rate ổn định.
- Các recurrent op và custom cell không bảo đảm chuyển thành công sang RKNN 1.6.0; có thể chạy CPU trước.

**Hoàn thành khi:** có bảng so sánh ít nhất 3 kiến trúc, đánh giá theo subject và bản giải thích GRU/LSTM cell.

## 6. Project 04 — Missing-Data Time Series (GRU-D/BRITS)

### Bài toán
Dữ liệu cảm biến thiếu mẫu, thời điểm không đều, nhiều feature vắng mặt. Phân biệt rõ **dự đoán nhãn** và **điền dữ liệu thiếu** (imputation); hai nhiệm vụ dùng metric khác nhau.

### Mô hình nghiên cứu
- Baseline: forward-fill/mean imputation + GRU, CNN1D.
- **GRU-D (GRU with Decay):** dùng giá trị quan sát, `mask` và khoảng thời gian từ lần đo hợp lệ (`delta`) để điều chỉnh input/hidden state bằng decay.
- **BRITS:** tham khảo cho bài toán imputation hai chiều nếu đủ thời gian và dữ liệu.

### Kịch bản thử nghiệm
- Có bộ dữ liệu chuỗi thời gian đầy đủ hoặc tương đối đầy đủ; giữ một test set riêng.
- Mô phỏng missingness: MCAR ở tỷ lệ 10/20/40%; thêm block-missing và missingness có tính hệ thống. **Không coi tất cả các dạng missing là giống nhau**.
- Đối với imputation: MAE/RMSE **chỉ trên vị trí bị che có ground truth**; đối với classification: macro-F1/AUROC nếu phù hợp.
- Nghiên cứu tác động của mask và delta bằng ablation: `GRU`, `GRU + mask`, `GRU + mask + delta`, `GRU-D`.

### Thách thức triển khai
GRU-D custom cell có thể export thành graph phức tạp hoặc unsupported op trên NPU; mục tiêu cốt lõi là **tự viết layer và thực nghiệm trên Ubuntu**, không hứa chắc chạy trên LuckFox.

**Hoàn thành khi:** tái lập một baseline, mô tả rõ cơ chế decay và chứng minh hiệu quả/giới hạn trong ít nhất hai dạng dữ liệu thiếu.

## 7. Project 05 — Time-Series Forecasting (TCN/Transformer/PatchTST)

### Bài toán
Dự báo giá trị trong tương lai từ cửa sổ lịch sử của nhiều cảm biến, ví dụ: nhiệt độ, độ ẩm, áp suất (hoặc một bộ dữ liệu công khai phù hợp). Không tự gắn nhãn tương lai; bài toán là regression.

### Bộ thí nghiệm
- Baseline: last-value/persistence, moving average, có thể thêm linear regression.
- `CNN1D/TCN`: hướng tối ưu gọn, phù hợp nghiên cứu triển khai edge.
- `GRU`: đối chứng recurrent.
- `Transformer` hoặc `PatchTST`: mô hình attention để so sánh khả năng nắm bắt phụ thuộc dài hạn.
- Input ví dụ: 120 thời điểm gần nhất; dự báo 10 thời điểm tiếp theo. Đây là đề xuất, phải điều chỉnh theo đơn vị lấy mẫu thực tế.

### Đánh giá
- Chia tập **theo thời gian**, không đảo trộn tương lai vào quá khứ.
- Đánh giá MAE/RMSE, theo từng horizon và từng biến; so với persistence baseline.
- Kiểm tra hiệu ứng scaling, seasonality, drift và độ dài context; báo cả tốc độ và RAM.
- Tùy chọn triển khai Edge: thử **TCN/Conv1D trước**; Transformer/PatchTST có thể không phù hợp với SDK/NPU hiện tại.

**Hoàn thành khi:** có baseline, ít nhất 3 kiến trúc, sai số theo horizon và kết luận model nào đáng triển khai thực tế.

## 8. Thứ tự milestone tổng quát

| Giai đoạn | Đầu ra cần có |
|---|---|
| P0 — Define | README bài toán, định nghĩa class/label, metric, nguồn dữ liệu |
| P1 — Dataset | Dữ liệu có tổ chức, nhãn và split chống leakage |
| P2 — Baseline | Chạy một model đơn giản có log và metric |
| P3 — Improve | So sánh kiến trúc/hyperparameters; ghi kết quả |
| P4 — Export | Lưu model + config preprocessing/postprocessing |
| P5 — Edge (nếu khả thi) | Model RKNN/CPU, C++ runtime, đo end-to-end |
| P6 — Document | README, kết quả test, limitation, cách tái lập |

## 9. Cấu trúc workspace đề xuất

```text
~/Luckfox_EdgeAI_Workspace/
├── Docs/
│   └── EdgeAI_5_Project_Roadmap.md  # Tài liệu chung này
├── Tools/
│   ├── .venv-rknn/                  # RKNN Toolkit2 (đã có)
│   └── yolov5/                     # Clone mã nguồn YOLOv5 khi bắt đầu Project 02
└── Project/
    ├── 01_Single-Person_Face_Verification/
    ├── 02_Custom_Object_Detection/
    ├── 03_Human_Activity_Recognition/
    ├── 04_Missing_Data_Time_Series/
    └── 05_Time_Series_Forecasting/
```

Không cần tạo đồng loạt thư mục 03–05 cho đến khi thực sự bắt đầu; tránh project rỗng.

## 10. Tài liệu tham khảo và điểm kiểm chứng

- LuckFox — RKNN, YOLOv5 trên RV1106: https://wiki.luckfox.com/Luckfox-Pico-Pro-Max/RKNN/
- LuckFox — RKNN troubleshooting (đặc biệt postprocess class mismatch): https://wiki.luckfox.com/Luckfox-Pico-RV1106/FAQ/RKNN/
- LuckFox RKNN example: https://github.com/LuckfoxTECH/luckfox_pico_rknn_example
- Ultralytics — YOLOv5 custom dataset/training: https://docs.ultralytics.com/yolov5/tutorials/train-custom-data/
- UCI HAR (dataset chính thức): https://archive.ics.uci.edu/dataset/240/human+activity+recognition+using+smartphones
- Che et al. (2018), GRU-D: https://www.nature.com/articles/s41598-018-24271-9

> **Trạng thái roadmap:** Project 01 đã hoàn thành demo chính; **bắt đầu Project 02**. Các mục tiêu của Project 02–05 là kế hoạch/tiêu chí đề xuất, không phải kết quả đã đạt.
