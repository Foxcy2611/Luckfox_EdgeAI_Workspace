# Download ONNX Models — YuNet & SFace

Tải hai model pretrained từ **OpenCV Zoo** và đặt đúng đường dẫn theo layout của `01_Single-Person_Face_Verification`. Tài liệu này **chỉ hướng dẫn tải model và đổi tên**.

## 1. Vị trí cần đặt model

```text
01_Single-Person_Face_Verification/
└── Models/
    ├── detectors/
    │   └── yunet.onnx
    └── recognizers/
        └── sface.onnx
```

| Chức năng | Tên trên OpenCV Zoo | Đổi tên trong project |
|---|---|---|
| Face detection | `face_detection_yunet_2023mar.onnx` | `Models/detectors/yunet.onnx` |
| Face embedding | `face_recognition_sface_2021dec.onnx` | `Models/recognizers/sface.onnx` |

Nguồn chính thức:

- YuNet: https://github.com/opencv/opencv_zoo/blob/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
- SFace: https://github.com/opencv/opencv_zoo/blob/main/models/face_recognition_sface/face_recognition_sface_2021dec.onnx

## 2. Cách nhanh nhất: tải trực tiếp vào đúng `Models/` (khuyến nghị)

Chạy trên **Ubuntu**, từ terminal bất kỳ:

```bash
PROJECT="$HOME/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification"
mkdir -p "$PROJECT/Models/detectors" "$PROJECT/Models/recognizers"

# YuNet: tải và lưu trực tiếp dưới tên yunet.onnx
curl -fL --retry 3 \
  'https://huggingface.co/opencv/opencv_zoo/resolve/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx' \
  -o "$PROJECT/Models/detectors/yunet.onnx"

# SFace: tải và lưu trực tiếp dưới tên sface.onnx
curl -fL --retry 3 \
  'https://huggingface.co/opencv/opencv_zoo/resolve/main/models/face_recognition_sface/face_recognition_sface_2021dec.onnx' \
  -o "$PROJECT/Models/recognizers/sface.onnx"
```

> `curl -o` sẽ **ghi đè** nếu file cùng tên đã tồn tại. Nếu đã có đúng hai model như trong VS Code, **không cần tải lại**.

## 3. Cách thay thế: `git clone` OpenCV Zoo rồi copy + đổi tên

Chỉ dùng cách này nếu muốn lấy model bằng Git/Git LFS thay vì URL tải trực tiếp.

```bash
sudo apt install -y git git-lfs
git lfs install

PROJECT="$HOME/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification"
ZOO="$HOME/Luckfox_EdgeAI_Workspace/Tools/opencv_zoo"

# Clone repo vào Tools/ (không clone đè lên Models/ của project).
GIT_LFS_SKIP_SMUDGE=1 git clone --depth 1 \
  https://github.com/opencv/opencv_zoo.git "$ZOO"

# Lấy chính xác 2 model ONNX từ Git LFS.
git -C "$ZOO" lfs pull \
  --include='models/face_detection_yunet/face_detection_yunet_2023mar.onnx,models/face_recognition_sface/face_recognition_sface_2021dec.onnx' \
  --exclude=''

mkdir -p "$PROJECT/Models/detectors" "$PROJECT/Models/recognizers"
cp "$ZOO/models/face_detection_yunet/face_detection_yunet_2023mar.onnx" \
   "$PROJECT/Models/detectors/yunet.onnx"
cp "$ZOO/models/face_recognition_sface/face_recognition_sface_2021dec.onnx" \
   "$PROJECT/Models/recognizers/sface.onnx"
```

> Chỉ chạy `git clone` khi `Tools/opencv_zoo` **chưa tồn tại**. Lệnh `cp` cũng ghi đè file đích nếu đã có.

## 4. Kiểm tra sau khi tải

```bash
cd ~/Luckfox_EdgeAI_Workspace/Project/01_Single-Person_Face_Verification
ls -lh Models/detectors/yunet.onnx Models/recognizers/sface.onnx
sha256sum Models/detectors/yunet.onnx Models/recognizers/sface.onnx
```

SHA-256 của **hai bản ONNX đã dùng khi enrollment**:

```text
YuNet  8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4
SFace  0ba9fbfa01b5270c96627c4ef784da859931e02f04419c829e83484087c34e79
```

Nếu SHA khác, kiểm tra lại phiên bản file vừa tải trước khi sử dụng.