"""FireANTs registration bridge used by Entropy.

This module contains the implementation for Entropy's FireANTs subprocess
adapter. The package entry point in ``__main__.py`` delegates here so the C++
application can launch it as::

    python -m fireants_bridge run <job.json>

The bridge translates Entropy job JSON into ``fireantsRegistration`` arguments,
applies narrow compatibility patches for FireANTs runtime behavior, streams
JSON-lines progress events, and writes a result manifest consumed by Entropy.
"""

from __future__ import annotations

import argparse
import importlib.metadata
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
    """Emit one JSON-lines event to stdout for Entropy to consume."""
    payload = {"event": event}
    payload.update({key: value for key, value in fields.items() if value is not None})
    print(json.dumps(payload, separators=(",", ":")), flush=True)


def _artifact_path(job: dict[str, Any], suffix: str) -> str:
    """Return the output artifact path for a job-specific suffix."""
    output_dir = Path(job.get("outputDirectory") or ".")
    prefix = job.get("outputPrefix") or "registration"
    return str(output_dir / f"{prefix}{suffix}")


def _parameter_value(job: dict[str, Any], key: str, fallback: str) -> str:
    """Look up a string parameter value from the Entropy job, falling back when absent or empty."""
    for value in job.get("parameterValues") or []:
        if value.get("key") == key and str(value.get("value") or ""):
            return str(value["value"])
    return fallback


def _choice_number(value: str) -> str:
    """Extract the FireANTs numeric choice from a UI label such as "Principal axes (2)"."""
    text = str(value or "").strip()
    if text in {"1", "2", "3", "4"}:
        return text
    if "(1)" in text:
        return "1"
    if "(2)" in text:
        return "2"
    if "(3)" in text:
        return "3"
    if "(4)" in text:
        return "4"
    return ""


def _truthy(value: str) -> bool:
    """Return whether a serialized boolean-like value is enabled."""
    return str(value or "").strip().lower() in {"true", "1", "yes", "on"}


def _bracket_pair(value: str) -> str:
    """Normalize vector-style CLI values to the comma-separated format expected by FireANTs source."""
    text = str(value or "").strip()
    if text.startswith("[") and text.endswith("]"):
        text = text[1:-1]
    text = text.replace("x", ",").replace("X", ",")
    return text


def _selected_deformable(job: dict[str, Any]) -> str:
    """Return the selected FireANTs deformable class name for deformable stages."""
    deformable = _parameter_value(job, "deformableTransform", "")
    if not deformable:
        deformable = _parameter_value(job, "deformationType", "Greedy")
    return "SyN" if deformable.lower() == "syn" else "Greedy"


def _moment_type(job: dict[str, Any]) -> str:
    """Map Entropy moment-initialization parameters to FireANTs initial-moving-transform type numbers."""
    legacy = _choice_number(_parameter_value(job, "initialMovingTransform", ""))
    scaling = _truthy(_parameter_value(job, "momentPerformScaling", "false"))
    if legacy == "1" and not scaling:
        return "1"
    order = "2" if scaling else _parameter_value(job, "momentOrder", "2")
    if order == "1":
        return "1"
    orientation = _parameter_value(job, "momentOrientation", "").lower()
    if legacy == "3" and orientation in {"", "rot"}:
        orientation = "antirot"
    elif legacy == "4" and orientation in {"", "rot"}:
        orientation = "both"
    if orientation == "antirot":
        return "3"
    if orientation == "both":
        return "4"
    return "2"


