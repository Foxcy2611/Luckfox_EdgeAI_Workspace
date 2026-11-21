#!/usr/bin/env python3
"""Validate YOLO detection labels after annotation/export (standard library only)."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

IMG_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def main() -> None:
    ap = argparse.ArgumentParser(description="Check YOLO format, class indices, and split overlap")
    ap.add_argument("--dataset", type=Path, default=Path(__file__).resolve().parents[1] / "Dataset")
    ap.add_argument("--classes", type=int, default=3)
    args = ap.parse_args()
    if args.classes <= 0:
        ap.error("--classes must be positive")

    total_images = total_boxes = warnings = errors = 0
    hashes: dict[str, tuple[str, str]] = {}
    for split in ("train", "val", "test"):
        images_dir = args.dataset / "images" / split
        labels_dir = args.dataset / "labels" / split
        images = sorted(p for p in images_dir.glob("*") if p.is_file() and p.suffix.lower() in IMG_EXTS)
        image_stems = {p.stem for p in images}
        labels = list(labels_dir.glob("*.txt"))
        print(f"[{split.upper()}] images={len(images)} labels={len(labels)}")
        total_images += len(images)
        for label in labels:
            if label.stem not in image_stems:
                print(f"[ERROR] label without image: {label}")
                errors += 1
        for image in images:
            digest = hashlib.sha256(image.read_bytes()).hexdigest()
            if digest in hashes and hashes[digest][0] != split:
                print(f"[ERROR] byte-identical image across splits: {hashes[digest][1]} / {image}")
                errors += 1
            else:
                hashes[digest] = (split, str(image))

            label = labels_dir / (image.stem + ".txt")
            if not label.exists():
                print(f"[WARN] missing label (treated as negative image): {image.name} [{split}]")
                warnings += 1
                continue
            for line_no, line in enumerate(label.read_text(encoding="utf-8").splitlines(), 1):
                if not line.strip():
                    continue
                parts = line.split()
                where = f"{label}:{line_no}"
                try:
                    if len(parts) != 5:
                        raise ValueError("expected class x y w h")
                    cls = int(parts[0])
                    x, y, w, h = map(float, parts[1:])
                    if not 0 <= cls < args.classes:
                        raise ValueError(f"class {cls} outside 0..{args.classes - 1}")
                    if not (0 <= x <= 1 and 0 <= y <= 1 and 0 < w <= 1 and 0 < h <= 1):
                        raise ValueError("normalized values outside expected ranges")
                    if (x - w / 2 < -1e-5 or x + w / 2 > 1 + 1e-5 or
                        y - h / 2 < -1e-5 or y + h / 2 > 1 + 1e-5):
                        raise ValueError("bounding box extends outside image")
                    total_boxes += 1
                except ValueError as exc:
                    print(f"[ERROR] {where}: {exc}")
                    errors += 1
    if total_images == 0:
        print("[INFO] Dataset has no annotated images yet. Collect & label first.")
    print(f"[SUMMARY] images={total_images}, boxes={total_boxes}, errors={errors}, warnings={warnings}")
    raise SystemExit(1 if errors else 0)


if __name__ == "__main__":
    main()
