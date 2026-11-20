"""Reference embedding serialization for Python and later C++ interoperability."""
from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

from face_pipeline import l2_normalize


def file_sha256(path: Path) -> str:
    checksum = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def save_reference(output: Path, embedding: np.ndarray, *, sample_count: int,
                   yunet_model: Path, sface_model: Path) -> None:
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    vec = l2_normalize(embedding)
    # Raw float32 little-endian, no header: straightforward to read in C++.
    vec.astype("<f4").tofile(output)
    metadata = {
        "format": "raw-f32-le-l2norm-v1",
        "dimension": int(vec.size),
        "sample_count": int(sample_count),
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "detector": "YuNet/FaceDetectorYN",
        "recognizer": "SFace/FaceRecognizerSF",
        "yunet_sha256": file_sha256(yunet_model),
        "sface_sha256": file_sha256(sface_model),
        "note": "ONNX reference; compare with RKNN embeddings before use on device.",
    }
    output.with_suffix(".json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )


def load_reference(path: Path, *, sface_model: Path | None = None,
                   ignore_model_hash: bool = False) -> np.ndarray:
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"Chua co reference: {path}. Hay chay enroll truoc.")
    meta_path = path.with_suffix(".json")
    if not meta_path.is_file():
        raise ValueError(f"Thieu metadata {meta_path}; khong the kiem tra model/dimension")
    metadata = json.loads(meta_path.read_text(encoding="utf-8"))
    if metadata.get("format") != "raw-f32-le-l2norm-v1":
        raise ValueError("Dinh dang reference khong duoc ho tro")
    arr = np.fromfile(path, dtype="<f4")
    if arr.size != int(metadata["dimension"]):
        raise ValueError("Reference bi hong: dimension khong khop metadata")
    if sface_model is not None and not ignore_model_hash:
        actual = file_sha256(sface_model)
        if actual != metadata.get("sface_sha256"):
            raise ValueError(
                "Model SFace khac voi model da enrollment; "
                "dung dung ONNX model hoac enrollment lai."
            )
    return l2_normalize(arr)
