# Entropy Registration Backend Integration Plan

## Summary

Add registration as an external-job workflow with backend-specific, reactive UI. Entropy will expose Greedy, ANTs, and FireANTs capabilities without overwhelming users by using progressive disclosure:

- **Simple mode**: presets and safe defaults.
- **Advanced mode**: all commonly useful backend parameters.
- **Expert mode**: full backend command preview, editable advanced arguments, and detailed logs.

Backends:

- **Greedy** first: CPU CLI, masks, labels, segmentations, surfaces, inverse warps.
- **ANTs** second: mature CLI, masks, labels, landmarks/point tools, broad transform models.
- **FireANTs** third: CUDA/Python, masked losses, keypoint evaluation, GPU registration through a bridge.

The integration boundary is process + disk + JSON/JSONL. Entropy writes a job specification, launches the backend, tracks structured progress, imports output artifacts, and records the imported result in the project.

## UI Model

Add:

- `Image -> Registration -> Register active image to reference...`
- `Image -> Registration -> Registration jobs...`
- `Application Settings -> Registration`

The setup dialog opens with the current reference image as the fixed image and the active image as the moving image. The default preset is `Affine + Deformable`.

Use a dockable ImGui window titled `Register Image`. Keep the top summary always visible:

- Fixed image
- Moving image
- Backend
- Preset
- Current validation status

Use large collapsible headers:

- Backend
- Images
- Transform model
- Metric
- Masks
- Segmentations and labels
- Landmarks
- Surfaces
- Outputs
- Advanced
- Command preview

Default-open headers: Backend, Images, Transform model, Metric, Outputs.

Every parameter gets a concise label, tooltip, sensible default, appropriate widget, and backend support/validation state. Unsupported options are hidden by default. If an option becomes unsupported after the backend changes, show an inline warning and disable it.

## Backend Options

### Greedy

Expose:

- Transform models: Rigid, Affine, Deformable, Affine + Deformable, stationary velocity deformable.
- Metrics: SSD, MI, NMI, NCC, WNCC.
- Controls: iteration schedule, NCC/WNCC radius, step size, smoothing sigma and units, fixed/moving masks, thread count, output inverse warp, saved warp precision, single precision, verbosity.
- Outputs: warped moving image, warped segmentation, transformed surfaces, inverse/sampling warp, forward warp.

Default label interpolation for warped segmentations: Greedy `LABEL 0.2vox`.

### ANTs

Expose:

- Stages: Translation, Rigid, Similarity, Affine, SyN, BSpline displacement field, Gaussian displacement field, time-varying velocity field where supported.
- Per-stage controls: transform parameters, metric, iterations, shrink factors, smoothing sigmas, convergence threshold/window, sampling strategy/percentage.
- Inputs: masks, multivariate image metric pairs, segmentation/label metric pairs, landmark/point-set inputs where supported.
- Outputs: warped image, inverse-warped fixed image, forward warp, inverse warp, affine/composite transform, transformed landmarks, warped segmentations.

### FireANTs

Expose through a Python bridge:

- Registration classes: Rigid, Affine, Greedy deformable, SyN where available, chained affine + deformable.
- Losses: CC, MI, MSE, fused CC/MI, masked CC, masked MSE.
- Controls: device, scales, iterations, optimizer, learning rate, CC kernel size, MI settings, smooth warp sigma, smooth gradient sigma, deformation type, progress callback frequency, save ANTs-compatible transforms.
- Inputs: fixed/moving masks as appended mask channels, keypoints/landmarks for transform evaluation, segmentations as auxiliary outputs.

## Progress UI

Add a dockable `Registration Jobs` window. Each job row shows backend, fixed image, moving image, preset, status, progress bar, current stage, level, iteration, latest metric/loss, elapsed time, warning/error badge, and buttons for Cancel, Show log, Import outputs, and Reveal files.

Statuses:

- Queued
- Preparing inputs
- Running
- Writing outputs
- Importing outputs
- Completed
- Cancelled
- Failed

Also show a compact non-modal lower-left popup while jobs run, matching the existing loading-status popup. It shows backend, fixed/moving names, current stage, progress, and latest loss. It never blocks rendering.

Normalize backend progress into JSONL events:

```json
{"event":"started"}
{"event":"stage_started","stage":0,"name":"Affine","totalIterations":160}
{"event":"progress","stage":0,"level":1,"iteration":42,"iterations":100,"loss":0.1234}
{"event":"artifact","kind":"warped_image","path":"..."}
{"event":"completed","manifest":"..."}
{"event":"failed","message":"...","details":"..."}
```

Greedy and ANTs may initially use parsed stdout/stderr. FireANTs should emit JSONL directly from the Python bridge.

