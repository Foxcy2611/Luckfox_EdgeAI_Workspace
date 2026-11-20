# Thu dataset Person_A từ camera LuckFox

**Mục tiêu:** ~200 ảnh gốc (`5 session × 40 ảnh`), thu trực tiếp từ RTSP của SC3336 trên Ubuntu. **Không cần SCP ảnh từ board.**

## 1. Chuẩn bị

Mở **2 terminal** trên Ubuntu. Đảm bảo LuckFox đang chạy RTSP tại `rtsp://172.32.0.93/live/0` và máy Ubuntu có `ffmpeg`, `ffplay`.

Từ **thư mục gốc Project 01**:

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification
mkdir -p Dataset/Person_A/Session_{1..5}
```

## 2. Terminal 1 — Xem camera để căn mặt

```bash
ffplay -rtsp_transport tcp \
  -fflags nobuffer -flags low_delay \
  rtsp://172.32.0.93/live/0
```

Giữ cửa sổ video mở để kiểm tra vị trí khuôn mặt, ánh sáng và độ nét.

## 3. Terminal 2 — Lưu ảnh tự động

Ví dụ thu **Session_1**: 40 giây, **1 ảnh/giây**, JPEG chất lượng cao.

```bash
ffmpeg -rtsp_transport tcp \
  -i rtsp://172.32.0.93/live/0 \
  -ss 5 \
  -t 40 \
  -vf "fps=1" \
  -q:v 2 \
  Dataset/Person_A/Session_1/img_%04d.jpg
```

Với các phiên còn lại, chỉ đổi `Session_1` thành `Session_2`, `Session_3`, `Session_4`, `Session_5`.

> **Lưu ý:** Hai terminal đồng thời cần RTSP server cho phép **2 kết nối**. Nếu không mở được cả hai, đóng cửa sổ `ffplay` rồi thu ảnh, đừng đổi cấu hình camera vội. Khi thu lại một session, xóa/đổi tên ảnh cũ trong session đó để tránh lẫn ảnh giữa các lần thu.

## 4. Cách thu 5 session

| Session | Cách thu (40 ảnh/session) |
|---|---|
| `Session_1` | Chính diện, ánh sáng đều, thấy rõ 2 mắt |
| `Session_2` | Xoay trái/phải nhẹ (khoảng 10–15°), vẫn thấy rõ mặt |
| `Session_3` | Thay đổi khoảng cách, giữ khuôn mặt đủ lớn |
| `Session_4` | Thay đổi ánh sáng, cúi/ngửa nhẹ |
| `Session_5` | Điều kiện khác các phiên trước để làm **test độc lập** |

- Di chuyển camera **chậm** để tránh nhòe, không cần cố chụp góc nghiêng quá lớn.
- Tránh che mắt, cắt mất cằm/trán, quá tối hoặc ngược sáng.
- Giữ khuôn mặt gần vị trí ROI dự kiến dùng khi demo trên LuckFox.
- Nếu ảnh RTSP có chữ ngày/giờ trên góc màn hình, cần **crop bỏ vùng chữ** khi preprocessing để tránh mô hình học dấu hiệu không liên quan.
- Session_5 nên thu trong điều kiện thật sự khác, không chỉ vài giây nối tiếp Session_4.

## 5. Kiểm tra số ảnh

```bash
for dir in Dataset/Person_A/Session_*, do
  printf '%s: ' "$(basename "$dir")"
  find "$dir" -maxdepth 1 -type f -name '*.jpg' | wc -l
done

find Dataset/Person_A -type f -name '*.jpg' | wc -l
```

Ảnh ở `Dataset/Person_A` là **dữ liệu gốc**: giữ nguyên để sau này chạy crop/resize và chia Train/Val/Test theo session.