def _moment_config(job: dict[str, Any]) -> dict[str, Any]:
    """Build keyword configuration used when patching FireANTs MomentsRegistration."""
    moment_type = _moment_type(job)
    if moment_type == "1":
        moments = 1
        orientation = "rot"
    elif moment_type == "3":
        moments = 2
        orientation = "antirot"
    elif moment_type == "4":
        moments = 2
        orientation = "both"
    else:
        moments = 2
        orientation = "rot"
    perform_scaling = _truthy(_parameter_value(job, "momentPerformScaling", "false"))
    if perform_scaling:
        moments = 2
    return {
        "moments": moments,
        "orientation": orientation,
        "perform_scaling": perform_scaling,
        "scale": _parameter_value(job, "momentScale", "1"),
        "loss_type": _parameter_value(job, "momentLossType", "cc").lower(),
        "cc_kernel_size": _parameter_value(job, "ccKernelSize", "5"),
        "cc_kernel_type": _parameter_value(job, "ccKernelType", "rectangular"),
        "mi_kernel_type": _parameter_value(job, "miKernel", "gaussian"),
        "mi_bins": _parameter_value(job, "miBins", "32"),
    }


def _uses_rich_moments(job: dict[str, Any]) -> bool:
    """Return whether moment initialization needs constructor options unavailable through the stock CLI."""
    cfg = _moment_config(job)
    return cfg["perform_scaling"] or str(cfg["scale"]) != "1" or cfg["loss_type"] != "cc"


def _deformable_optimizer(job: dict[str, Any], fallback: str) -> str:
    """Resolve the deformable-stage optimizer, including compatibility with old saved project values."""
    value = _parameter_value(job, "deformableOptimizer", "Use optimizer setting")
    if value in {"Use optimizer setting", "Same as optimizer"}:
        return fallback
    if value == "Levenberg-Marquardt":
        return "Levenberg"
    return value


def _uses_levenberg_marquardt(job: dict[str, Any]) -> bool:
    """Return whether the job requests the Levenberg-Marquardt deformable optimizer."""
    return _parameter_value(job, "deformableOptimizer", "Use optimizer setting") == "Levenberg-Marquardt"


def _levenberg_config(job: dict[str, Any]) -> dict[str, Any]:
    """Collect Levenberg-Marquardt damping parameters from the Entropy job."""
    return {
        "lambda_init": _parameter_value(job, "levenbergLambdaInit", "1e-2"),
        "lambda_increase_factor": _parameter_value(job, "levenbergLambdaIncrease", "1.5"),
        "lambda_decrease_factor": _parameter_value(job, "levenbergLambdaDecrease", "0.975"),
    }


def _transform_stages(job: dict[str, Any]) -> list[str]:
    """Expand Entropy transform-model selection into ordered FireANTs registration stages."""
    model = str(job.get("transformModel") or "AffineDeformable")
    deformable = _selected_deformable(job)
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
    if model == "RigidAffineDeformable":
        return ["Rigid", "Affine", deformable]
    raise ValueError(f"FireANTs bridge does not support transform model '{model}'.")


def _has_deformable_stage(job: dict[str, Any]) -> bool:
    """Return whether the selected transform model includes a dense deformable stage."""
    return str(job.get("transformModel") or "AffineDeformable") in {
        "Deformable",
        "AffineDeformable",
        "RigidAffineDeformable",
        "BSplineDisplacement",
        "GaussianDisplacement",
        "TimeVaryingVelocity",
    }


def _metric(job: dict[str, Any], fixed: str, moving: str, masked: bool = False) -> str:
    """Build a FireANTs metric expression from Entropy metric and mask settings."""
    metric = str(job.get("metric") or "CC")
    if metric in {"MSE", "SSD"}:
        loss_override = ",loss_type=masked_mse" if masked else ""
        return f"MSE[{fixed},{moving}{loss_override}]"
    if metric == "MI":
        if masked:
            raise ValueError("FireANTs masked registration is documented for CC and MSE metrics; choose CC or MSE when using masks.")
        bins = _parameter_value(job, "miBins", "32")
        kernel = _parameter_value(job, "miKernel", "gaussian")
        return f"MI[{fixed},{moving},{kernel},{bins}]"
    if metric == "CC":
        kernel = _parameter_value(job, "ccKernelSize", "5")
        kernel_type = _parameter_value(job, "ccKernelType", "rectangular")
        loss_override = ",loss_type=masked_cc" if masked else ""
        return f"CC[{fixed},{moving},{kernel},cc_kernel_type={kernel_type}{loss_override}]"
    raise ValueError(f"FireANTs bridge does not support metric '{metric}' through the CLI adapter.")


