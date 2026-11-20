# 🛠️ Ubuntu Setup — Môi trường phát triển LuckFox

> **Mục tiêu:** Chuẩn bị bộ công cụ học Linux, lập trình C/C++, Git, SSH và kiểm tra môi trường build SDK.

## 📑 Mục lục

- [1. Chọn phiên bản Ubuntu](#1-chọn-phiên-bản-ubuntu)
- [2. Khảo sát máy Host](#2-khảo-sát-máy-host)
- [3. Cài công cụ nền tảng](#3-cài-công-cụ-nền-tảng)
- [4. Thiết lập workspace](#4-thiết-lập-workspace)
- [5. Chuẩn bị cho Python 3.11.x và TensorFlow hỗ trợ huấn luyện trên Card RTX](#5-chuẩn-bị-cho-python-311x-và-tensorflow-hỗ-trợ-huấn-luyện-trên-card-rtx)

## 1. Chọn phiên bản Ubuntu

**Khuyến nghị cho tài liệu build LuckFox Pico SDK phổ biến:** Ubuntu **22.04 LTS x86_64** (bare metal hoặc VM được cấp đủ tài nguyên). Đây là môi trường được LuckFox ghi rõ cho nhiều hướng dẫn Pico/Plus/Mini, **không phải cam kết cho mọi dòng LuckFox**. Có thể dùng máy Ubuntu phiên bản khác để **viết code và học Linux**, nhưng khi build SDK cần đối chiếu đúng model và tài liệu của hãng, đôi khi cần container tương ứng.

## 2. Khảo sát máy Host

```bash
lsb_release -a             # Ubuntu release (nếu có lsb_release)
uname -m                   # Cần phân biệt x86_64 với aarch64
free -h                    # RAM còn trống
df -h "$HOME"              # Dung lượng trống
nproc                      # Số CPU logic
```

Để build image SDK, nên chuẩn bị **nhiều dung lượng trống** và RAM thích hợp theo hướng dẫn của đúng bản SDK, không áp dụng một con số cố định cho mọi board.

## 3. Cài công cụ nền tảng

Cập nhật danh sách package trước khi cài:

```bash
sudo apt update
```

Chọn cài theo từng nhóm nhu cầu:

```bash
# 1. Git, SSH & nén/đồng bộ file
sudo apt install -y git curl wget ca-certificates openssh-client rsync file unzip zip

# 2. Biên dịch C/C++
sudo apt install -y build-essential cmake ninja-build pkg-config gdb-multiarch

# 3. Môi trường Python
sudo apt install -y python3 python3-venv python3-pip python-is-python3

# 4. Giao tiếp USB, mạng & Serial UART
sudo apt install -y usbutils iproute2 net-tools picocom v4l-utils

# 5. Dependency bổ sung (Chỉ cần khi tự build LuckFox SDK Image)
sudo apt install -y gcc-multilib g++-multilib device-tree-compiler u-boot-tools bison flex gawk texinfo libssl-dev libncurses-dev gperf autoconf fakeroot expect bc cpio
```

**Lưu ý:** `v4l-utils` trên Host giúp kiểm tra camera cắm trực tiếp vào PC, muốn dùng V4L2 trên LuckFox thì target image phải tích hợp sẵn gói này.

Xác nhận sau khi cài:

```bash
git --version
gcc --version | head -n 1
cmake --version | head -n 1
ninja --version
python3 --version
ssh -V
```

## 4. Thiết lập workspace

Giữ **mã nguồn tự viết** và **SDK upstream** thành hai thư mục ngang cấp:

```text
~/development/
├── Luckfox_EdgeAI_Workspace/   # Git repo của bạn
└── luckfox-pico/               # SDK tải riêng, chỉ khi phù hợp board
```

Kiểm tra thư mục workspace hiện tại:

```bash
cd ~/Luckfox_EdgeAI_Workspace
pwd
ls -la
```

**Quy ước:** Không đưa SDK nhiều GB, image build, dataset lớn, password/token vào Git workspace. Các script tái sử dụng mới đưa vào `Tools/` hoặc `Platform/`.

## 5. Chuẩn bị cho Python 3.11.x và TensorFlow hỗ trợ huấn luyện trên Card RTX

Trên Ubuntu, kernel phù hợp và gắn bắt buộc với bản 3.10.x của python, cần nâng cấp lên 3.11.x để phù hợp với RockChip và đủ chuẩn với bộ Tensorflow & Keras 3.

Cài Python thêm 3.11.x chạy song song với 3.10.x của Kernel:

```bash
sudo add-apt-repository ppa:deadsnakes/ppa -y
sudo apt update
sudo apt install -y python3.11 python3.11-venv python3.11-dev

curl -sS https://bootstrap.pypa.io/get-pip.py | python3.11

# Cập nhật PATH để gọi trực tiếp các tool cài qua pip
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

Kiểm tra:
```bash
python3 --version      # Vẫn giữ nguyên Python 3.10.12 cho Ubuntu & LuckFox SDK
python3.11 --version   # Bản Python 3.11.x để chạy TF 2.21 / Keras 3 / AI
```

Sau khi thành công, tiến hành cài đặt TensorFlow hỗ trợ huấn luyện trên card đồ họa giúp huấn luyện nhanh hơn:
```bash
# Cài TensorFlow kèm CUDA/cuDNN (tắt cache để tiết kiệm ~3 GB ổ cứng)
python3.11 -m pip install --no-cache-dir "tensorflow[and-cuda]"

# Kiểm tra nhận diện card RTX và dung lượng trống còn lại
python3.11 -c "import tensorflow as tf; print('TF:', tf.__version__, '| GPU:', tf.config.list_physical_devices('GPU'))"
df -h "$HOME"
```

Trường hợp xảy ra lỗi như này (`TF: 2.21.0 | GPU: []`) tức khả năng cơ chế `SecureBoot enable` không cho Kernel nạp module driver của card NVDIA vì card thuộc bên thứ 3. Để sử dụng, cần đăng ký chữ ký số với BIOS

Để sử dụng cần làm các bước sau:
- Chạy lệnh sau: 
```bash
sudo mokutil --import /var/lib/shim-signed/mok/MOK.der
```
- Khởi động lại máy
```bash
sudo reboot
```

Khi mới bật máy lên, trước khi vào Ubuntu sẽ xuất hiện ra màn hình xanh trắng và lựa chọn như sau
- Chọn `Enroll MOK` -> `Enter`
- Nhập Pass tùy chọn (`123`)
- Chọn `Continue` -> `Enter`
- Chọn `Yes` -> `Enter`
- Nhập mật khẩu tạm vừa tạo bước 1 -> `Enter`
- CHọn `Reboot` -> `Enter`

Trường hợp ăn MOK nhưng Ubuntu vẫn chưa thấy thư viện CUDA thì kiểm tra như sau:

```bash
# 1. Xác nhận driver NVIDIA đã hoạt động
nvidia-smi

# 2. Lấy đường dẫn thư viện CUDA/cuDNN của Python 3.11 và lưu vào ~/.bashrc
echo 'export LD_LIBRARY_PATH=$(python3.11 -c "import os, glob, site; dirs = [d for p in ([site.getusersitepackages()] + site.getsitepackages()) for d in glob.glob(os.path.join(p, \"nvidia\", \"*\", \"lib\"))]; print(\":\".join(dirs))"):$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

# 3. Test lại TensorFlow GPU
python3.11 -c "import tensorflow as tf; print('TF:', tf.__version__, '| GPU:', tf.config.list_physical_devices('GPU'))"
```