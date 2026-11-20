# 📷 Camera trên LuckFox Pico Max — Từ kết nối đến xử lý ảnh

> **Phạm vi:** LuckFox Pico Max (RV1106), Buildroot, sử dụng camera CSI SC3336. 

## 📑 Mục lục

- [1. Camera hoạt động như thế nào?](#1-camera-hoạt-động-như-thế-nào)
- [2. Kết nối và kiểm tra camera](#2-kết-nối-và-kiểm-tra-camera)
- [3. Xem hình ảnh qua RTSP](#3-xem-hình-ảnh-qua-rtsp)
- [4. Đưa frame vào chương trình C++](#4-đưa-frame-vào-chương-trình-c)
- [5. Thu thập và xử lý dữ liệu](#5-thu-thập-và-xử-lý-dữ-liệu)

## 1. Camera hoạt động như thế nào?

```text
Camera CSI (SC3336)
        |
        v
Driver + MIPI CSI + ISP      [tầng kernel / multimedia]
        |
        v
RKMPI (VI, xử lý frame)     [C/C++ lấy dữ liệu ảnh]
        |
        +--> VENC (H.264) --> RTSP --> VLC trên laptop
        |
        +--> OpenCV / preprocessing --> RKNN Runtime --> NPU
                                                   |
                                                   v
                                Class / face box / embedding
```

- **Driver + ISP:** nhận tín hiệu camera và xử lý ảnh (phơi sáng, màu, cân bằng trắng...).
- **RKMPI:** giao diện multimedia Rockchip giúp lấy frame, quản lý buffer và kết nối các khối xử lý/encode. Các API cụ thể phụ thuộc phiên bản SDK.
- **OpenCV / opencv-mobile:** thao tác ảnh như resize, crop, đổi hệ màu, vẽ bounding box.
- **RKNN:** suy luận mô hình `.rknn` trên NPU, OpenCV không tự động biến ứng dụng thành inference NPU.
- **RTSP:** phát video qua mạng, không phải điều kiện bắt buộc để chạy nhận diện.

**Phân biệt:** *Thu thập ảnh để tạo dataset* là một nhiệm vụ, *đọc camera liên tục để inference* là nhiệm vụ khác. Cả hai đều dùng chung đầu vào là frame.

## 2. Kết nối và kiểm tra camera

### 2.1. Lắp camera (khi có phần cứng)

1. **Tắt nguồn** LuckFox trước khi gắn hoặc tháo cáp CSI.
2. Mở khóa đầu nối nhẹ nhàng, lắp cáp đúng chiều, khóa lại.
3. Với **Pico Pro/Max**, tài liệu LuckFox hướng dẫn mặt tiếp xúc kim loại của cáp hướng về phía chip RV1106. Xác nhận theo đúng loại cáp/module thực tế.
4. Cấp nguồn lại, SSH vào board.

### 2.2. Kiểm tra driver và dịch vụ trên LuckFox

```sh
ssh root@172.32.0.93

# Chạy các lệnh sau trên LuckFox
uname -a
lsmod
ls /dev/video* /dev/media* 2>/dev/null
ls /userdata/
ps | grep -E 'rkipc|rkisp' | grep -v grep
```

- Tìm dấu hiệu driver camera (ví dụ `sc3336`), `video_rkisp`, `rockit` trong `lsmod`, nếu chúng được build dưới dạng module.
- Firmware hãng có thể tạo `rkipc.ini` trong `/userdata/` khi camera hoạt động. Không xem riêng một dấu hiệu là phép kiểm tra tuyệt đối.
- Nếu không thấy `/dev/video*`, kiểm tra driver/Device Tree/image/cáp trước khi kết luận camera hỏng.
- Nếu firmware hiện tại không có các công cụ kiểm tra như `v4l2-ctl`, **không cần cài chúng ngay**, hãy kiểm thử bằng ví dụ RKMPI trước.

> Địa chỉ `172.32.0.93` là IP USB RNDIS trong bản Buildroot đang sử dụng. Nếu SSH không vào được, làm lại `docs/Build_Image.md` phần cấu hình LAN ảo.

## 3. Xem hình ảnh qua RTSP

Với image Buildroot đã tích hợp `rkipc` và dịch vụ camera mặc định:

1. Giữ LuckFox đang chạy, nối USB RNDIS hoặc Ethernet để laptop và board có thể liên lạc.
2. Mở **VLC** trên Ubuntu → **Media → Open Network Stream**.
3. Nhập địa chỉ (ví dụ cấu hình mặc định):

```text
rtsp://172.32.0.93/live/0
```

4. Nếu có hình ảnh, ghi lại độ phân giải, FPS và độ trễ. Nếu không có, kiểm tra `rkipc`, driver, IP và firmware, **SSH được không có nghĩa camera đã hoạt động**.

Có thể kiểm tra RTSP từ host bằng VLC trước khi viết bất kỳ ứng dụng C++ nào.

## 4. Đưa frame vào chương trình C++

Khi muốn **tự lập trình pipeline**, cách thực dụng là dựa trên demo RKMPI/OpenCV chính thức rồi điều chỉnh từng bước, không viết driver camera từ đầu.

### 4.1. Lấy ví dụ chính thức trên Ubuntu

```bash
cd ~
git clone https://github.com/LuckfoxTECH/luckfox_pico_rkmpi_example.git
cd luckfox_pico_rkmpi_example

export LUCKFOX_SDK_PATH="$(realpath ../luckfox-pico)"
chmod +x build.sh
./build.sh
```

Chọn đúng **uClibc** và ví dụ tương ứng với Buildroot/SC3336 trong menu. Menu/tên ví dụ có thể khác theo commit. Trước khi chạy, đối chiếu tài liệu của repository và SDK đang dùng.

Ví dụ đáng đọc:

- `luckfox_pico_rtsp_opencv`: nhận frame, xử lý/vẽ bằng OpenCV rồi phát RTSP.
- `luckfox_pico_rtsp_retinaface`: phát hiện **vị trí khuôn mặt**, chưa xác định tên người.
- `luckfox_pico_rtsp_retinaface_osd`: hiển thị kết quả bằng OSD.

**Lưu ý phiên bản:** Tài liệu demo có thông báo thay đổi cách dùng VPSS trong các bản Rockit mới. Không sao chép cứng pipeline VPSS từ ví dụ cũ sang SDK mới mà chưa kiểm tra.

### 4.2. Triển khai bản demo bằng SCP

Sau khi biên dịch, bộ ví dụ có thư mục `install/` chứa **toàn bộ tài nguyên cần chạy**. Hãy gửi nguyên thư mục demo, không chỉ gửi file binary:

```bash
# Trên Ubuntu, thay <thu_muc_demo> bằng tên thực tế trong install/
scp -r install/<thu_muc_demo> root@172.32.0.93:/root/
```

Trên LuckFox:

```sh
# Dừng ứng dụng camera mặc định nếu đang chiếm camera.
# Lệnh này có thể làm ngừng stream RTSP mặc định.
RkLunch-stop.sh

cd /root/<thu_muc_demo>
ls -la
# Theo README đi kèm demo để chạy đúng binary và model.
```

**Không bật đồng thời hai chương trình cùng tranh quyền điều khiển camera.** Khi thử nghiệm xong, khởi động lại hoặc phục hồi dịch vụ mặc định theo cách firmware hỗ trợ.

### 4.3. Xử lý từng frame

```cpp
// Pseudocode: không phải mã C++ có thể build độc lập.
while (running) {
    Frame frame = camera_get_frame(),  // VI/RKMPI
    Image image = convert_to_rgb(frame),
    Image input = resize_or_letterbox(image, model_size),
    Prediction pred = inference(input), // RKNN, nếu có model
    display_or_send(pred),
    camera_release_frame(frame),       // Trả buffer lại cho RKMPI
}
```

Đối với camera thực, phải lưu ý **pixel format (NV12/RGB/BGR), kích thước, stride, sở hữu buffer và thời gian release**. Xử lý ảnh là pipeline, không nhất thiết phải đa luồng ngay từ đầu.

## 5. Thu thập và xử lý dữ liệu

- Có thể chụp ảnh và lưu trên laptop từ video RTSP, hoặc ghi frame bằng chương trình trên LuckFox.
- Dùng OpenCV trên laptop để crop, resize và kiểm tra ảnh, chỉ lưu frame hữu ích, không cần lưu toàn bộ video.
- **Dữ liệu dùng để huấn luyện** thường cần nhiều góc nhìn, ánh sáng, khoảng cách và người khác nhau.
- **Ảnh đăng ký danh tính (enrollment)** cho một mô hình nhận diện khuôn mặt **đã huấn luyện trước** có thể bắt đầu với 2–3 ảnh/người, đây là mẫu đối chiếu, **không phải** tập train từ đầu.
- Nên lưu ảnh có sự đồng ý của chủ thể, hạn chế đưa ảnh/embedding khuôn mặt lên Git công khai.