# SFace

SFace nhận ảnh khuôn mặt đã căn chỉnh và tạo vector đặc trưng 128 chiều. Project so sánh vector này với vector tham chiếu của người đã đăng ký để xác minh.

- `sface.onnx` dùng với OpenCV trên Ubuntu và làm đầu vào cho bước chuyển đổi.
- `sface.rknn` là bản INT8 dùng để trích xuất vector trên LuckFox RV1106.
