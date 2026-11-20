# YuNet

YuNet phát hiện khuôn mặt trong khung hình và trả về vị trí cùng năm điểm mốc để căn chỉnh khuôn mặt trước khi đưa vào SFace.

- `yunet.onnx` dùng với OpenCV trên Ubuntu và làm đầu vào cho bước chuyển đổi.
- `yunet.rknn` là bản INT8 dùng để phát hiện khuôn mặt trên LuckFox RV1106.
