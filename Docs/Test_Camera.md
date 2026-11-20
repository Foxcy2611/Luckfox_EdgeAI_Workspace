# 📷 Test Camera — LuckFox Pico Max (SC3336)

> Kiểm tra camera được Linux nhận diện, stream RTSP và lấy ảnh/video test. Các lệnh RTSP sử dụng IP LuckFox hiện tại: `172.32.0.93`.

## 1. Kiểm tra kernel nhận SC3336 (trên LuckFox)

```bash
dmesg | grep -i sc3336
```

Log thực tế đã ghi nhận:

```text
[root@luckfox root]# dmesg | grep -i sc3336
[    2.614927] sc3336 4-0030: driver version: 00.01.01
[    2.614999] sc3336 4-0030: Failed to get reset-gpios
[    2.615051] sc3336 4-0030: could not get default pinstate
[    2.615061] sc3336 4-0030: could not get sleep pinstate
[    2.615085] sc3336 4-0030: supply avdd not found, using dummy regulator
[    2.615290] sc3336 4-0030: supply dovdd not found, using dummy regulator
[    2.615350] sc3336 4-0030: supply dvdd not found, using dummy regulator
[    2.633091] sc3336 4-0030: Detected OV00cc41 sensor
[    2.770558] rockchip-csi2-dphy csi2-dphy0: dphy0 matches m00_b_sc3336 4-0030:bus type 5
[root@luckfox root]# ps | grep rkipc
  845 root     rkipc -a /oem/usr/share/iqfiles
 1156 root     grep rkipc
```

- `Detected OV00cc41 sensor` và `dphy0 matches ...`: driver phát hiện sensor và ghép được cấu hình CSI/DPHY; **cần xem hình RTSP mới xác nhận camera thực sự truyền ảnh ổn**.
- Các cảnh báo `reset-gpios` / `dummy regulator` không nhất thiết gây lỗi trên cấu hình đang dùng. Nếu không có hình, vẫn phải kiểm tra lại cáp, nguồn, cấu hình và log.

## 2. Kiểm tra dịch vụ stream mặc định `rkipc` (trên LuckFox)

```bash
ps | grep '[r]kipc'
```

Kết quả trong lần thử:

```text
845 root     rkipc -a /oem/usr/share/iqfiles
```

## 3. Xem camera qua RTSP (trên Ubuntu)

```bash
sudo apt install -y ffmpeg
ffplay -rtsp_transport tcp -fflags nobuffer -flags low_delay rtsp://172.32.0.93/live/0
```

Nếu thấy hình ảnh liên tục, không đứng hình bất thường thì phần stream đã hoạt động. Nhấn `q` để thoát.

## 4. Một số kiểm tra bổ sung (trên Ubuntu)

**Xem codec, độ phân giải và FPS khai báo của stream:**

```bash
ffprobe -v error -rtsp_transport tcp -select_streams v:0 \
  -show_entries stream=codec_name,width,height,avg_frame_rate \
  -of default=noprint_wrappers=1 rtsp://172.32.0.93/live/0
```

**Chụp một frame về laptop (ảnh test/dataset):**

```bash
ffmpeg -y -rtsp_transport tcp -i rtsp://172.32.0.93/live/0 -frames:v 1 snapshot.jpg
```

**Ghi lại 10 giây video để kiểm tra:**

```bash
ffmpeg -y -rtsp_transport tcp -i rtsp://172.32.0.93/live/0 -t 10 -c:v copy -an test_cam.mp4
```

> Các file `snapshot.jpg`, `test_cam.mp4` được lưu ở thư mục terminal Ubuntu đang đứng, không phải trên LuckFox.

## 5. Ghi chú cho project Face Recognition tiếp theo

- Camera mặc định đang do `rkipc` sử dụng. Khi chạy C++ riêng qua RKMPI, cần dừng ứng dụng mặc định trước (demo LuckFox dùng `RkLunch-stop.sh`), tránh tranh chấp camera.
- Mục tiêu project: **Camera → RetinaFace (phát hiện) → Face embedding (so khớp mẫu) → bounding box xanh = Known / đỏ = Unknown → RTSP**.
- **2–3 ảnh/người** chỉ phù hợp đăng ký thử nghiệm với model đã huấn luyện sẵn; ngưỡng Known/Unknown phải được kiểm thử và cần sự đồng ý của người cung cấp ảnh.

Tham khảo: [LuckFox RKMPI Examples: OpenCV, RetinaFace, RTSP](https://github.com/LuckfoxTECH/luckfox_pico_rkmpi_example/tree/kernel-5.10.160).