def _stage_values(job: dict[str, Any], key: str) -> list[str]:
    """Split an expert semicolon-separated per-stage override parameter into values."""
    value = _parameter_value(job, key, "")
    return [part.strip() for part in value.split(";") if part.strip()]


def _stage_value(job: dict[str, Any], key: str, index: int) -> str:
    """Return the stage-specific override value, reusing the final override for later stages."""
    values = _stage_values(job, key)
    if not values:
        return ""
    return values[index] if index < len(values) else values[-1]


def _metric_from_token(job: dict[str, Any], token: str, fixed: str, moving: str, masked: bool) -> str:
    """Convert a shorthand or full expert metric token into a FireANTs metric expression."""
    token = token.strip()
    if not token:
        return _metric(job, fixed, moving, masked)
    if "[" in token:
        return token.replace("{fixed}", fixed).replace("{moving}", moving)
    stage_job = dict(job)
    stage_job["metric"] = token.upper()
    return _metric(stage_job, fixed, moving, masked)


def _stage_metric(job: dict[str, Any], index: int, fixed: str, moving: str, masked: bool) -> str:
    """Return the FireANTs metric expression for one registration stage."""
    return _metric_from_token(job, _stage_value(job, "stageMetrics", index), fixed, moving, masked)


def _stage_transform_arg(job: dict[str, Any], index: int, stage: str) -> str:
    """Return the FireANTs transform expression for one registration stage."""
    return _stage_value(job, "stageTransforms", index) or _transform_arg(job, stage)


def _stage_convergence_arg(job: dict[str, Any], index: int) -> str:
    """Return the FireANTs convergence expression for one registration stage."""
    value = _stage_value(job, "stageConvergence", index)
    if not value:
        return _convergence_arg(job)
    return value if value.startswith("[") else f"[{value}]"


def _stage_shrink_factors(job: dict[str, Any], index: int) -> str:
    """Return the shrink-factor schedule for one registration stage."""
    return _stage_value(job, "stageShrinkFactors", index) or _parameter_value(job, "scales", "4x2x1")


def _rigid_affine_common_options(job: dict[str, Any], stage: str) -> list[str]:
    """Return FireANTs key-value options shared by rigid and affine stages."""
    options = []
    if _truthy(_parameter_value(job, "centerOfFrameInitialization", "false")):
        options.append("init_translation=cof" if stage == "Rigid" else "init_rigid=cof")
    options.append(f"around_center={1 if _truthy(_parameter_value(job, 'aroundCenter', 'true')) else 0}")
    options.append(f"blur={1 if _truthy(_parameter_value(job, 'blurImages', 'true')) else 0}")
    return options


def _transform_arg(job: dict[str, Any], stage: str) -> str:
    """Build the default FireANTs transform expression for a structured Entropy stage."""
    learning_rate = _parameter_value(job, "learningRate", "0.5")
    optimizer = _parameter_value(job, "optimizer", "Adam")
    if stage == "Rigid":
        scaling = "1" if _truthy(_parameter_value(job, "rigidScaling", "false")) else "0"
        parts = [learning_rate, optimizer, scaling] + _rigid_affine_common_options(job, stage)
        return f"Rigid[{','.join(parts)}]"
    if stage == "Affine":
        parts = [learning_rate, optimizer] + _rigid_affine_common_options(job, stage)
        return f"Affine[{','.join(parts)}]"
    deformable_optimizer = _deformable_optimizer(job, optimizer)
    smooth_warp = _parameter_value(job, "smoothWarpSigma", "0.25")
    smooth_grad = _parameter_value(job, "smoothGradSigma", "0.5")
    deformation_model = _parameter_value(job, "deformationModel", "compositive").lower()
    if deformation_model not in {"compositive", "geodesic"}:
        deformation_model = "compositive"
    parts = [
        learning_rate,
        deformable_optimizer,
        smooth_warp,
        smooth_grad,
        f"deformation_type={deformation_model}",
        f"reduction={_parameter_value(job, 'lossReduction', 'mean')}",
        f"blur={1 if _truthy(_parameter_value(job, 'blurImages', 'true')) else 0}",
    ]
    if stage == "Greedy":
        parts.append(f"freeform={1 if _truthy(_parameter_value(job, 'greedyFreeform', 'false')) else 0}")
    if deformation_model == "geodesic":
        integrator_n = _parameter_value(job, "integratorN", "10")
        parts.append(f"integrator_n={integrator_n}")
    return f"{stage}[{','.join(parts)}]"


