# Models

Project dùng hai mô hình theo thứ tự xử lý sau.

1. [YuNet](detectors/README.md) phát hiện khuôn mặt và các điểm mốc để căn chỉnh khuôn mặt.
2. [SFace](recognizers/README.md) trích xuất vector đặc trưng 128 chiều từ khuôn mặt đã căn chỉnh để so sánh với vector tham chiếu khi xác minh.

Các file `.onnx` và `.rknn` được lưu cục bộ và Git bỏ qua. Tải bản ONNX theo [hướng dẫn tải mô hình](../Docs/Download_ONNX_Models.md), rồi dùng [quy trình lượng tử hóa INT8](../Scripts/README.md) để tạo bản RKNN chạy trên NPU của LuckFox RV1106.
