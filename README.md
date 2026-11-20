# 🚀 LuckFox EdgeAI Workspace

Workspace phát triển **Camera, Embedded Linux và Edge AI** trên **LuckFox / Rockchip**, sử dụng **Ubuntu** làm máy phát triển. Mỗi project được xây dựng và kiểm thử độc lập.

<!-- Các badge mô tả công nghệ dự kiến sử dụng, không có nghĩa tất cả đã được tích hợp. -->

**🖥️ Nền tảng & công cụ**  
![LuckFox](https://img.shields.io/badge/Board-LuckFox-1766A0?style=flat-square&logo=linux&logoColor=white)
![Ubuntu](https://img.shields.io/badge/Host-Ubuntu-E95420?style=flat-square&logo=ubuntu&logoColor=white)
![Linux](https://img.shields.io/badge/Target-Embedded_Linux-3A6EA5?style=flat-square&logo=linux&logoColor=white)
![C++](https://img.shields.io/badge/Code-C%2B%2B-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Python](https://img.shields.io/badge/Code-Python-3776AB?style=flat-square&logo=python&logoColor=white)
![CMake](https://img.shields.io/badge/Apps-CMake_%2B_Ninja-064F8C?style=flat-square&logo=cmake&logoColor=white)
![AI](https://img.shields.io/badge/AI-TensorFlow%20%2F%20Keras-FF6F00?style=flat-square&logo=tensorflow&logoColor=white)

**📷 Camera & Edge AI**  
![V4L2](https://img.shields.io/badge/Camera-V4L2%20%2F%20ISP-0C8F8F?style=flat-square)
![RTSP](https://img.shields.io/badge/Video-H.264%20%2F%20RTSP-C45A37?style=flat-square)
![OpenCV](https://img.shields.io/badge/CV-OpenCV-5C3EE8?style=flat-square&logo=opencv&logoColor=white)
![RKNN](https://img.shields.io/badge/AI-RKNN-9B59B6?style=flat-square)
![ONNX](https://img.shields.io/badge/Model-ONNX-005CED?style=flat-square&logo=onnx&logoColor=white)

**🔌 Tích hợp phần cứng**  
![STM32](https://img.shields.io/badge/MCU-STM32-03234B?style=flat-square&logo=stmicroelectronics&logoColor=white)
![ESP32](https://img.shields.io/badge/MCU-ESP32-E7352A?style=flat-square&logo=espressif&logoColor=white)
![Interfaces](https://img.shields.io/badge/Interface-UART%20%7C%20SPI%20%7C%20I2C-455A64?style=flat-square)

## 🎯 Mục tiêu

- **Camera & Multimedia:** capture ảnh/video, xử lý frame và streaming.
- **Computer Vision & Edge AI:** triển khai, kiểm thử và tối ưu suy luận trên thiết bị biên.
- **Embedded Integration:** giao tiếp với STM32/ESP32 khi project yêu cầu.

## 📂 Cấu trúc workspace

Cấu trúc tối giản dự kiến (các thư mục project chỉ tạo khi bắt đầu thực hiện):

```text
Luckfox_EdgeAI_Workspace/
├── README.md
├── docs/
│   ├── Ubuntu_Setup.md       # Thiết lập môi trường Ubuntu
│   └── Build_Image.md        # Build, nạp image và kết nối LuckFox
├── projects/                 # Mỗi project có README và code riêng
├── tools/                    # Script dùng chung, chỉ tạo khi cần
├── .gitignore
└── LICENSE
```

## ⚡ Bắt đầu

1. [**Thiết lập Ubuntu**](docs/Ubuntu_Setup.md) — cài công cụ và chuẩn bị môi trường phát triển.
2. [**Build & nạp image LuckFox**](docs/Build_Image.md) — kiểm tra board, build firmware, ghi thẻ và kết nối SSH.
3. Bắt đầu project mới trong `projects/`, ghi cách build/chạy/kiểm thử tại `README.md` riêng.

> **Lưu ý:** LuckFox SDK được quản lý **ngoài repository**. Cấu hình camera, SDK, RKNN và firmware phụ thuộc model board/SoC thực tế. **CMake + Ninja** dùng cho ứng dụng tự viết; image hệ thống được build theo SDK.

## 👨‍💻 Tác giả & giấy phép

**Nguyen Ngoc Chien** · Ubuntu / LuckFox / Embedded Linux  
**License:** [MIT](LICENSE) — các thành phần bên thứ ba tuân theo giấy phép riêng.
