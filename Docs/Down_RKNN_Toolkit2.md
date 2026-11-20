# RKNN-Toolkit2 — Ghi chú cài đặt (LuckFox RV1106)

> Mục tiêu: **TensorFlow/Keras (train) → ONNX → RKNN INT8 → C++ trên LuckFox**.  
> LuckFox đang có **RKNN Runtime 1.6.0**; ưu tiên **RKNN-Toolkit2 1.6.0** để giảm rủi ro lệch phiên bản.  
> **Các lệnh cài đặt dưới đây chạy trên Ubuntu**, không chạy qua SSH vào LuckFox.

## 1. Tạo môi trường RKNN riêng

```bash
cd ~/Luckfox_EdgeAI_Workspace/Tools
# Tạo 1 venv lấy tên là .venv-rknn
python3.11 -m venv .venv-rknn
source .venv-rknn/bin/activate
python -m pip install --upgrade pip
```

Nếu đã tạo venv thì chỉ cần `source .../activate`; không tạo lại. Môi trường TensorFlow dùng để train **vẫn giữ nguyên bên ngoài venv này**.

Việc tạo `venv` riêng là do:
- `RKNN 1.6.0` cần các bản `dependency` cũ, mà TF/KR rất có thể đang dùng bản mới
- Không ảnh hướng đến Python hệ thống và dễ quản lý

Có thể bật/tắt venv ngay tại trong `/Tools/`
```bash
# Bật
source .venv-rknn/bin/activate 

# Tắt
deactivate
```
## 2. Lấy bộ RKNN-Toolkit2 1.6.0

```bash
cd ~/Luckfox_EdgeAI_Workspace/Tools
git clone --depth 1 -b v1.6.0 \
  https://github.com/rockchip-linux/rknn-toolkit2.git \
  rknn-toolkit2-v1.6.0
```

**Nếu đã clone thì bỏ qua bước này.** `git clone` không liên quan venv, **không cần `deactivate`**.

## 3. Cài dependency nhẹ trước (tạm bỏ torch và tensorflow)

```bash
cd ~/Luckfox_EdgeAI_Workspace/Tools
REQ=rknn-toolkit2-v1.6.0/rknn-toolkit2/packages/requirements_cp311-1.6.0.txt

grep -vE '^(torch|tensorflow)==' "$REQ" > /tmp/rknn-requirements.txt
python -m pip install --no-cache-dir -r /tmp/rknn-requirements.txt
```

> Ta **train bằng TensorFlow/Keras**, không dùng PyTorch. Vì chỉ chuyển **ONNX → RKNN**, trước mắt thử bỏ hai gói nặng trong venv để tiết kiệm dung lượng. Đây là cách cài tối giản **chưa được Rockchip bảo đảm hỗ trợ đầy đủ**, nếu thiếu module khi chạy sẽ xử lý theo lỗi thực tế.

## 4. Cài RKNN-Toolkit2 từ wheel

```bash
cd ~/Luckfox_EdgeAI_Workspace/Tools
find rknn-toolkit2-v1.6.0/rknn-toolkit2/packages \
  -name '*cp311*linux_x86_64.whl'
```

Lấy **đúng đường dẫn** `.whl` vừa tìm được để cài:

```bash
python -m pip install --no-cache-dir --no-deps "DUONG_DAN_FILE_WHL"
```

`--no-deps` tránh pip tự kéo lại PyTorch/TensorFlow. **Không dùng nhầm wheel `cp310` hoặc `aarch64`.**

## 5. Kiểm tra rồi chuyển model

```bash
python -m pip show rknn-toolkit2
python -c 'from rknn.api import RKNN; print("RKNN OK")'
df -h "$HOME"
```

Nếu import thành công, thử chuyển đổi trong **Project 01** (khi đã có script):

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification
python Training/quantize_rknn.py
```

Dùng dữ liệu **train** để calibration PTQ INT8; kiểm tra định dạng RGB và chuẩn hóa `/255` **đúng một lần**. Có `.rknn` rồi vẫn phải thử nạp/inference trên board.

## 6. Dùng lại / dọn dung lượng

```bash
# Lần sau chỉ cần kích hoạt:
source ~/Luckfox_EdgeAI_Workspace/Tools/.venv-rknn/bin/activate

# Khi làm xong:
deactivate

# Nếu ổ đầy: xóa cache pip (không gỡ package đã cài)
python3.11 -m pip cache purge
```

**Lưu ý:** Chỉ khi đã `deactivate`, có thể xóa riêng `Tools/.venv-rknn/` để giải phóng dung lượng; giữ `Tools/rknn-toolkit2-v1.6.0/` để khỏi clone lại. Nếu `pip check` báo thiếu `torch`/`tensorflow`, đó là hệ quả của cách cài tối giản: **phải thử import và build thực tế**, không mặc định là đủ dependency.

**Tham khảo:** [RKNN-Toolkit2 v1.6.0](https://github.com/rockchip-linux/rknn-toolkit2/tree/v1.6.0) · [LuckFox RV1106 RKNN](https://wiki.luckfox.com/Luckfox-Pico-RV1106/RKNN/)