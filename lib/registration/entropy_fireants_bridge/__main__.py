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
import os
import shlex
import shutil
import subprocess
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


def _parameter_value(job: dict[str, Any], key: str, fallback: str) -> str:
    for value in job.get("parameterValues") or []:
        if value.get("key") == key and str(value.get("value") or ""):
            return str(value["value"])
    return fallback


def _transform_stages(job: dict[str, Any]) -> list[str]:
    model = str(job.get("transformModel") or "AffineDeformable")
    deformable = _parameter_value(job, "deformationType", "Greedy")
    if deformable.lower() not in {"greedy", "syn"}:
        deformable = "Greedy"
    deformable = "SyN" if deformable.lower() == "syn" else "Greedy"
    if model == "Rigid":
        return ["Rigid"]
    if model == "Affine":
        return ["Affine"]
    if model == "RigidAffine":
        return ["Rigid", "Affine"]
    if model == "Deformable":
        return [deformable]
    if model == "AffineDeformable":
        return ["Affine", deformable]
    raise ValueError(f"FireANTs bridge does not support transform model '{model}'.")


def _metric(job: dict[str, Any], fixed: str, moving: str) -> str:
    metric = str(job.get("metric") or "CC")
    if metric in {"MSE", "SSD"}:
        return f"MSE[{fixed},{moving}]"
    if metric == "MI":
        bins = _parameter_value(job, "miBins", "32")
        kernel = _parameter_value(job, "miKernel", "gaussian")
        return f"MI[{fixed},{moving},{kernel},{bins}]"
    if metric in {"CC", "NCC", "WNCC"}:
        kernel = _parameter_value(job, "ccKernelSize", "5")
        return f"CC[{fixed},{moving},{kernel}]"
    raise ValueError(f"FireANTs bridge does not support metric '{metric}' through the CLI adapter.")


def _transform_arg(job: dict[str, Any], stage: str) -> str:
    learning_rate = _parameter_value(job, "learningRate", "0.5")
    optimizer = _parameter_value(job, "optimizer", "Adam")
    if stage == "Rigid":
        return f"Rigid[{learning_rate},{optimizer}]"
    if stage == "Affine":
        return f"Affine[{learning_rate},{optimizer}]"
    smooth_warp = _parameter_value(job, "smoothWarpSigma", "0.25")
    smooth_grad = _parameter_value(job, "smoothGradSigma", "0.5")
    return f"{stage}[{learning_rate},{optimizer},{smooth_warp},{smooth_grad}]"


def _convergence_arg(job: dict[str, Any]) -> str:
    iterations = _parameter_value(job, "iterations", job.get("iterationSchedule") or "100x50x25")
    return (
        f"[{iterations},"
        f"{_parameter_value(job, 'convergenceThreshold', '1e-6')},"
        f"{_parameter_value(job, 'convergenceWindow', '10')}]"
    )


def _fireants_output_prefix(job: dict[str, Any]) -> Path:
    output_dir = Path(job.get("outputDirectory") or ".")
    prefix = job.get("outputPrefix") or "registration"
    return output_dir / f"{prefix}_fireants"


def _fireants_transform_path(job: dict[str, Any], final_stage: str) -> Path:
    prefix = _fireants_output_prefix(job)
    if final_stage in {"Rigid", "Affine"}:
        return Path(f"{prefix}0GenericAffine.txt")
    return Path(f"{prefix}0Warp.nii.gz")


def _build_fireants_command(job: dict[str, Any]) -> tuple[list[str], Path, Path, list[str]]:
    fixed = str((job.get("fixedImage") or {}).get("fileName") or "")
    moving = str((job.get("movingImage") or {}).get("fileName") or "")
    if not fixed or not moving:
        raise ValueError("FireANTs requires fixed and moving image file paths.")

    stages = _transform_stages(job)
    warped = Path(_artifact_path(job, "_warped.nii.gz"))
    output_prefix = _fireants_output_prefix(job)
    command = [
        shutil.which("fireantsRegistration") or "fireantsRegistration",
        "--output",
        f"{output_prefix},{warped}",
        "--device",
        _parameter_value(job, "device", "cuda:0"),
    ]
    if job.get("useImageCentersForInitialization", True):
        command += ["--initial-moving-transform", f"[{fixed},{moving},1]"]
    fixed_mask = str((job.get("fixedMask") or {}).get("fileName") or "")
    moving_mask = str((job.get("movingMask") or {}).get("fileName") or "")
    if fixed_mask or moving_mask:
        if not fixed_mask or not moving_mask:
            raise ValueError("FireANTs requires both fixed and moving masks when masks are used.")
        command += ["--mask", f"{fixed_mask},{moving_mask}"]
    if _parameter_value(job, "normalizeImageIntensities", "false").lower() in {"true", "1", "yes", "on"}:
        command.append("--normalize-image-intensities")
    winsorize = _parameter_value(job, "winsorizeImageIntensities", "")
    if winsorize:
        command += ["--winsorize-image-intensities", f"[{winsorize}]"]

    metric = _metric(job, fixed, moving)
    convergence = _convergence_arg(job)
    shrink = _parameter_value(job, "scales", "4x2x1")
    for stage in stages:
        command += ["--transform", _transform_arg(job, stage)]
        command += ["--metric", metric]
        command += ["--convergence", convergence]
        command += ["--shrink-factors", shrink]
    extra_args = _parameter_value(job, "extraArgs", "")
    if extra_args:
        command += shlex.split(extra_args)
    command += list(job.get("extraArguments") or [])
    return command, warped, _fireants_transform_path(job, stages[-1]), stages


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


