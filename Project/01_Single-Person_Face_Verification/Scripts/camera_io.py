"""PC/Ubuntu video sources: webcam device, video file, or LuckFox RTSP."""
from __future__ import annotations

import cv2


def open_camera(source: str) -> cv2.VideoCapture:
    src = int(source) if source.isdecimal() else source
    cap = cv2.VideoCapture(src)
    if not cap.isOpened():
        raise RuntimeError("Khong mo duoc camera/video. Kiem tra --source, RTSP va ket noi mang.")
    # Not all capture backends support this; harmless if ignored.
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    return cap


def blur_score(bgr) -> float:
    gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
    return float(cv2.Laplacian(gray, cv2.CV_64F).var())