## Error Handling

Before launch, show inline blocking errors for missing executables, invalid image selection, unsupported dimensions, mask geometry mismatch, no matched landmarks, or unsupported backend features. Blocking errors disable `Start`.

Runtime warnings are non-fatal and appear in the job row, job detail panel, app log, and result manifest. Examples include poor convergence, repeated metric worsening, missing expected artifacts, suspicious transform geometry, CUDA unavailable, and image/mask space mismatch within tolerance.

Runtime failures capture command line, working directory, environment summary, stdout, stderr, exit code, elapsed time, partial artifacts, and parsed backend error text. Launch failures or immediate fatal failures use native OS dialogs. Detailed diagnostics live in the Registration Jobs window with copy-to-clipboard and reveal-folder controls.

## Output Import And Image Updates

On success, import artifacts on the main thread:

1. Load inverse/sampling warp.
2. Load forward warp if produced.
3. Assign inverse/sampling warp to the moving image.
4. Assign forward warp to the moving image if available.
5. Load warped moving image if requested.
6. Load warped segmentation if requested.
7. Transform landmarks/annotations if a forward warp exists.
8. Load transformed surfaces if produced.
9. Create/update render textures.
10. Reconcile layouts.
11. Mark project dirty.
12. Save result metadata into project state.

Default names:

- Warped image: `<moving> registered to <fixed>`
- Inverse/sampling warp: `<moving> inverse warp to <fixed>`
- Forward warp: `<moving> forward warp from <fixed>`
- Warped segmentation: `<segmentation> registered to <fixed>`

After import, keep the current layout, make the warped image visible in current views, do not hide the original moving image automatically, and make the warped image active only if the user selected that option.

## Application Settings

Add `Application Settings -> Registration` with:

- default backend
- Greedy executable path
- ANTs executable directory
- FireANTs Python executable
- default output directory
- keep temporary files
- max concurrent jobs
- default CPU thread count
- default FireANTs device
- show expert options by default

Buttons:

- Check backends
- Open output folder
- Reset registration settings

Backend checks run asynchronously and report found/not found, version, detected features, and CUDA availability for FireANTs.

## Data Model

Add backend-neutral types:

- `RegistrationBackend`
- `RegistrationBackendCapabilities`
- `RegistrationParameterSchema`
- `RegistrationJobSpec`
- `RegistrationInputArtifacts`
- `RegistrationOutputRequests`
- `RegistrationProgressEvent`
- `RegistrationResultManifest`
- `RegistrationJobState`

Use parameter schemas to drive backend-reactive UI. Each parameter records label, type, default value, min/max where relevant, tooltip, visibility condition, validation rule, and backend support.

Project JSON stores completed imported results: backend, fixed image UID, moving image UID, selected parameters, generated artifacts, assigned warp UIDs, warped image UID, transformed segmentation/landmark/surface outputs, result manifest path, and warnings.

## Implementation Steps

1. Add backend capability and parameter-schema models.
2. Add tests for schema-driven visibility/validation.
3. Add job/result/progress JSON models and tests.
4. Add process runner abstraction and mocked tests.
5. Add Registration setup dialog using backend schemas.
6. Add Registration jobs window and compact progress popup.
7. Add Application Settings registration section.
8. Add export helpers for masks, segmentations, landmarks, annotations, and surfaces.
9. Implement Greedy backend.
10. Implement ANTs backend.
11. Implement FireANTs bridge and backend adapter.
12. Add project persistence for registration results.
13. Add integration tests gated by backend executable environment variables.
14. Add user documentation for backends, parameters, errors, and warp conventions.

## Tests

Unit tests:

- backend capability detection
- parameter visibility
- parameter validation
- default parameter generation
- command generation
- JSON job serialization
- progress parsing
- error classification
- manifest import
- artifact naming
- project persistence

Integration tests:

- Greedy intensity registration
- Greedy masked registration
- Greedy segmentation output
- Greedy surface output
- ANTs staged registration
- ANTs point transform
- FireANTs bridge check
- FireANTs CUDA registration when available

Manual acceptance tests:

- switch backends and verify UI updates correctly
- inspect every backend option and tooltip
- launch simple preset registration
- launch advanced registration
- cancel a job
- inspect backend errors
- import outputs
- compare original, warped, and reference images
- save/reload project and verify registration artifacts persist

## Assumptions And Defaults

- Advanced options are available but hidden behind headers by default.
- Expert users can inspect generated commands and logs.
- Raw backend argument pass-through is allowed only in Expert mode.
- Greedy is implemented first.
- ANTs is implemented second.
- FireANTs is implemented third.
- Real-time renderer updates during every iteration are deferred.
- Per-scale snapshots may be added after core workflow is stable.
