"""OpenCV ONNX reference pipeline: YuNet -> SFace, runs on the Ubuntu host."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np


class FacePipelineError(RuntimeError):
    pass


@dataclass
class FaceObservation:
    face: np.ndarray   # YuNet row: bbox(4), five landmarks(10), score(1)
    aligned: np.ndarray  # BGR, normally 112 x 112, OpenCV SFace alignCrop output
    embedding: np.ndarray  # L2-normalized float32


def l2_normalize(vec: np.ndarray) -> np.ndarray:
    v = np.asarray(vec, dtype=np.float32).reshape(-1)
    norm = float(np.linalg.norm(v))
    if norm < 1e-9 or not np.isfinite(norm):
        raise FacePipelineError("Embedding co norm bang 0/khong hop le")
    return np.ascontiguousarray(v / norm, dtype=np.float32)


def cosine_similarity(a: np.ndarray, b: np.ndarray) -> float:
    if np.size(a) != np.size(b):
        raise FacePipelineError(f"Embedding khac kich thuoc: {np.size(a)} vs {np.size(b)}")
    return float(np.dot(l2_normalize(a), l2_normalize(b)))


class FacePipeline:
    def __init__(
        self,
        yunet_path: Path,
        sface_path: Path,
        score_threshold: float = 0.75,
        max_detector_side: int = 640,
    ) -> None:
        self.yunet_path = Path(yunet_path).resolve()
        self.sface_path = Path(sface_path).resolve()
        for path in (self.yunet_path, self.sface_path):
            if not path.is_file():
                raise FileNotFoundError(f"Thieu model: {path}")
        if not hasattr(cv2, "FaceDetectorYN") or not hasattr(cv2, "FaceRecognizerSF"):
            raise FacePipelineError(
                "OpenCV hien tai khong ho tro FaceDetectorYN/FaceRecognizerSF. "
                "Can OpenCV ban co hai API nay."
            )
        self.detector = cv2.FaceDetectorYN.create(
            str(self.yunet_path), "", (320, 320),
            score_threshold=score_threshold,
            nms_threshold=0.3,
            top_k=5000,
        )
        self.recognizer = cv2.FaceRecognizerSF.create(str(self.sface_path), "")
        self.max_detector_side = max_detector_side

    def detect(self, bgr: np.ndarray) -> np.ndarray:
        if bgr is None or bgr.ndim != 3 or bgr.shape[2] != 3:
            raise FacePipelineError("Anh dau vao phai la BGR 3 kenh")
        h, w = bgr.shape[:2]
        if min(w, h) < 2:
            return np.empty((0, 15), dtype=np.float32)
        scale = min(1.0, self.max_detector_side / max(w, h))
        nw, nh = max(2, int(round(w * scale))), max(2, int(round(h * scale)))
        small = cv2.resize(bgr, (nw, nh)) if (nw, nh) != (w, h) else bgr
        self.detector.setInputSize((nw, nh))
        _, faces = self.detector.detect(small)
        if faces is None:
            return np.empty((0, 15), dtype=np.float32)
        faces = np.asarray(faces, dtype=np.float32).copy()
        if (nw, nh) != (w, h):
            faces[:, [0, 2, 4, 6, 8, 10, 12]] *= w / nw
            faces[:, [1, 3, 5, 7, 9, 11, 13]] *= h / nh
        # Deterministic ordering: highest confidence first.
        return faces[np.argsort(-faces[:, 14])]

    def observe(self, bgr: np.ndarray, require_single: bool = True) -> FaceObservation:
        faces = self.detect(bgr)
        if not len(faces):
            raise FacePipelineError("NO_FACE")
        if require_single and len(faces) != 1:
            raise FacePipelineError(f"MULTIPLE_FACES({len(faces)})")
        face = faces[0]
        aligned = self.recognizer.alignCrop(bgr, face)
        embedding = l2_normalize(self.recognizer.feature(aligned))
        return FaceObservation(face=face, aligned=aligned, embedding=embedding)

    @staticmethod
    def draw_faces(bgr: np.ndarray, faces: np.ndarray, caption: str = "") -> np.ndarray:
        out = bgr.copy()
        for face in faces:
            x, y, w, h = map(int, face[:4])
            cv2.rectangle(out, (x, y), (x + w, y + h), (0, 220, 0), 2)
            for k in range(5):
                cv2.circle(out, (int(face[4 + 2 * k]), int(face[5 + 2 * k])), 2, (0, 255, 255), -1)
            cv2.putText(out, f"{face[14]:.2f}", (x, max(16, y - 6)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 220, 0), 1)
        if caption:
            cv2.putText(out, caption, (12, 30), cv2.FONT_HERSHEY_SIMPLEX,
                        0.7, (0, 255, 255), 2, cv2.LINE_AA)
        return out