def _convergence_arg(job: dict[str, Any]) -> str:
    """Build the default FireANTs convergence expression from Entropy iteration controls."""
    iterations = _parameter_value(job, "iterations", job.get("iterationSchedule") or "128x64x32")
    return (
        f"[{iterations},"
        f"{_parameter_value(job, 'convergenceThreshold', '1e-6')},"
        f"{_parameter_value(job, 'convergenceWindow', '10')}]"
    )


def _fireants_output_prefix(job: dict[str, Any]) -> Path:
    """Return the FireANTs-native output prefix path for this job."""
    output_dir = Path(job.get("outputDirectory") or ".")
    prefix = job.get("outputPrefix") or "registration"
    return output_dir / f"{prefix}_fireants"


def _fireants_transform_path(job: dict[str, Any], final_stage: str) -> Path:
    """Return the final transform path FireANTs is expected to write for the last stage."""
    prefix = _fireants_output_prefix(job)
    if final_stage in {"Rigid", "Affine"}:
        return Path(f"{prefix}0GenericAffine.txt")
    return Path(f"{prefix}0Warp.nii.gz")


def _read_registration_image(path: str):
    """Read an image with SimpleITK, reporting a bridge-specific dependency error if unavailable."""
    try:
        import SimpleITK as sitk
    except ImportError as exc:
        raise RuntimeError("FireANTs masked registration requires SimpleITK in the FireANTs Python environment.") from exc
    return sitk, sitk.ReadImage(path)


def _ones_mask_like(sitk: Any, image: Any) -> Any:
    """Create an all-ones mask with geometry matching the given SimpleITK image."""
    mask = sitk.Image(image.GetSize(), sitk.sitkFloat32)
    mask.CopyInformation(image)
    mask += 1.0
    return mask


def _validate_mask_geometry(image: Any, mask: Any, label: str) -> None:
    """Validate that a mask image has the same physical grid as its paired intensity image."""
    if image.GetDimension() != mask.GetDimension() or image.GetSize() != mask.GetSize():
        raise ValueError(f"FireANTs {label} mask geometry does not match its image size.")
    if image.GetSpacing() != mask.GetSpacing() or image.GetOrigin() != mask.GetOrigin() or image.GetDirection() != mask.GetDirection():
        raise ValueError(f"FireANTs {label} mask geometry does not match its image spacing/origin/direction.")


def _compose_masked_input(image_path: str, mask_path: str, output_path: Path, label: str) -> str:
    """Write a two-channel image-plus-mask file for FireANTs documented masked-loss mode."""
    sitk, image = _read_registration_image(image_path)
    if mask_path:
        mask = sitk.ReadImage(mask_path)
        _validate_mask_geometry(image, mask, label)
    else:
        mask = _ones_mask_like(sitk, image)
    composed = sitk.Compose(sitk.Cast(image, sitk.sitkFloat32), sitk.Cast(mask, sitk.sitkFloat32))
    composed.CopyInformation(image)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sitk.WriteImage(composed, str(output_path))
    return str(output_path)


