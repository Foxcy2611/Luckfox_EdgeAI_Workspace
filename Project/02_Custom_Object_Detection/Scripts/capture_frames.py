#!/usr/bin/env python3
"""Extract diverse raw frames from a video, webcam index, or LuckFox RTSP.

Examples:
  python3 Scripts/capture_frames.py --source clip.mp4 --output Dataset/raw/session_01 --interval-sec 2
  python3 Scripts/capture_frames.py --source 0 --output Dataset/raw/webcam --interval-sec 2 --show

Requires: opencv-python (cv2). This tool does NOT annotate objects.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import time

import cv2


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Capture raw frames for YOLO labeling")
    p.add_argument("--source", required=True, help="Video file, RTSP URL, or webcam index (0,1,...)")
    p.add_argument("--output", required=True, type=Path, help="A raw session directory")
    p.add_argument("--interval-sec", type=float, default=2.0, help="Minimum time between saved frames")
    p.add_argument("--max-frames", type=int, default=100)
    p.add_argument("--min-sharpness", type=float, default=0.0, help="Minimum variance of Laplacian; 0 disables")
    p.add_argument("--show", action="store_true", help="Show live preview; press q to stop")
    args = p.parse_args()
    if args.interval_sec <= 0 or args.max_frames <= 0 or args.min_sharpness < 0:
        p.error("--interval-sec and --max-frames must be > 0; --min-sharpness >= 0")
    return args


def next_id(folder: Path) -> int:
    nums = []
    for p in folder.glob("frame_*.jpg"):
        m = re.fullmatch(r"frame_(\d+)", p.stem)
        if m:
            nums.append(int(m.group(1)))
    return max(nums, default=0) + 1


def main() -> None:
    args = parse_args()
    src = int(args.source) if args.source.isdigit() else args.source
    is_file = not isinstance(src, int) and not str(src).lower().startswith(("rtsp://", "http://", "https://"))
    if is_file and not Path(src).is_file():
        raise SystemExit(f"[ERROR] File not found: {src}")

    args.output.mkdir(parents=True, exist_ok=True)
    cap = cv2.VideoCapture(src)
    if not cap.isOpened():
        raise SystemExit(f"[ERROR] Cannot open source: {src}")

    fps = cap.get(cv2.CAP_PROP_FPS)
    if fps <= 0 or fps > 1000:
        fps = 0.0
    saved = 0
    skipped_blur = 0
    frame_index = 0
    ident = next_id(args.output)
    last_saved_at = -1e12
    start = time.monotonic()
    print(f"[CAPTURE] source={args.source}  output={args.output}  interval={args.interval_sec}s")
    try:
        while saved < args.max_frames:
            ok, frame = cap.read()
            if not ok:
                break
            frame_index += 1
            elapsed = (frame_index - 1) / fps if is_file and fps > 0 else time.monotonic() - start
            if elapsed - last_saved_at < args.interval_sec:
                if args.show:
                    cv2.imshow("Collect raw images (q to stop)", frame)
                    if cv2.waitKey(1) & 0xFF in (ord("q"), 27):
                        break
                continue

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            sharpness = cv2.Laplacian(gray, cv2.CV_64F).var()
            if sharpness < args.min_sharpness:
                skipped_blur += 1
            else:
                filename = args.output / f"frame_{ident:04d}.jpg"
                if not cv2.imwrite(str(filename), frame, [int(cv2.IMWRITE_JPEG_QUALITY), 93]):
                    raise RuntimeError(f"Cannot write {filename}")
                print(f"[SAVE] {filename.name} at {elapsed:.2f}s  sharpness={sharpness:.1f}")
                saved += 1
                ident += 1
                last_saved_at = elapsed

            if args.show:
                cv2.imshow("Collect raw images (q to stop)", frame)
                if cv2.waitKey(1) & 0xFF in (ord("q"), 27):
                    break
    finally:
        cap.release()
        if args.show:
            cv2.destroyAllWindows()
    print(f"[DONE] saved={saved}; skipped_blur={skipped_blur}; read={frame_index}")


if __name__ == "__main__":
    main()
