# Tài liệu Project 01

Các bước chuẩn bị dữ liệu, mô hình và triển khai ứng dụng xác minh khuôn mặt trên LuckFox Pico Max.

1. [Thu ảnh từ camera SC3336](Collect_Data.md) — chuẩn bị ảnh khuôn mặt cho Person_A.
2. [Tải YuNet và SFace ONNX](Download_ONNX_Models.md) — lưu hai mô hình vào đúng thư mục dưới `Models/`.
3. [Enrollment, calibration và chuyển RKNN INT8](../Scripts/README.md) — tạo embedding tham chiếu và bản RKNN cho RV1106.
4. [Build và triển khai C++ trên LuckFox](CPP_Deploy_LuckFox.md) — cross-compile, chép file qua SCP, chạy camera và mở viewer.

[Tổng quan Project 01](../README.md) · [Tài liệu dùng chung](../../../Docs/README.md)