def _prepare_masked_metric_inputs(job: dict[str, Any], fixed: str, moving: str) -> tuple[str, str, bool]:
    """Prepare metric image paths, composing mask channels when either mask is present."""
    fixed_mask = str((job.get("fixedMask") or {}).get("fileName") or "")
    moving_mask = str((job.get("movingMask") or {}).get("fileName") or "")
    if not fixed_mask and not moving_mask:
        return fixed, moving, False
    output_prefix = _fireants_output_prefix(job)
    fixed_masked = _compose_masked_input(fixed, fixed_mask, Path(f"{output_prefix}_fixed_masked.nii.gz"), "fixed")
    moving_masked = _compose_masked_input(moving, moving_mask, Path(f"{output_prefix}_moving_masked.nii.gz"), "moving")
    return fixed_masked, moving_masked, True


def _find_fireants_registration() -> str | None:
    """Locate the fireantsRegistration executable beside the selected Python or on PATH."""
    executable = shutil.which("fireantsRegistration")
    if executable:
        return executable
    sibling = Path(sys.executable).parent / "fireantsRegistration"
    if sibling.is_file() and os.access(sibling, os.X_OK):
        return str(sibling)
    return None


def _fireants_runpy_cli_path(cli_path: str) -> str:
    """Resolve shell-script launch shims to the real Python CLI script for runpy execution."""
    path = Path(cli_path)
    try:
        first_lines = path.read_text(encoding="utf-8", errors="replace").splitlines()[:4]
    except OSError:
        return cli_path
    for line in first_lines:
        text = line.strip()
        if text.startswith("exec ") and "fireantsRegistration" in text:
            parts = shlex.split(text)
            for part in parts[2:]:
                if part == "$@":
                    break
                if part.endswith("fireantsRegistration") and Path(part).is_file():
                    return part
    return cli_path


