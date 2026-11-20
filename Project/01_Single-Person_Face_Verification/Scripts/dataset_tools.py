"""Inspect an existing SC3336 enrollment dataset without changing originals."""
from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path

import cv2

from camera_io import blur_score
from face_pipeline import FacePipeline

IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def existing_images(directory: Path) -> list[Path]:
    if not directory.is_dir():
        raise FileNotFoundError(f"Khong co thu muc anh: {directory}")
    return sorted(p for p in directory.rglob("*") if p.is_file() and p.suffix.lower() in IMAGE_SUFFIXES)


@dataclass
class Inspection:
    file: Path
    status: str
    count: int
    face_score: float
    face_width: float
    face_height: float
    sharpness: float
    width: int
    height: int

    @property
    def accepted(self) -> bool:
        return self.status == "OK"


def inspect_image(path: Path, pipeline: FacePipeline, *, min_face: int, min_blur: float):
    bgr = cv2.imread(str(path))
    if bgr is None:
        return Inspection(path, "INVALID_IMAGE", 0, 0, 0, 0, 0, 0, 0), None, None
    height, width = bgr.shape[:2]
    sharp = blur_score(bgr)
    faces = pipeline.detect(bgr)
    count = len(faces)
    face = faces[0] if count else None
    if count == 0:
        status = "NO_FACE"
    elif count > 1:
        status = "MULTIPLE_FACES"
    elif min(face[2], face[3]) < min_face:
        status = "SMALL_FACE"
    elif sharp < min_blur:
        status = "LOW_SHARPNESS"
    else:
        status = "OK"
    return (Inspection(path, status, count,
                       float(face[14]) if face is not None else 0.0,
                       float(face[2]) if face is not None else 0.0,
                       float(face[3]) if face is not None else 0.0,
                       sharp, width, height), bgr, faces)


def inspect_dataset(
    directory: Path, pipeline: FacePipeline, *, min_face: int, min_blur: float,
    output_csv: Path | None = None, annotated_dir: Path | None = None,
) -> dict[str, int]:
    images = existing_images(directory)
    if not images:
        raise ValueError(f"Khong co anh trong: {directory}")
    summary: dict[str, int] = {}
    rows = []
    for index, path in enumerate(images, start=1):
        result, bgr, faces = inspect_image(path, pipeline, min_face=min_face, min_blur=min_blur)
        summary[result.status] = summary.get(result.status, 0) + 1
        rows.append({
            "file": str(path.relative_to(directory)), "status": result.status,
            "faces": result.count, "confidence": f"{result.face_score:.4f}",
            "face_width": f"{result.face_width:.1f}",
            "face_height": f"{result.face_height:.1f}",
            "laplacian_variance": f"{result.sharpness:.1f}",
            "image_width": result.width, "image_height": result.height,
        })
        if annotated_dir and bgr is not None:
            annotated_dir.mkdir(parents=True, exist_ok=True)
            # Numbered output avoids filename collisions in subdirectories.
            out = annotated_dir / f"{index:04d}_{path.stem}.jpg"
            cv2.imwrite(str(out), pipeline.draw_faces(bgr, faces, result.status))
        print(f"[INSPECT] {index}/{len(images)} {path.name}: {result.status} "
              f"faces={result.count} sharpness={result.sharpness:.1f}")
    if output_csv is not None:
        output_csv.parent.mkdir(parents=True, exist_ok=True)
        with output_csv.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        print(f"[INSPECT] CSV: {output_csv}")
    print(f"[INSPECT] {len(images)} images; " + ", ".join(f"{k}={v}" for k, v in sorted(summary.items())))
    return summary
