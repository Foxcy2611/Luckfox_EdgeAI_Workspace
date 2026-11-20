# 🏗️ Chuẩn bị và build image LuckFox (giai đoạn đầu)

## 📑 Mục lục

- [1. Kiểm tra phiên bản của Luckfox](#1-kiểm-tra)
- [2. Chuẩn bị cho LuckFox SDK](#2-chuẩn-bị-cho-luckfox-sdk)
- [3. Nạp image vào thẻ nhớ](#3-nạp-image-vào-thẻ-nhớ)
<a id="muc-tieu"></a>

## 1. Kiểm tra phiên bản của Luckfox

Có thể sử dụng UART với moudle `CP2102` nối thông qua UART2 để kiểm tra phiên bản với các bước sau:

### 1.1. Set up dây cho UART2

Luckfox mặc định xuất log debug mặc định qua `UART2` với baud 115200, cắm dây theo quy tắc sau:
```text
CP2102 GND -> LuckFox GND
CP2102 RXD -> LuckFox UART2_TX (chân TX nằm cuối cạnh cổng type-C)
CP2102 TXD -> LuckFox UART2_RX (chân RX ngay cạnh đó)
```

### 1.2. Chạy lệnh

Cắm CP2102 vào laptop, và mở sẵn 1 terminal chạy lệnh sau
```bash
ls /dev/ttyUSB* # Ví dụ: /dev/ttyUSB0
sudo picocom -b 115200 /dev/ttyUSB0 # Mở cổng sẵn sàng lắng nghe
```

Sau khi có dòng chữ `Terminal ready` thì sẵn sàng

### 1.3. Cắm type-C cấp nguồn cho Luckfox

Sau khi cấp nguồn, nó sẽ chạy 1 loạt log để chuẩn bị đăng nhập, tua lại đoạn đầu và check thông số. Nếu log ra như thế này là bản Pico Max DRAM 256MB

```terminal
Model: Rockchip RV1106 EVB Board
MPIDR: 0xf00
PreSerial: 2, raw, 0xff4c0000
DRAM:  256 MiB
Sysmem: init
Relocation Offset: 0fd80000
Relocation fdt: 0edf9f58 - 0edfede8
CR: M/C/I
Using default environment
```

## 2. Chuẩn bị cho LuckFox SDK

Với **LuckFox Pico SDK** được hãng hướng dẫn, bước lấy source thường là:

```bash
git clone --depth 1 https://github.com/LuckfoxTECH/luckfox-pico.git
```

Cờ `--depth 1` giúp tải nhanh hơn và bỏ qua toàn bộ lịch sử commit cũ.

Bắt đầu build image thông qua SDK được cài trên máy với lần lượt lệnh sau (Không nên copy hết rồi paste 1 lần):
```bash
cd ~/luckfox-pico
./build.sh lunch
4 # Bản Pico MAX vừa check
0 # SD CARD
0 # Buildroot
./build.sh
```

Sau khi build xong, các output sẽ được xuất ra ở `~/luckfox-pico/output/image/`.

## 3. Nạp image vào thẻ nhớ

Cắm thẻ nhớ vào đầu đọc thẻ, rồi tiến hành nạp image đã build vào thẻ nhớ

### 3.1. Kiểm tra tên thiết bị thẻ nhớ

Chạy lệnh sau
```bash
lsblk
```

Thẻ nhớ được cắm vào vào sẽ nhận diện dưới dạng
- `SIZE` khoảng 58-60GB (Đây là khoảng của thẻ nhớ 64GB)
- `TYPE` là `disk` và có tên là `sda`

### 3.2. Nạp image và ssh

Sử dụng tool của chính `luckfox-pico` tên là `blkenvflash` để tiến hành nạp image vào thẻ nhớ
```bash
# 1. Vào thư mục chứa các file ảnh firmware vừa build (có sẵn sd_update.txt)
cd ~/luckfox-pico/output/image

# 2. Tìm và copy tool blkenvflash từ bộ SDK ra thư mục hiện tại
find ~/luckfox-pico -name "blkenvflash" -exec cp {} . \;

# 3. Dự phòng: nếu lệnh trên tìm không thấy thì tải từ server LuckFox và giải nén
[ -f ./blkenvflash ] || (wget https://files.luckfox.com/wiki/Luckfox-Pico/Software/blkenvflash.zip && unzip -o blkenvflash.zip)

# 4. Cấp quyền thực thi cho file blkenvflash
chmod +x blkenvflash

# 5. Nạp toàn bộ phân vùng firmware xuống thẻ nhớ /dev/sda
sudo ./blkenvflash /dev/sda
```

### 3.3. Khắc phục ghi thiếu

Đôi khi, `blkenvflash` ghi chỉ 5 phân vùng đầu (env, idblock, uboot, boot, userdata) mà không ghi đủ `rootfs.img`, để khắc phục điều đó ta cần chạy bash sau

```bash
# 1. Kiểm tra lại tên thẻ nhớ (xác nhận vẫn là sda ~59.5G)
lsblk

# 2. Gỡ mount tự động nếu có và ghi nối tiếp rootfs.img vào sector 591424
sudo umount /dev/sda* 2>/dev/null || true
sudo dd if=~/luckfox-pico/output/image/rootfs.img of=/dev/sda bs=512 seek=591424 conv=fsync status=progress
sync
```

Lưu ý: Cần thử đọc UART xem thực sự vào được giao diện login chưa rồi mới khẳng định rằng nạp thiếu.

Lắp thẻ vào Luckfox rồi mới cấp nguồn, sau 15-20s tiến hành `SSH` vào Luckfox:
```bash
ssh root@172.32.0.93
```

Mặc định, bản Buildroot của Luckfox sẽ có password là: `luckfox`.

## 4. Cấu hình mạng LAN ảo qua USB để SSH

Khi lần đầu SSH vào LuckFox, kết nối có thể bị treo do Ubuntu chưa được cấp địa chỉ IP cùng lớp mạng với LuckFox.

Cổng USB-C của LuckFox hỗ trợ **USB RNDIS**, cho phép tạo mạng Ethernet ảo giữa laptop và LuckFox mà không cần dây LAN.

**Bước 1: Xác định tên card mạng USB RNDIS trên Ubuntu**

```bash
ip -br link
```

Ví dụ card mạng được nhận diện là `enx2ebb6b234bf1`.

**Bước 2: Gán IP cùng lớp mạng với LuckFox**

```bash
sudo ip addr add 172.32.0.100/24 dev enx2ebb6b234bf1
```

**Bước 3: Kết nối SSH**

```bash
ssh root@172.32.0.93
```

Mật khẩu mặc định: `luckfox`.

**Bước 4: Cấu hình tự động cho những lần kết nối sau**

```bash
sudo nmcli connection add type ethernet \
  con-name "luckfox-usb" \
  match.driver rndis_host \
  ipv4.addresses 172.32.0.100/24 \
  ipv4.method manual \
  autoconnect yes
```

Sau khi cấu hình, Ubuntu sẽ tự gán IP cho card RNDIS khi profile được kích hoạt. Những lần sau chỉ cần kết nối USB-C và chạy:

```bash
ssh root@172.32.0.93
```

> **Lưu ý:** Các địa chỉ IP trên áp dụng cho cấu hình LuckFox Buildroot đang sử dụng. Tên card mạng USB có thể khác giữa các máy.