def _write_fireants_wrapper(
    job: dict[str, Any], cli_path: str, patch_moments: bool, patch_levenberg: bool, patch_downsample: bool
) -> str:
    """Write a temporary Python wrapper that applies Entropy compatibility patches before running FireANTs."""
    output_prefix = _fireants_output_prefix(job)
    wrapper_path = Path(f"{output_prefix}_wrapper.py")
    wrapper_path.parent.mkdir(parents=True, exist_ok=True)
    wrapper_path.write_text(
        "import json, runpy, sys\n"
        "config = json.loads(sys.argv[1])\n"
        "cli_path = sys.argv[2]\n"
        "if config.get('patch_downsample'):\n"
        "    import fireants.registration.abstract as abstract_module\n"
        "    import fireants.utils.imageutils as imageutils_module\n"
        "    _entropy_original_downsample = abstract_module.downsample\n"
        "    def _entropy_safe_downsample(image, size, mode, sigma=None, gaussians=None, use_fft=True):\n"
        "        source = list(image.shape[2:])\n"
        "        target = list(size)\n"
        "        if use_fft and any(t > s for t, s in zip(target, source)):\n"
        "            print(f'Entropy FireANTs bridge: using non-FFT downsample for source {source} -> target {target}.', flush=True)\n"
        "            return imageutils_module.downsample(image, size=size, mode=mode, sigma=sigma, gaussians=gaussians, use_fft=False)\n"
        "        return _entropy_original_downsample(image, size=size, mode=mode, sigma=sigma, gaussians=gaussians, use_fft=use_fft)\n"
        "    abstract_module.downsample = _entropy_safe_downsample\n"
        "if config.get('patch_moments'):\n"
        "    import fireants.registration.moments as moments_module\n"
        "    OriginalMomentsRegistration = moments_module.MomentsRegistration\n"
        "    class EntropyMomentsRegistration(OriginalMomentsRegistration):\n"
        "        def __init__(self, *args, **kwargs):\n"
        "            moment = config['moment']\n"
        "            kwargs['scale'] = float(moment['scale'])\n"
        "            kwargs['moments'] = int(moment['moments'])\n"
        "            kwargs['orientation'] = moment['orientation']\n"
        "            kwargs['perform_scaling'] = bool(moment['perform_scaling'])\n"
        "            kwargs['loss_type'] = moment['loss_type']\n"
        "            if moment['loss_type'] == 'cc':\n"
        "                kwargs['cc_kernel_size'] = int(moment['cc_kernel_size'])\n"
        "                kwargs['cc_kernel_type'] = moment['cc_kernel_type']\n"
        "            elif moment['loss_type'] == 'mi':\n"
        "                kwargs['mi_kernel_type'] = moment['mi_kernel_type']\n"
        "                kwargs['loss_params'] = {'num_bins': int(moment['mi_bins'])}\n"
        "            super().__init__(*args, **kwargs)\n"
        "    moments_module.MomentsRegistration = EntropyMomentsRegistration\n"
        "if config.get('patch_levenberg'):\n"
        "    def _lm_params(existing=None):\n"
        "        params = dict(existing or {})\n"
        "        lm = config['levenberg']\n"
        "        lambda_init = lm['lambda_init']\n"
        "        if lambda_init != 'auto':\n"
        "            lambda_init = float(lambda_init)\n"
        "        params.update({\n"
        "            'lambda_init': lambda_init,\n"
        "            'lambda_increase_factor': float(lm['lambda_increase_factor']),\n"
        "            'lambda_decrease_factor': float(lm['lambda_decrease_factor']),\n"
        "        })\n"
        "        return params\n"
        "    import fireants.registration.greedy as greedy_module\n"
        "    OriginalGreedyRegistration = greedy_module.GreedyRegistration\n"
        "    class EntropyGreedyRegistration(OriginalGreedyRegistration):\n"
        "        def __init__(self, *args, **kwargs):\n"
        "            if str(kwargs.get('optimizer', '')).lower() == 'levenberg':\n"
        "                kwargs['optimizer_params'] = _lm_params(kwargs.get('optimizer_params'))\n"
        "            super().__init__(*args, **kwargs)\n"
        "    greedy_module.GreedyRegistration = EntropyGreedyRegistration\n"
        "    import fireants.registration.syn as syn_module\n"
        "    OriginalSyNRegistration = syn_module.SyNRegistration\n"
        "    class EntropySyNRegistration(OriginalSyNRegistration):\n"
        "        def __init__(self, *args, **kwargs):\n"
        "            if str(kwargs.get('optimizer', '')).lower() == 'levenberg':\n"
        "                kwargs['optimizer_params'] = _lm_params(kwargs.get('optimizer_params'))\n"
        "            super().__init__(*args, **kwargs)\n"
        "    syn_module.SyNRegistration = EntropySyNRegistration\n"
        "sys.argv = [cli_path] + sys.argv[3:]\n"
        "runpy.run_path(cli_path, run_name='__main__')\n"
    )
    return str(wrapper_path)