def _fireants_manifest(job: dict[str, Any], success: bool, transform_path: Path | None = None, error: str = "") -> dict[str, Any]:
    manifest = _expected_manifest(job, success=success, error=error)
    outputs = job.get("outputs") or {}
    warnings = manifest["warnings"]
    manifest["forwardWarp"] = ""
    manifest["warpedSegmentations"] = []
    manifest["transformedLandmarks"] = []
    manifest["transformedSurfaces"] = []

    if transform_path and transform_path.exists():
        if transform_path.suffix == ".txt":
            manifest["affineTransform"] = str(transform_path)
            manifest["inverseWarp"] = ""
        else:
            expected_inverse = Path(_artifact_path(job, "_inverse_warp.nii.gz"))
            if transform_path != expected_inverse:
                expected_inverse.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(transform_path, expected_inverse)
            manifest["inverseWarp"] = str(expected_inverse)
            manifest["affineTransform"] = ""
    else:
        manifest["affineTransform"] = ""
        manifest["inverseWarp"] = ""

    if outputs.get("loadForwardWarp", True):
        warnings.append("FireANTs CLI does not currently produce a forward warp; this output was skipped.")
    if outputs.get("loadWarpedSegmentation", False):
        warnings.append("FireANTs CLI apply-transform support is not available yet; warped segmentation output was skipped.")
    if outputs.get("transformLandmarksAndAnnotations", True):
        warnings.append("FireANTs CLI landmark/annotation transform output is not available yet; this output was skipped.")
    if outputs.get("transformSurfaces", False):
        warnings.append("FireANTs CLI surface transform output is not available yet; this output was skipped.")
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

    if shutil.which("fireantsRegistration") is None:
        message = (
            "FireANTs is importable, but the fireantsRegistration executable is not on PATH. "
            "Install the FireANTs CLI entry points or add them to PATH."
        )
        manifest = _expected_manifest(job, success=False, error=message)
        _write_manifest(manifest_path, manifest, time.monotonic() - start)
        _emit("Failed", message=message, artifactPath=str(manifest_path), artifactKind="result_manifest")
        return 2

    try:
        command, warped_path, transform_path, stages = _build_fireants_command(job)
    except Exception as exc:
        message = str(exc)
        manifest = _expected_manifest(job, success=False, error=message)
        _write_manifest(manifest_path, manifest, time.monotonic() - start)
        _emit("Failed", message=message, artifactPath=str(manifest_path), artifactKind="result_manifest")
        return 2

    _emit("StageStarted", stageName="FireANTs registration", message="Launching fireantsRegistration.")
    env = os.environ.copy()
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=env,
    )
    assert process.stdout is not None
    for line in process.stdout:
        text = line.rstrip()
        if text:
            _emit("Progress", stageName="FireANTs registration", message=text)
    exit_code = process.wait()

    if exit_code != 0:
        message = f"fireantsRegistration failed with exit code {exit_code}."
        manifest = _fireants_manifest(job, success=False, transform_path=None, error=message)
        _write_manifest(manifest_path, manifest, time.monotonic() - start)
        _emit("Failed", message=message, artifactPath=str(manifest_path), artifactKind="result_manifest")
        return exit_code or 2

    if not warped_path.exists():
        message = f"fireantsRegistration completed but did not write warped image: {warped_path}"
        manifest = _fireants_manifest(job, success=False, transform_path=transform_path, error=message)
        _write_manifest(manifest_path, manifest, time.monotonic() - start)
        _emit("Failed", message=message, artifactPath=str(manifest_path), artifactKind="result_manifest")
        return 2

    manifest = _fireants_manifest(job, success=True, transform_path=transform_path)
    _write_manifest(manifest_path, manifest, time.monotonic() - start)
    _emit("Artifact", artifactPath=str(warped_path), artifactKind="warped_image")
    if manifest.get("inverseWarp"):
        _emit("Artifact", artifactPath=manifest["inverseWarp"], artifactKind="inverse_warp")
    _emit(
        "Completed",
        stageName="FireANTs registration",
        message=f"FireANTs registration completed with {len(stages)} stage(s).",
        artifactPath=str(manifest_path),
        artifactKind="result_manifest",
    )
    return 0


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
