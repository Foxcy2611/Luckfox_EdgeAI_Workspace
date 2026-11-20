#!/usr/bin/env python3
"""Entry point for Ubuntu-side YuNet + SFace face-verification workflow.

NOTE: `live` uses ONNX on the Ubuntu PC, fed by webcam or LuckFox RTSP.
It is not on-device RKNN inference (that will be the C++ executable).
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

import cv2
import numpy as np

from camera_io import blur_score, open_camera
from dataset_tools import existing_images, inspect_dataset
from face_pipeline import FacePipeline, FacePipelineError, cosine_similarity, l2_normalize
from storage import load_reference, save_reference

ROOT = Path(__file__).resolve().parent.parent
def get_pipeline(args) -> FacePipeline:
    return FacePipeline(args.yunet, args.sface,
                        score_threshold=args.score_threshold,
                        max_detector_side=args.detector_side)


def get_reference(args):
    return load_reference(args.reference, sface_model=args.sface,
                          ignore_model_hash=args.ignore_model_hash)


def check(args) -> None:
    print(f"[CHECK] project root: {ROOT}")
    print(f"[CHECK] OpenCV: {cv2.__version__}")
    for attr in ("FaceDetectorYN", "FaceRecognizerSF"):
        print(f"[CHECK] OpenCV {attr}: {'OK' if hasattr(cv2, attr) else 'MISSING'}")
    for label, path in (("YuNet", args.yunet), ("SFace", args.sface)):
        print(f"[CHECK] {label}: {'OK' if path.is_file() else 'MISSING'} ({path})")
    folder = ROOT / "Enrollment/Person_A"
    if folder.is_dir():
        print(f"[CHECK] Enrollment/Person_A: {len(existing_images(folder))} images ({folder})")
    else:
        print(f"[CHECK] Enrollment/Person_A: MISSING ({folder})")
    print(f"[CHECK] reference: {'OK' if args.reference.is_file() else 'NOT ENROLLED'} ({args.reference})")


def inspect(args) -> None:
    pipeline = get_pipeline(args)
    inspect_dataset(args.input, pipeline, min_face=args.min_face,
                    min_blur=args.min_blur, output_csv=args.csv,
                    annotated_dir=args.annotated_dir)


def preview(args) -> None:
    pipeline = get_pipeline(args)
    cap = open_camera(args.source)
    print("[PREVIEW] YuNet ONNX tren Ubuntu; anh tu RTSP LuckFox/PC. "
          "Khong phai inference NPU tren board.")
    print("[PREVIEW] Nhan q / ESC de thoat GUI; Ctrl+C neu khong co GUI.")
    frames = 0
    started = time.monotonic()
    try:
        while True:
            ok, bgr = cap.read()
            if not ok:
                print("[PREVIEW] End of stream")
                break
            faces = pipeline.detect(bgr)
            frames += 1
            elapsed = time.monotonic() - started
            if elapsed >= args.log_interval:
                print(f"[PREVIEW] faces={len(faces)} fps={frames / elapsed:.1f}")
                frames, started = 0, time.monotonic()
            if args.show:
                cv2.imshow("YuNet detection - Ubuntu ONNX", pipeline.draw_faces(
                    bgr, faces, f"Detected: {len(faces)}"))
                if (cv2.waitKey(1) & 0xFF) in (ord("q"), 27):
                    break
    finally:
        cap.release()
        if args.show:
            cv2.destroyAllWindows()


def capture(args) -> None:
    pipeline = get_pipeline(args)
    args.output.mkdir(parents=True, exist_ok=True)
    cap = open_camera(args.source)
    successes, inspected = 0, 0
    last_saved = -1e12
    session = time.strftime("%Y%m%d_%H%M%S")
    print("[CAPTURE] Nhan q hoac ESC de dung (neu --show); Ctrl+C neu terminal.")
    print("[CAPTURE] Luu anh khi co dung 1 mat, du kich thuoc va do net.")
    try:
        while successes < args.count:
            ok, bgr = cap.read()
            if not ok:
                print("[CAPTURE] Stream ngat/het file")
                break
            inspected += 1
            faces = pipeline.detect(bgr)
            status = f"faces={len(faces)} saved={successes}/{args.count}"
            valid = len(faces) == 1
            if valid:
                x, y, w, h = faces[0, :4]
                valid = min(w, h) >= args.min_face and blur_score(bgr) >= args.min_blur
            now = time.monotonic()
            if valid and (now - last_saved >= args.interval):
                path = args.output / f"{session}_{successes + 1:04d}.jpg"
                if not cv2.imwrite(str(path), bgr):
                    raise RuntimeError(f"Khong ghi duoc: {path}")
                successes += 1
                last_saved = now
                print(f"[CAPTURE] {successes}/{args.count}: {path}")
            if args.show:
                cv2.imshow("Enrollment capture (PC)", pipeline.draw_faces(bgr, faces, status))
                if (cv2.waitKey(1) & 0xFF) in (ord("q"), 27):
                    break
    finally:
        cap.release()
        if args.show:
            cv2.destroyAllWindows()
    print(f"[CAPTURE] completed: {successes} images, checked {inspected} frames")


def enroll(args) -> None:
    pipeline = get_pipeline(args)
    images = existing_images(args.input)
    if not images:
        raise FacePipelineError("Khong tim thay anh enrollment")
    accepted = []
    for path in images:
        bgr = cv2.imread(str(path))
        if bgr is None:
            print(f"[SKIP] invalid image: {path}")
            continue
        try:
            obs = pipeline.observe(bgr)
            if min(obs.face[2], obs.face[3]) < args.min_face:
                raise FacePipelineError("SMALL_FACE")
            accepted.append(obs.embedding)
        except FacePipelineError as exc:
            print(f"[SKIP] {path.name}: {exc}")
    if len(accepted) < args.min_samples:
        raise FacePipelineError(
            f"Chi nhan {len(accepted)} anh hop le (< {args.min_samples}); "
            "can them anh/mat ro net."
        )
    # Normalize per image -> mean -> normalize reference (prototype).
    mean_embedding = l2_normalize(np.mean(np.stack(accepted, axis=0), axis=0))
    save_reference(args.reference, mean_embedding, sample_count=len(accepted),
                   yunet_model=args.yunet, sface_model=args.sface)
    similarities = [cosine_similarity(embedding, mean_embedding) for embedding in accepted]
    print(f"[ENROLL] {len(accepted)}/{len(images)} images accepted")
    print(f"[ENROLL] dimension={mean_embedding.size}; cosine vs prototype: "
          f"min={min(similarities):.3f} mean={np.mean(similarities):.3f}")
    print(f"[ENROLL] saved {args.reference} and {args.reference.with_suffix('.json')}")


def verify(args) -> None:
    pipeline = get_pipeline(args)
    reference = get_reference(args)
    bgr = cv2.imread(str(args.input))
    if bgr is None:
        raise FileNotFoundError(f"Khong doc duoc anh: {args.input}")
    try:
        observation = pipeline.observe(bgr)
        score = cosine_similarity(reference, observation.embedding)
        result = "VERIFIED" if score >= args.threshold else "UNKNOWN"
    except FacePipelineError as exc:
        score, result = None, str(exc)
    print(f"[VERIFY] result={result} "
          f"cosine={score:.4f} threshold={args.threshold:.3f}" if score is not None
          else f"[VERIFY] result={result}")
    if args.output:
        faces = pipeline.detect(bgr)
        caption = f"{result}: {score:.3f}" if score is not None else result
        args.output.parent.mkdir(parents=True, exist_ok=True)
        cv2.imwrite(str(args.output), pipeline.draw_faces(bgr, faces, caption))
        print(f"[VERIFY] saved annotated image: {args.output}")


def live(args) -> None:
    pipeline = get_pipeline(args)
    reference = get_reference(args)
    cap = open_camera(args.source)
    print("[LIVE] ONNX on PC (not RKNN on device). q/ESC to exit GUI; Ctrl+C to exit terminal.")
    frames = 0
    last = time.monotonic()
    try:
        while True:
            ok, bgr = cap.read()
            if not ok:
                print("[LIVE] End of stream")
                break
            faces = pipeline.detect(bgr)
            if not len(faces):
                caption = "NO_FACE"
            elif len(faces) != 1:
                caption = f"MULTIPLE_FACES({len(faces)})"
            else:
                aligned = pipeline.recognizer.alignCrop(bgr, faces[0])
                feature = l2_normalize(pipeline.recognizer.feature(aligned))
                score = cosine_similarity(reference, feature)
                caption = f"{'VERIFIED' if score >= args.threshold else 'UNKNOWN'}  {score:.3f}"
            frames += 1
            elapsed = time.monotonic() - last
            if elapsed >= args.log_interval:
                print(f"[LIVE] frames={frames} faces={len(faces)} status={caption} "
                      f"avg_fps={frames / elapsed:.1f}")
                frames, last = 0, time.monotonic()
            if args.show:
                cv2.imshow("YuNet + SFace ONNX (PC)", pipeline.draw_faces(bgr, faces, caption))
                if (cv2.waitKey(1) & 0xFF) in (ord("q"), 27):
                    break
    finally:
        cap.release()
        if args.show:
            cv2.destroyAllWindows()


def evaluate(args) -> None:
    pipeline = get_pipeline(args)
    reference = get_reference(args)
    counts = {"TP": 0, "FN": 0, "FP": 0, "TN": 0}
    for label, directory in ((True, args.positive), (False, args.negative)):
        images = existing_images(directory)
        for path in images:
            bgr = cv2.imread(str(path))
            if bgr is None:
                continue
            try:
                candidate = pipeline.observe(bgr).embedding
                score = cosine_similarity(reference, candidate)
                predicted = score >= args.threshold
            except FacePipelineError:
                # Conservative: no face / >1 face => reject.
                predicted = False
            key = ("TP" if predicted else "FN") if label else ("FP" if predicted else "TN")
            counts[key] += 1
    p = counts["TP"] + counts["FN"]
    n = counts["FP"] + counts["TN"]
    if not p or not n:
        raise ValueError("Evaluate can ca anh positive va negative; khong dung anh enrollment de danh gia.")
    print(f"[EVAL] {counts} | FAR={counts['FP']/n:.4f} | FRR={counts['FN']/p:.4f} "
          f"| accuracy={(counts['TP']+counts['TN'])/(p+n):.4f}")
    print("[EVAL] Unknown image = rejected; FAR/FRR here are empirical on provided dataset.")


def run_quantize(args) -> None:
    from rknn_tools import quantize
    quantize(args)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Single-person verification: Ubuntu OpenCV YuNet + SFace ONNX."
    )
    parser.add_argument("--yunet", type=Path, default=ROOT / "Models/detectors/yunet.onnx")
    parser.add_argument("--sface", type=Path, default=ROOT / "Models/recognizers/sface.onnx")
    parser.add_argument("--score-threshold", type=float, default=0.75,
                        help="YuNet detection score minimum")
    parser.add_argument("--detector-side", type=int, default=640,
                        help="Downscale long side of input for YuNet detection; 0 invalid")
    parser.add_argument("--threshold", type=float, default=0.363,
                        help="Cosine threshold (OpenCV SFace demo starting point, tune on data)")
    parser.add_argument("--reference", type=Path, default=ROOT / "Embeddings/person_a.bin")
    parser.add_argument("--ignore-model-hash", action="store_true",
                        help="Skip SFace SHA256 reference check (for controlled experiments only)")
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("check", help="Check models and pre-collected SC3336 images")
    p.set_defaults(handler=check)
    p = sub.add_parser("inspect", help="Inspect existing Enrollment/Person_A images")
    p.add_argument("--input", type=Path, default=ROOT / "Enrollment/Person_A")
    p.add_argument("--min-face", type=int, default=48)
    p.add_argument("--min-blur", type=float, default=0,
                   help="Optional rough sharpness filter; 0 disables it")
    p.add_argument("--csv", type=Path, default=ROOT / "Docs/enrollment_inspection.csv")
    p.add_argument("--annotated-dir", type=Path,
                   help="Optional: export annotated images for visual inspection")
    p.set_defaults(handler=inspect)
    p = sub.add_parser("preview", help="YuNet detection preview on Ubuntu from RTSP/webcam")
    p.add_argument("--source", default=os.environ.get("LUCKFOX_RTSP_URL", "0"))
    p.add_argument("--show", action="store_true")
    p.add_argument("--log-interval", type=float, default=2.0)
    p.set_defaults(handler=preview)
    p = sub.add_parser("capture", help="Capture images from webcam or LuckFox RTSP")
    p.add_argument("--source", default=os.environ.get("LUCKFOX_RTSP_URL", "0"))
    p.add_argument("--output", type=Path, default=ROOT / "Enrollment/Person_A")
    p.add_argument("--count", type=int, default=200)
    p.add_argument("--interval", type=float, default=1.0)
    p.add_argument("--min-face", type=int, default=64)
    p.add_argument("--min-blur", type=float, default=25.0,
                   help="Variance-of-Laplacian threshold, 0 disables blur filtering")
    p.add_argument("--show", action="store_true", help="Enable GUI preview")
    p.set_defaults(handler=capture)
    p = sub.add_parser("enroll", help="Create one normalized reference embedding")
    p.add_argument("--input", type=Path, default=ROOT / "Enrollment/Person_A")
    p.add_argument("--min-samples", type=int, default=5)
    p.add_argument("--min-face", type=int, default=48)
    p.set_defaults(handler=enroll)
    p = sub.add_parser("verify", help="Compare one still image to reference")
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path)
    p.set_defaults(handler=verify)
    p = sub.add_parser("live", help="Live verify video on Ubuntu PC (OpenCV ONNX)")
    p.add_argument("--source", default=os.environ.get("LUCKFOX_RTSP_URL", "0"))
    p.add_argument("--show", action="store_true")
    p.add_argument("--log-interval", type=float, default=2.0)
    p.set_defaults(handler=live)
    p = sub.add_parser("evaluate", help="Test with separate genuine/impostor sets")
    p.add_argument("--positive", type=Path, required=True)
    p.add_argument("--negative", type=Path, required=True)
    p.set_defaults(handler=evaluate)
    p = sub.add_parser('quantize', help='RKNN INT8 calibration and convert for RV1106')
    p.add_argument('--stage', choices=['prepare', 'convert', 'all'], default='all',
                   help='prepare calibration PNGs, convert RKNN models, or both')
    p.add_argument('--model', choices=['both', 'yunet', 'sface'], default='both',
                   help='Models to convert (only for convert/all stage)')
    p.add_argument('--enrollment', type=Path, default=ROOT / 'Enrollment/Person_A')
    p.add_argument('--yunet-size', type=int, default=320,
                   help='NPU YuNet fixed input side (multiple of 32)')
    p.add_argument('--max-images', type=int, default=100)
    p.add_argument('--verbose', action='store_true')
    p.set_defaults(handler=run_quantize)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.detector_side < 32:
        parser.error("--detector-side >= 32")
    if not 0 <= args.threshold <= 1:
        parser.error("--threshold phai nam trong [0,1]")
    if args.command in ("inspect", "enroll") and args.min_face < 1:
        parser.error("--min-face >= 1")
    if args.command == "inspect" and args.min_blur < 0:
        parser.error("--min-blur >= 0")
    if args.command == "capture" and (args.count < 1 or args.interval < 0):
        parser.error("--count >= 1 va --interval >= 0")
    if args.command in ("live", "preview") and args.log_interval <= 0:
        parser.error("--log-interval > 0")
    if args.command == 'quantize':
        if args.max_images < 1:
            parser.error('--max-images >= 1')
        if args.yunet_size < 32 or args.yunet_size % 32:
            parser.error('--yunet-size phai chia het cho 32 va >=32')
    try:
        args.handler(args)
    except KeyboardInterrupt:
        print("\n[STOP] interrupted")
        return 130
    except (FileNotFoundError, FacePipelineError, RuntimeError, ValueError, cv2.error) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