def _build_fireants_command(job: dict[str, Any]) -> tuple[list[str], Path, Path, list[str]]:
    """Translate an Entropy job dictionary into the subprocess command and expected output paths."""
    fixed = str((job.get("fixedImage") or {}).get("fileName") or "")
    moving = str((job.get("movingImage") or {}).get("fileName") or "")
    if not fixed or not moving:
        raise ValueError("FireANTs requires fixed and moving image file paths.")

    stages = _transform_stages(job)
    warped = Path(_artifact_path(job, "_warped.nii.gz"))
    output_prefix = _fireants_output_prefix(job)
    fireants_registration = _find_fireants_registration() or "fireantsRegistration"
    command = [
        fireants_registration,
        "--output",
        f"{output_prefix},{warped}",
        "--device",
        _parameter_value(job, "device", "cuda:0"),
    ]
    initial_affine = str(job.get("initialAffineTransform") or "")
    if initial_affine:
        raise ValueError("FireANTs bridge does not support initial affine transform files yet.")
    metric_fixed, metric_moving, masked_metric = _prepare_masked_metric_inputs(job, fixed, moving)
    moment_requested = False
    if initial_affine:
        pass
    else:
        moment_init = _choice_number(_parameter_value(job, "initialMovingTransform", ""))
        if not moment_init and job.get("useImageCentersForInitialization", False):
            moment_init = "1"
        if moment_init:
            moment_init = _moment_type(job)
            moment_requested = True
            command += ["--initial-moving-transform", f"[{metric_fixed},{metric_moving},{moment_init}]"]
    patch_moments = moment_requested and _uses_rich_moments(job)
    patch_levenberg = _uses_levenberg_marquardt(job)
    patch_downsample = True
    if patch_moments or patch_levenberg or patch_downsample:
        wrapper = _write_fireants_wrapper(
            job, _fireants_runpy_cli_path(fireants_registration), patch_moments, patch_levenberg, patch_downsample
        )
        wrapper_config = {
            "patch_moments": patch_moments,
            "moment": _moment_config(job),
            "patch_levenberg": patch_levenberg,
            "levenberg": _levenberg_config(job),
            "patch_downsample": patch_downsample,
        }
        command = [sys.executable, wrapper, json.dumps(wrapper_config), _fireants_runpy_cli_path(fireants_registration)] + command[1:]
    if _parameter_value(job, "normalizeImageIntensities", "false").lower() in {"true", "1", "yes", "on"}:
        command.append("--normalize-image-intensities")
    winsorize = _parameter_value(job, "winsorizeImageIntensities", "")
    if winsorize:
        command += ["--winsorize-image-intensities", _bracket_pair(winsorize)]
    if _has_deformable_stage(job):
        command += ["--smooth_warp_sigma", _parameter_value(job, "smoothWarpSigma", "0.25")]
        command += ["--smooth_grad_sigma", _parameter_value(job, "smoothGradSigma", "0.5")]
    if _parameter_value(job, "verbose", "true").lower() in {"true", "1", "yes", "on"}:
        command.append("--verbose")

    for index, stage in enumerate(stages):
        command += ["--transform", _stage_transform_arg(job, index, stage)]
        command += ["--metric", _stage_metric(job, index, metric_fixed, metric_moving, masked_metric)]
        command += ["--convergence", _stage_convergence_arg(job, index)]
        command += ["--shrink-factors", _stage_shrink_factors(job, index)]
    extra_args = _parameter_value(job, "extraArgs", "")
    if extra_args:
        command += shlex.split(extra_args)
    command += list(job.get("extraArguments") or [])
    return command, warped, _fireants_transform_path(job, stages[-1]), stages


