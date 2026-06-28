"""Command-line bridge between Entropy and FireANTs.

The C++ app launches this module as:

    python -m entropy_fireants_bridge run <job.json>

The bridge owns FireANTs/Python-specific concerns and communicates back to
Entropy through JSON-lines progress events and a result manifest JSON file.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
import time
from pathlib import Path
from typing import Any


def _emit(event: str, **fields: Any) -> None:
    payload = {"event": event}
    payload.update({key: value for key, value in fields.items() if value is not None})
    print(json.dumps(payload, separators=(",", ":")), flush=True)


def _artifact_path(job: dict[str, Any], suffix: str) -> str:
    output_dir = Path(job.get("outputDirectory") or ".")
    prefix = job.get("outputPrefix") or "registration"
    return str(output_dir / f"{prefix}{suffix}")


def _expected_manifest(job: dict[str, Any], success: bool, error: str = "") -> dict[str, Any]:
    outputs = job.get("outputs") or {}
    manifest = {
        "version": 1,
        "success": success,
        "backend": "FireANTs",
        "fixedImageUid": (job.get("fixedImage") or {}).get("uid", ""),
        "movingImageUid": (job.get("movingImage") or {}).get("uid", ""),
        "warpedImage": "",
        "inverseWarp": "",
        "forwardWarp": "",
        "affineTransform": _artifact_path(job, "_affine.mat"),
        "warpedSegmentations": [],
        "transformedSurfaces": [],
        "transformedLandmarks": [],
        "warnings": [],
        "errorMessage": error,
        "warpConvention": "inverse_warp_samples_moving_from_fixed_space",
        "elapsedSeconds": 0.0,
    }
    if outputs.get("loadWarpedImage", True):
        manifest["warpedImage"] = _artifact_path(job, "_warped.nii.gz")
    if outputs.get("loadInverseWarp", True) or outputs.get("applyWarpToMovingImage", True):
        manifest["inverseWarp"] = _artifact_path(job, "_inverse_warp.nii.gz")
    if (
        outputs.get("loadForwardWarp", True)
        or outputs.get("transformLandmarksAndAnnotations", True)
        or outputs.get("transformSurfaces", False)
    ):
        manifest["forwardWarp"] = _artifact_path(job, "_forward_warp.nii.gz")
    if outputs.get("loadWarpedSegmentation", False):
        manifest["warpedSegmentations"].append(_artifact_path(job, "_warped_segmentation_01.nii.gz"))
    if outputs.get("transformLandmarksAndAnnotations", True):
        manifest["transformedLandmarks"].append(_artifact_path(job, "_transformed_landmarks_01.json"))
    if outputs.get("transformSurfaces", False):
        manifest["transformedSurfaces"].append(_artifact_path(job, "_transformed_surface_01.vtk"))
    return manifest


def _result_manifest_path(job_path: Path, job: dict[str, Any]) -> Path:
    output_dir = Path(job.get("outputDirectory") or job_path.parent)
    prefix = job.get("outputPrefix") or "registration"
    return output_dir / f"{prefix}_result.json"


def _write_manifest(path: Path, manifest: dict[str, Any], elapsed_seconds: float) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    manifest["elapsedSeconds"] = elapsed_seconds
    path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")


def run(job_path: Path) -> int:
    start = time.monotonic()
    _emit("Started", message="Starting FireANTs bridge.")

    try:
        job = json.loads(job_path.read_text(encoding="utf-8"))
    except Exception as exc:
        _emit("Failed", message=f"Unable to read FireANTs job spec: {exc}")
        return 2

    manifest_path = _result_manifest_path(job_path, job)
    fireants_available = importlib.util.find_spec("fireants") is not None
    if not fireants_available:
        message = (
            "FireANTs is not importable in this Python environment. "
            "Install FireANTs and run Entropy with that Python executable selected."
        )
        manifest = _expected_manifest(job, success=False, error=message)
        _write_manifest(manifest_path, manifest, time.monotonic() - start)
        _emit("Failed", message=message, artifactPath=str(manifest_path), artifactKind="result_manifest")
        return 2

    message = "FireANTs is available, but the concrete registration adapter is not implemented yet."
    manifest = _expected_manifest(job, success=False, error=message)
    manifest["warnings"].append("Entropy FireANTs bridge reached the unimplemented adapter boundary.")
    _write_manifest(manifest_path, manifest, time.monotonic() - start)
    _emit("Failed", message=message, artifactPath=str(manifest_path), artifactKind="result_manifest")
    return 2


def check() -> int:
    if importlib.util.find_spec("fireants") is None:
        print("FireANTs is not importable in this Python environment.", file=sys.stderr)
        return 2
    print("FireANTs is importable.")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Entropy FireANTs bridge")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("check", help="check whether FireANTs is importable")
    run_parser = subparsers.add_parser("run", help="run one Entropy registration job")
    run_parser.add_argument("job", type=Path, help="Entropy registration job JSON file")
    args = parser.parse_args(argv)

    if args.command == "check":
        return check()
    if args.command == "run":
        return run(args.job)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
