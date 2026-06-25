# Warp Field Generator

`WarpFieldGenerator` creates synthetic ITK vector images for testing deformation-field loading and rendering.
The generated vectors are physical displacement vectors: for registration-style sampling fields, each vector can be
interpreted as the offset from a fixed/reference location to the moving-image location to sample.

The tool is built when `BUILD_TESTING` is enabled.

```sh
WarpFieldGenerator --print-example
WarpFieldGenerator --print-time-example
WarpFieldGenerator --spec warp.json --output warp.nii.gz
```

## CMake Example Generation

To generate all checked-in example JSON specs from CMake:

```sh
cmake --build build-release --target GenerateWarpFieldExamples
```

By default this writes images to `build-release/warp-field-examples`.

To choose a different output directory, set `WarpFieldGenerator_EXAMPLE_OUTPUT_DIR` when configuring the app build:

```sh
cmake --preset app-release \
  -DWarpFieldGenerator_EXAMPLE_OUTPUT_DIR=/tmp/entropy-warp-field-examples

cmake --build build-release --target GenerateWarpFieldExamples
```

Command-line options can override common JSON fields:

- `--output`: output image file.
- `--component-type` or `--component`: `float`, `double`, `int16`, `uint16`, `int32`, or `uint32`.
- `--size` or `--dimensions`: image dimensions.
- `--spacing`: image spacing.
- `--origin`: image origin.
- `--direction`: row-major image direction matrix.

## JSON Fields

```json
{
  "output": "warp.nii.gz",
  "component_type": "float",
  "spatial_dimension": 3,
  "vector_dimension": 3,
  "size": [64, 64, 64],
  "spacing": [1.0, 1.0, 1.0],
  "origin": [-32.0, -32.0, -32.0],
  "direction": [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0],
  "composition_mode": "additive",
  "operations": []
}
```

`spatial_dimension` may be 1, 2, or 3. For time-dependent fields, either include a full 4D `size` or set
`time_points`; the output becomes a 4D vector image with time on the final axis. A spatial direction matrix is
accepted for time-dependent fields and is embedded into the spatial block of a 4D identity direction matrix.

## Composition

Operations can be combined in two ways:

- `additive`: evaluates every operation at the original physical point and sums the displacements.
- `composed`: applies operations in order, sampling each later operation at the point produced by the earlier operations.

In both modes the generated voxel stores the net displacement from the original physical point. `additive` is the
default.

```json
{
  "composition_mode": "composed",
  "operations": [
    {"type": "affine", "translation": [2.0, 0.0, 0.0]},
    {"type": "twist", "center": [0.0, 0.0, 0.0], "axis": [0.0, 0.0, 1.0], "amplitude": 0.4}
  ]
}
```

## Operations

The final field is created from all operations using the selected composition mode.

- `affine`: linear transform relative to `center`, plus `translation`.
- `inverse_affine`: inverse displacement for the same affine matrix and translation.
- `expansion`: localized spherical or elliptical radial expansion.
- `contraction`: localized spherical or elliptical radial contraction.
- `rotation`: pure rotation around `axis`.
- `twist`: localized rotation around `axis`.
- `swirl`: localized tangential displacement around `axis`.
- `vortex_pair`: two opposing swirls.
- `source_sink_pair`: paired expansion and contraction centers.
- `shear`: displacement along `direction` proportional to position along `normal`.
- `wave`: sinusoidal displacement along `direction`, varying along `normal`.
- `stretch`: component-wise scaling relative to `center`.
- `piecewise_affine`: different affine transforms on either side of a plane.
- `fold`: smooth signed displacement across a plane.
- `tear`: smooth one-sided displacement across a plane.
- `sliding_interface`: tangential motion across a plane with a smooth transition.
- `control_point`: B-spline-like smooth interpolation from sparse control point displacements.
- `landmark`: landmark-driven field using fixed/moving landmark pairs.
- `smooth_random`: deterministic smooth procedural noise.
- `white_noise`: deterministic cell-wise procedural noise.
- `divergence_free`: incompressible 2D flow pattern in the first two axes.
- `curl_free`: gradient field from a sinusoidal scalar potential.
- `jacobian_expansion`: isotropic expansion with the requested approximate Jacobian determinant.
- `boundary_constrained`: displacement that goes to zero at an ellipsoidal boundary.
- `jump`: discontinuous one-sided step displacement across a plane.

Most operations accept `amplitude`, `center`, `direction`, `radii`, `normal`, and `width` as applicable.
Paired operations use `secondary_center`; piecewise affine fields use `secondary_matrix` and
`secondary_translation`. Procedural noise fields use `seed` and `cell_size`.
Control point fields use:

```json
{
  "type": "control_point",
  "width": 20.0,
  "control_points": [
    {"point": [0.0, 0.0, 0.0], "displacement": [3.0, 0.0, 0.0], "radius": 15.0}
  ]
}
```

Landmark fields use:

```json
{
  "type": "landmark",
  "width": 20.0,
  "landmarks": [
    {"fixed": [0.0, 0.0, 0.0], "moving": [3.0, 0.0, 0.0], "radius": 15.0}
  ]
}
```

Time-dependent operations can include:

```json
{
  "time": {
    "mode": "sine",
    "amplitude": 1.0,
    "frequency": 1.0,
    "phase": 0.0,
    "offset": 0.0
  }
}
```

Supported time modes are `constant`, `sine`, `cosine`, and `ramp`.
