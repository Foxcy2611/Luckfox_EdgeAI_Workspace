# 📚 Tài liệu — LuckFox EdgeAI Workspace

Tổng hợp **hướng dẫn thiết lập, ghi chú thực nghiệm và quy trình triển khai** trên LuckFox Pico Max (RV1106). Tài liệu tập trung vào những bước cần dùng khi làm project, không tổ chức thành giáo trình lý thuyết.

## 📑 Danh sách tài liệu

| Tài liệu | Nội dung chính |
|---|---|
| [🐧 Ubuntu Setup](Ubuntu_Setup.md) | Chuẩn bị Ubuntu, công cụ phát triển, Python và môi trường AI trên laptop. |
| [🏗️ Build Image](Build_Image.md) | Kiểm tra board qua UART, build SDK, ghi thẻ SD, boot và SSH qua USB RNDIS. |
| [🧠 Dev → Release (RKNN)](Dev_to_Release_LuckFox_RKNN.md) | Train/export model, cross-compile C++, SCP để thử nghiệm và đóng gói firmware tự khởi chạy. |
| [📷 Camera Usage](Camera_Usage.md) | Kết nối camera, kiểm tra pipeline, thu frame, xử lý ảnh và chuẩn bị đầu vào cho AI. |
| [🧩 C++ Libraries & Cross-Compile](Cpp_Libraries_and_Cross_Compile.md) | `#include` / compile / link; dùng OpenCV-Mobile đã c-p hoặc tự c-p OpenCV từ source; RKNN và YOLO. |

## 🚀 Quy trình làm việc

```text
Ubuntu Setup → Build Image → Boot & SSH
                           ↓
          C-p C++ + link thư viện / Convert RKNN
                           ↓
                   SCP → Test → Debug
                           ↓
                Đóng gói bản Release
```

**Khi thêm thư viện C++:** xem [C++ Libraries & Cross-Compile](Cpp_Libraries_and_Cross_Compile.md) để phân biệt thư viện đã c-p và thư viện source.

**Khi có camera:** tham khảo `Camera_Usage.md` để thay ảnh tĩnh bằng frame từ camera trong pipeline C++/RKNN.

## 📌 Quy ước

- `docs/` chỉ giữ **quy trình dùng chung**; code và hướng dẫn riêng để trong `projects/<ten_project>/`.
- Phân biệt **bước đã thử nghiệm** với **bước dự kiến**. Những lệnh phụ thuộc board, SDK, hệ điều hành hoặc firmware cần kiểm tra lại trước khi dùng.
- Trong giai đoạn phát triển, ưu tiên **cross-compile + SCP**; chỉ đóng gói/build lại image khi cần thay đổi firmware hoặc chuẩn bị bản phát hành.
- Trước khi ghi thẻ SD, luôn xác nhận đúng thiết bị đích để tránh ghi nhầm ổ đĩa.

## 🔗 Tham khảo

[LuckFox Wiki](https://wiki.luckfox.com/) · [LuckFox Pico SDK](https://github.com/LuckfoxTECH/luckfox-pico) · [RKNN Toolkit2](https://github.com/airockchip/rknn-toolkit2)
