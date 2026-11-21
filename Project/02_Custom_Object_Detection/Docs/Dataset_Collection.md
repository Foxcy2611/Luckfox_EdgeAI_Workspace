# Dataset Collection — Project 02 (SC3336)

> **Mục tiêu:** chuẩn bị dataset YOLO detection v1 để fine-tune **Model A** và **Model B** trên **cùng dữ liệu**. Bắt đầu với **100–140 ảnh chất lượng**; không cần thu hàng trăm ảnh cho mỗi class ngay từ đầu.

## 1. Đặc tả đối tượng và quy tắc nhãn

| ID | Tên class (`data.yaml`) | Vật thể mẫu | Quy tắc |
|---:|---|---|---|
| 0 | `mouse` | Chuột có dây, không dây | Gán **thân chuột**; không kéo bbox theo toàn bộ dây |
| 1 | `cell_phone` | Android, iPhone | Cùng một class cho mọi điện thoại |
| 2 | `bowl` | Bát trắng | Gán khung bao bát, kể cả khi bên trong có thìa |
| 3 | `spoon` | Thìa ngắn, dài | Gán **từng chiếc thìa** nếu phần nhìn thấy đủ nhận biết |

**Quy ước:**

- Mỗi vật thể mục tiêu nhìn thấy rõ được gán **một bounding box**; một ảnh có thể có nhiều bbox và nhiều class.
- Nếu bát chứa thìa, gán **cả `bowl` và `spoon`** khi nhìn thấy được; không gộp chúng thành một đối tượng.
- Với vật bị che khuất, bbox bao phần vật **nhìn thấy được** và giữ một quy tắc nhất quán trên toàn dataset; không đoán phần khuất quá mức. Loại ảnh cực mơ hồ khỏi pilot.
- Vật thể ngoài 4 class (bàn phím, sách, tai nghe...) **không gán nhãn**; có thể giữ một số ảnh chỉ có vật lạ để thử false positive. Ảnh negative phải bảo đảm không chứa vật thể mục tiêu bị bỏ nhãn.
- Không phân biệt Android/iPhone, chuột có dây/không dây, thìa dài/ngắn thành class riêng trong v1.

**Giới hạn dataset:** hiện chỉ có *một bát trắng*, nên mô hình có nguy cơ nhận diện tốt riêng chiếc bát đó nhưng kém với loại bát khác. Đây là giới hạn cần nêu khi đánh giá, không che giấu bằng cách nhân bản cùng góc chụp.

## 2. Số lượng ảnh và kế hoạch thu

**Đề xuất pilot: 120 ảnh** (có thể dừng ở 100 hoặc tăng tới 140 nếu cần). Không cần cân số ảnh theo từng class vì một frame có thể chứa 2–4 đối tượng; thay vào đó, kiểm tra số **instance/bbox** và độ đa dạng theo class.

| Session | Bối cảnh | Ảnh mục tiêu |
|---|---|---:|
| `session_01` | Từng vật thể, nền bàn và ánh sáng ổn định | 30 |
| `session_02` | Thay góc nhìn, hướng đặt, khoảng cách | 30 |
| `session_03` | Nhiều vật chung khung, thìa trong/ngoài bát | 30 |
| `session_04` | Nền và ánh sáng khác, che khuất nhẹ, vật lạ | 30 |
| **Tổng** | | **120** |

**Không chụp 30 ảnh liên tục cùng một góc.** Sau mỗi ảnh hoặc đoạn video, thay đổi ít nhất một yếu tố có nghĩa: bố cục, hướng vật, khoảng cách, ánh sáng hoặc phông nền. Ưu tiên ảnh rõ, không cháy sáng, vật thể không quá nhỏ.

Nếu dùng SC3336 cố định, có thể di chuyển vật thể và nền thay vì di chuyển camera. Với thìa phản chiếu và bát trắng, nên thử cả nền sáng lẫn nền tối, tránh chỉ có một điều kiện chiếu sáng.

## 3. Thu ảnh bằng SC3336 hoặc từ video