def _expected_manifest(job: dict[str, Any], success: bool, error: str = "") -> dict[str, Any]:
    """Create the baseline result manifest shape used for success and failure cases."""
    outputs = job.get("outputs") or {}
    has_deformable_stage = _has_deformable_stage(job)
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
    if has_deformable_stage and (outputs.get("loadInverseWarp", True) or outputs.get("applyWarpToMovingImage", True)):
        manifest["inverseWarp"] = _artifact_path(job, "_inverse_warp.nii.gz")
    if has_deformable_stage and (
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
    """Create the final result manifest after inspecting FireANTs outputs and skipped capabilities."""
    manifest = _expected_manifest(job, success=success, error=error)
    outputs = job.get("outputs") or {}
    has_deformable_stage = _has_deformable_stage(job)
    warnings = manifest["warnings"]
    manifest["forwardWarp"] = ""
    manifest["warpedSegmentations"] = []
    manifest["transformedLandmarks"] = []
    manifest["transformedSurfaces"] = []

    if transform_path and transform_path.exists():
        if transform_path.suffix == ".txt":
            manifest["affineTransform"] = str(transform_path)
            manifest["inverseWarp"] = ""
        elif has_deformable_stage:
            expected_inverse = Path(_artifact_path(job, "_inverse_warp.nii.gz"))
            if transform_path != expected_inverse:
                expected_inverse.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(transform_path, expected_inverse)
            manifest["inverseWarp"] = str(expected_inverse)
            manifest["affineTransform"] = ""
        else:
            manifest["affineTransform"] = ""
            manifest["inverseWarp"] = ""
    else:
        manifest["affineTransform"] = ""
        manifest["inverseWarp"] = ""

    if has_deformable_stage and outputs.get("loadAffineTransform", True) and not manifest.get("affineTransform"):
        warnings.append(
            "FireANTs CLI did not write a separate affine transform for this deformable job. "
            "The exported warp is composite, so Entropy imported the warp but left the moving image affine unchanged."
        )
    if has_deformable_stage and outputs.get("loadForwardWarp", True):
        warnings.append("FireANTs CLI does not currently produce a forward warp; this output was skipped.")
    if outputs.get("loadWarpedSegmentation", False):
        warnings.append("FireANTs CLI apply-transform support is not available yet; warped segmentation output was skipped.")
    if outputs.get("transformLandmarksAndAnnotations", True):
        warnings.append("FireANTs CLI landmark/annotation transform output is not available yet; this output was skipped.")
    if outputs.get("transformSurfaces", False):
        warnings.append("FireANTs CLI surface transform output is not available yet; this output was skipped.")
    return manifest


def _result_manifest_path(job_path: Path, job: dict[str, Any]) -> Path:
    """Return the result manifest path for a job file and job payload."""
    output_dir = Path(job.get("outputDirectory") or job_path.parent)
    prefix = job.get("outputPrefix") or "registration"
    return output_dir / f"{prefix}_result.json"


def _write_manifest(path: Path, manifest: dict[str, Any], elapsed_seconds: float) -> None:
    """Write a result manifest JSON file and attach elapsed runtime."""
    path.parent.mkdir(parents=True, exist_ok=True)
    manifest["elapsedSeconds"] = elapsed_seconds
    path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")


def _fireants_failure_message(exit_code: int, output_lines: list[str]) -> str:
    """Return a user-facing failure summary for known FireANTs process failures."""
    output_text = "\n".join(output_lines).lower()
    if (
        "torch.outofmemoryerror" in output_text
        or "cuda out of memory" in output_text
        or "memory allocation failed with oom" in output_text
    ):
        return (
            f"FireANTs ran out of CUDA memory and failed with exit code {exit_code}. "
            "This is a FireANTs GPU memory failure. Free GPU memory, use a smaller image or mask, "
            "reduce MI bins, omit the full-resolution x1 scale, choose a lower-memory metric, or run on CPU."
        )
    return f"fireantsRegistration failed with exit code {exit_code}."


def run(job_path: Path) -> int:
    """Run one Entropy FireANTs registration job from a JSON file."""
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

    if _find_fireants_registration() is None:
        message = (
            "FireANTs is importable, but the fireantsRegistration executable was not found on PATH "
            "or beside this Python executable."
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
    process_output: list[str] = []
    for line in process.stdout:
        text = line.rstrip()
        if text:
            process_output.append(text)
            _emit("Progress", stageName="FireANTs registration", message=text)
    exit_code = process.wait()

    if exit_code != 0:
        message = _fireants_failure_message(exit_code, process_output)
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
    """Check whether FireANTs and fireantsRegistration are available in this Python environment."""
    if importlib.util.find_spec("fireants") is None:
        print("FireANTs is not importable in this Python environment.", file=sys.stderr)
        return 2
    if _find_fireants_registration() is None:
        print(
            "FireANTs is importable, but fireantsRegistration was not found on PATH or beside this Python executable.",
            file=sys.stderr,
        )
        return 2
    try:
        fireants_version = importlib.metadata.version("fireants")
    except importlib.metadata.PackageNotFoundError:
        fireants_version = "unknown"
    print(f"FireANTs is importable. fireants version: {fireants_version}. fireantsRegistration was found.")
    return 0


def main(argv: list[str] | None = None) -> int:
    """Parse bridge CLI arguments and dispatch to the requested subcommand."""
    parser = argparse.ArgumentParser(
        prog="fireants_bridge", description="Entropy FireANTs bridge"
    )
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
