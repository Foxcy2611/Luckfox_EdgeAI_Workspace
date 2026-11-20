"""Prepare calibration images and convert YuNet/SFace to RKNN INT8 for RV1106.

Run via main.py:
  python3 Scripts/main.py quantize --stage prepare
  python3 Scripts/main.py quantize --stage convert

Calibration RGB/BGR contracts:
- YuNet: OpenCV FaceDetectorYN uses BGR input, no scaling.
- SFace: OpenCV FaceRecognizerSF::feature uses RGB input, no scaling.
- RKNN dataset PNG decoder reads RGB. quant_img_RGB2BGR=True applies to
  calibration YuNet ONLY. C++ app must itself feed BGR (YuNet)/RGB (SFace).
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / 'Calibration'


def _sha256(path: Path):
    digest = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def _assert_file(path: Path):
    if not path.is_file():
        raise FileNotFoundError(f'Thieu file: {path}')


def prepare_calibration(args):
    """Create input images for each model from existing Enrollment/Person_A."""
    import cv2
    import numpy as np
    from dataset_tools import existing_images
    from face_pipeline import FacePipeline

    _assert_file(args.yunet)
    _assert_file(args.sface)
    images = existing_images(args.enrollment)
    if not images:
        raise RuntimeError(f'Enrollment rong: {args.enrollment}')

    yunet_dir = DATA_DIR / 'yunet'
    sface_dir = DATA_DIR / 'sface'
    yunet_dir.mkdir(parents=True, exist_ok=True)
    sface_dir.mkdir(parents=True, exist_ok=True)
    pipeline = FacePipeline(args.yunet, args.sface,
                            score_threshold=args.score_threshold,
                            max_detector_side=args.detector_side)
    yunet_records = []
    sface_records = []
    failed = []

    for idx, src in enumerate(images[:args.max_images], start=1):
        image = cv2.imread(str(src))
        if image is None:
            failed.append(f'{src.name}: khong doc duoc anh')
            continue
        # Detector calibration is the whole camera image, resized to NPU input.
        # OpenCV's cv2.imwrite persists real image colors; RKNN dataset reader
        # decodes RGB and quant_img_RGB2BGR=True restores Yunet BGR input.
        det = cv2.resize(image, (args.yunet_size, args.yunet_size),
                         interpolation=cv2.INTER_LINEAR)
        det_path = yunet_dir / f'frame_{idx:04d}.png'
        if not cv2.imwrite(str(det_path), det):
            raise OSError(f'Ghi anh calibration that bai: {det_path}')
        yunet_records.append(det_path.resolve())

        # SFace calibration MUST use aligned 112x112 face crop (not full frame).
        faces = pipeline.detect(image)
        if len(faces) != 1:
            failed.append(f'{src.name}: {len(faces)} face(s), skip SFace crop')
            continue
        aligned_bgr = pipeline.recognizer.alignCrop(image, faces[0])
        if aligned_bgr.shape[:2] != (112, 112):
            raise RuntimeError(f'SFace alignCrop tra ve shape sai: {aligned_bgr.shape}')
        # Standard PNG is saved from BGR via cv2.imwrite. When RKNN reads it as
        # RGB (quant_img_RGB2BGR=False), bytes match SFace RGB network input.
        emb_path = sface_dir / f'face_{idx:04d}.png'
        if not cv2.imwrite(str(emb_path), aligned_bgr):
            raise OSError(f'Ghi anh calibration that bai: {emb_path}')
        sface_records.append(emb_path.resolve())

    if not yunet_records or not sface_records:
        raise RuntimeError(f'Khong du calibration: YuNet={len(yunet_records)} '
                           f'SFace={len(sface_records)}. Kiem tra anh/model.')

    def write_dataset(name, paths):
        file = DATA_DIR / f'{name}.txt'
        file.write_text(''.join(f'{p}\n' for p in paths), encoding='utf-8')
        return file

    write_dataset('yunet', yunet_records)
    write_dataset('sface', sface_records)
    manifest = {
        'generated_at_utc': datetime.now(timezone.utc).isoformat(),
        'target_platform': 'rv1106',
        'quantization': 'INT8',
        'yunet_hw': [args.yunet_size, args.yunet_size],
        'sface_hw': [112, 112],
        'yunet_network_color': 'BGR',
        'sface_network_color': 'RGB',
        'mean': [0, 0, 0],
        'std': [1, 1, 1],
        'yunet_sha256': _sha256(args.yunet),
        'sface_sha256': _sha256(args.sface),
        'num_detector_images': len(yunet_records),
        'num_recognizer_images': len(sface_records),
        'skipped_recognizer_images': failed,
    }
    (DATA_DIR / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n',
                                               encoding='utf-8')
    print(f'[CALIB] YuNet: {len(yunet_records)} frames {args.yunet_size}x{args.yunet_size}')
    print(f'[CALIB] SFace: {len(sface_records)} aligned faces 112x112')
    print(f'[CALIB] Manifest: {DATA_DIR / "manifest.json"}')
    if failed:
        print('[CALIB] Skipped SFace:')
        for item in failed:
            print('  -', item)
    if len(sface_records) < 10:
        print('[WARN] SFace <10 calibration crops: INT8 sai so co the cao')
    return manifest


def ensure_fixed_onnx(model: Path, size: int, output: Path):
    """Make first ONNX image input static if its spatial dimensions are dynamic.

    For fixed-shape models, returns original model unchanged. This is a best
    effort conversion for dynamic YuNet. It does not change weights/operators;
    some graphs may still require onnxsim/ONNX graph repairs.
    """
    try:
        import onnx
    except ImportError as exc:
        raise RuntimeError('Thieu onnx trong moi truong toolkit: pip install onnx') from exc
    net = onnx.load(str(model))
    inputs = [x for x in net.graph.input if x.name not in {i.name for i in net.graph.initializer}]
    if len(inputs) != 1:
        raise RuntimeError(f'{model.name}: can 1 image input, hien co {len(inputs)}')
    input_val = inputs[0]
    dims = input_val.type.tensor_type.shape.dim
    if len(dims) != 4:
        raise RuntimeError(f'{model.name}: input must be NCHW 4D; got {len(dims)}D')
    # OpenCV ONNX image models use [N,C,H,W].
    for index, exp in enumerate((1, 3)):
        d = dims[index]
        current = d.dim_value if d.HasField('dim_value') else None
        if current not in (None, exp):
            raise RuntimeError(f'{model.name}: dimension {index}={current}, expected {exp}')
    current_hw = [d.dim_value if d.HasField('dim_value') else None for d in dims[2:4]]
    if all(d == size for d in current_hw):
        print(f'[ONNX] {model.name}: fixed shape 1x3x{size}x{size}')
        return model
    if any(d not in (None, size) for d in current_hw):
        # Do not silently reshape an already-static model to the wrong size.
        raise RuntimeError(f'{model.name}: ONNX fixed spatial dimensions {current_hw} '
                           f'khac {size}; dung --yunet-size {current_hw[0]} neu H=W')
    print(f'[ONNX] {model.name}: dynamic dimensions {current_hw} => static {size}x{size}')
    for idx, val in enumerate((1, 3, size, size)):
        dims[idx].Clear()
        dims[idx].dim_value = val
    # Drop output inferred shape information to permit refreshed inference.
    for vi in net.graph.value_info:
        if vi.type.HasField('tensor_type'):
            del vi.type.tensor_type.shape.dim[:]
    inferred = onnx.shape_inference.infer_shapes(net, strict_mode=False)
    onnx.checker.check_model(inferred)
    output.parent.mkdir(parents=True, exist_ok=True)
    onnx.save(inferred, str(output))
    return output


def convert_one(name: str, onnx_path: Path, dataset: Path, out_path: Path,
                input_size: int, rgb_to_bgr: bool, *, verbose=False):
    """Execute compiler, never claim that export proves board inference works."""
    try:
        from rknn.api import RKNN
    except ImportError as exc:
        raise RuntimeError('Khong import duoc RKNN Toolkit2. Chay trong env 1.6.0. '
                           'Thu: python -c "from rknn.api import RKNN"') from exc
    _assert_file(dataset)
    _assert_file(onnx_path)
    if not dataset.read_text(encoding='utf8').strip():
        raise RuntimeError(f'Calibration rong: {dataset}')
    print(f'\n[RKNN] ===== {name.upper()} =====', flush=True)
    print(f'[RKNN] ONNX       : {onnx_path}')
    print(f'[RKNN] Calibration: {dataset}')
    print(f'[RKNN] Output     : {out_path}')
    model_fixed = ensure_fixed_onnx(
        onnx_path, input_size,
        ROOT / 'Models' / 'rknn_build' / f'{name}_fixed_{input_size}.onnx')
    rknn = RKNN(verbose=verbose)
    try:
        # IMPORTANT: first bool changes calibration dataset channel order only.
        # It is NOT stored as an automatic runtime RGB/BGR conversion.
        ret = rknn.config(
            mean_values=[[0, 0, 0]], std_values=[[1, 1, 1]],
            target_platform='rv1106', quantized_dtype='asymmetric_quantized-8',
            quant_img_RGB2BGR=rgb_to_bgr)
        if ret not in (None, 0):
            raise RuntimeError(f'{name}: rknn.config failed ({ret})')
        ret = rknn.load_onnx(model=str(model_fixed))
        if ret != 0:
            raise RuntimeError(f'{name}: rknn.load_onnx failed ({ret})')
        ret = rknn.build(do_quantization=True, dataset=str(dataset))
        if ret != 0:
            raise RuntimeError(f'{name}: rknn.build failed ({ret})')
        out_path.parent.mkdir(parents=True, exist_ok=True)
        ret = rknn.export_rknn(str(out_path))
        if ret != 0:
            raise RuntimeError(f'{name}: rknn.export_rknn failed ({ret})')
        print(f'[RKNN] EXPORTED {out_path} ({out_path.stat().st_size:,} bytes)')
    finally:
        rknn.release()


def convert_models(args):
    manifest_path = DATA_DIR / 'manifest.json'
    _assert_file(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding='utf8'))
    # Avoid calibration/model mismatch from another download/version.
    for label, path in (('yunet', args.yunet), ('sface', args.sface)):
        _assert_file(path)
        if _sha256(path) != manifest.get(f'{label}_sha256'):
            raise RuntimeError(f'{label}: model hash khong khop calibration; chay prepare lai')
    if args.yunet_size != manifest['yunet_hw'][0]:
        raise RuntimeError('Kich thuoc YuNet thay doi; chay prepare lai')

    specs = [
        ('yunet', args.yunet, DATA_DIR/'yunet.txt',
         ROOT/'Models/detectors/yunet.rknn', args.yunet_size, True),
        ('sface', args.sface, DATA_DIR/'sface.txt',
         ROOT/'Models/recognizers/sface.rknn', 112, False),
    ]
    for name, model, dataset, output, size, rgb_to_bgr in specs:
        if args.model in ('both', name):
            convert_one(name, model, dataset, output, size, rgb_to_bgr,
                        verbose=args.verbose)
    print('\n[RKNN] Models exported. Next: inspect outputs + integrate C++ on LuckFox.')
    print('[IMPORTANT] C++ YuNet input = BGR, SFace input = RGB (112x112 aligned).')
    print('[IMPORTANT] Keep SFace reference validation; current person_a.bin is ONNX.')


def quantize(args):
    if args.stage in ('prepare', 'all'):
        prepare_calibration(args)
    if args.stage in ('convert', 'all'):
        convert_models(args)