`Scripts/capture_frames.py` hiện đọc **file video, RTSP URL hoặc webcam**, và **lưu frame tự động theo khoảng thời gian**. Script **không điều khiển nút chụp của camera, không có phím `S` để chụp một ảnh, và không tự annotation**.

### 3.1. Chuẩn bị camera

1. Kết nối LuckFox Pico Max và SC3336 như ở Project 01.
2. Dùng luồng RTSP do hệ thống camera trên LuckFox cung cấp (URL tùy cấu hình thực tế).
3. **Không chạy đồng thời** ứng dụng C++ FaceVerification đang chiếm camera với dịch vụ RTSP; dừng ứng dụng chiếm camera trước khi thu RTSP.
4. Kiểm tra nguồn phát bằng VLC hoặc OpenCV trước khi bắt đầu lưu dữ liệu.

Nếu không muốn dùng RTSP trực tiếp, có thể quay video rồi trích frame trên Ubuntu. Điều này không thay đổi định dạng dataset YOLO về sau.

### 3.2. Trích ảnh từ video đã quay

Chạy từ thư mục gốc Project 02:

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/02_Custom_Object_Detection

python3 Scripts/capture_frames.py \
  --source /path/to/session_01.mp4 \
  --output Dataset/raw/session_01 \
  --interval-sec 3 \
  --max-frames 30
```

`--interval-sec 3` chọn các frame cách nhau tối thiểu 3 giây **theo thời gian video**, khi video có FPS hợp lệ. Đổi khoảng cách nếu clip quá ngắn hoặc nhiều cảnh chuyển động.

### 3.3. Trích ảnh trực tiếp từ RTSP

```bash
python3 Scripts/capture_frames.py \
  --source 'rtsp://<LUCKFOX_RTSP_URL>' \
  --output Dataset/raw/session_02 \
  --interval-sec 3 \
  --max-frames 30 \
  --show
```

`--show` bật cửa sổ xem; **`Q` hoặc `Esc` để dừng**, không phải để chụp một frame. Với RTSP, khoảng thời gian dựa trên đồng hồ máy chạy Python. Nên quan sát và thay đổi bố cục trong quá trình thu.

Các tham số đang có: `--source`, `--output`, `--interval-sec`, `--max-frames`, `--min-sharpness`, `--show`. `--min-sharpness 0` (mặc định) **tắt lọc mờ**. Nếu dùng lọc mờ, cần tự thử một ngưỡng hợp lý trên ảnh thật.

> **Muốn chụp từng ảnh bằng phím `S`?** Đó là chức năng **chưa có** trong starter. Có thể bổ sung chế độ `manual` vào script ở bước sau; tài liệu này không giả định nó đã được triển khai.

## 4. Sàng lọc trước khi gán nhãn

Thư mục `Dataset/raw/session_XX/` chỉ chứa ảnh thu thập ban đầu. Trước annotation:

1. Bỏ frame mờ, vật thể cắt mất quá nhiều, ảnh lỗi hoặc bị cháy sáng.
2. Bỏ bớt frame gần như trùng nhau; không cần giữ nhiều góc chụp cách nhau vài pixel.
3. Kiểm tra mỗi class có ảnh riêng lẻ, nhiều vật và ít nhất một số tình huống khó.
4. Giữ nguyên ảnh gốc trong `raw/`; làm việc trên bản copy khi xuất dataset.
5. Ghi lại session, điều kiện ánh sáng, nhóm vật thể và nguồn camera/video bằng một ghi chú ngắn, để sau này phân tích lỗi.

## 5. Annotation — định dạng YOLO detection

Có thể dùng **CVAT** hoặc công cụ gán bbox tương đương. Tạo đúng bốn nhãn theo thứ tự ID ở Mục 1, rồi xuất **YOLO detection**.

Mỗi ảnh `example_001.jpg` tương ứng với một file `example_001.txt`. Mỗi dòng trong file nhãn có dạng:

```text
<class_id> <x_center> <y_center> <width> <height>
```

Các tọa độ bbox được **chuẩn hóa về [0, 1]**, không phải pixel. Ví dụ minh họa:

```text
0 0.420 0.530 0.210 0.180
3 0.680 0.600 0.140 0.070
```

Nghĩa là ảnh có một `mouse` và một `spoon` (ví dụ tọa độ, không phải ảnh thực tế).

**Lưu ý:** Nếu một ảnh không chứa bất kỳ đối tượng mục tiêu nào, có thể dùng nhãn rỗng/không có file `.txt` theo công cụ train; khi chạy `check_dataset.py`, ảnh thiếu `.txt` sẽ được cảnh báo và coi là **negative image**.

## 6. Chia train / validation / test

Với **120 ảnh pilot**, có thể hướng tới khoảng **84 train / 18 val / 18 test (70/15/15)**, nhưng ưu tiên **tách theo session/cảnh** hơn việc ép chính xác tỷ lệ.

**Tránh data leakage:** hai frame gần giống nhau, hoặc cùng chuỗi cảnh vừa quay, không được chia vào cả train và test. Nên đặt riêng một số bố cục/phông nền cho test. Kiểm tra cả sự xuất hiện của 4 class ở val/test; nếu một class quá ít, thu bổ sung thay vì đưa frame gần trùng sang nhiều split.

```text
Dataset/
├── raw/
│   ├── session_01/
│   ├── session_02/
│   ├── session_03/
│   └── session_04/
├── images/
│   ├── train/
│   ├── val/
│   └── test/
├── labels/
│   ├── train/
│   ├── val/
│   └── test/
└── data.yaml
```

Tên stem của ảnh và nhãn phải trùng nhau; ví dụ `images/train/s01_0001.jpg` ↔ `labels/train/s01_0001.txt`. **Nên đặt tiền tố session** khi copy để không đè các file đều mang tên `frame_0001.jpg`.

### Cấu hình `Dataset/data.yaml`

```yaml
# Cập nhật path thành đường dẫn tuyệt đối đúng máy trước khi train.
path: /home/<USER>/Luckfox_EdgeAI_Workspace/Project/02_Custom_Object_Detection/Dataset
train: images/train
val: images/val
test: images/test
nc: 4
names:
  0: mouse
  1: cell_phone
  2: bowl
  3: spoon
```

Trong file thực tế, thay `<USER>` bằng người dùng Ubuntu và bảo đảm `path` trỏ đúng thư mục chứa `images/` và `labels/`. Hai model A/B **phải dùng cùng định nghĩa class**.

## 7. Kiểm tra dataset sau annotation

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/02_Custom_Object_Detection

python3 Scripts/check_dataset.py --classes 4
```

Script kiểm tra: nhãn không có ảnh, class ID ngoài giới hạn, bbox sai tọa độ, ảnh trùng **từng byte** giữa các split, và cảnh báo ảnh thiếu file nhãn. Script **không phát hiện near-duplicate** và **không kiểm tra bbox có ôm đúng vật thể**; vẫn phải mở một số ảnh/nhãn để soát trực quan.

Sau khi có dataset, có thể bắt đầu fine-tune **Model A**. Chỉ cân nhắc thu thêm ảnh khi đánh giá trên cảnh mới cho thấy lỗi rõ ràng (đồ vật quá nhỏ, phản chiếu, che khuất, nền lạ...).

## 8. Checklist trước khi training

- [ ] Đã khóa đúng **4 class / ID** ở cả annotation và `data.yaml`.
- [ ] Ảnh có đủ chuột có dây/không dây, Android/iPhone, bát trắng, thìa ngắn/dài.
- [ ] Có ảnh một vật, nhiều vật, thìa trong bát và vật lạ không thuộc class.
- [ ] Đã lọc ảnh mờ/ảnh gần trùng, giữ ảnh gốc theo session.
- [ ] Train/val/test tách theo cảnh; có đủ class ở val/test.
- [ ] Các file YOLO `.txt` khớp tên ảnh và bbox được kiểm tra bằng mắt.
- [ ] `python3 Scripts/check_dataset.py --classes 4` không còn lỗi.
- [ ] `Dataset/data.yaml` có `path` đúng với máy Ubuntu.

---

**Đầu ra của giai đoạn này:** `Dataset/images/`, `Dataset/labels/` và `Dataset/data.yaml` hợp lệ, sẵn sàng dùng chung để fine-tune và so sánh Model A/B.
