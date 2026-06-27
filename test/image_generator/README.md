# Image Generator

`ImageGenerator` creates synthetic ITK images for testing Entropy image loading and rendering. It supports scalar,
multi-component vector images, and true complex-valued images across 1D, 2D, 3D, and 4D image sizes. Four-dimensional
images are intended for time-series tests, where the final axis is the time axis.

The tool is built when `BUILD_TESTING` is enabled.

```sh
ImageGenerator --print-example
ImageGenerator --spec image.json --output synthetic.nrrd
```

## CMake Example Generation

To generate all checked-in example JSON specs from CMake:

```sh
cmake --build build-release --target GenerateImageExamples
```

By default this writes images to `build-release/image-generator-examples`.

To choose a different output directory, set `ImageGenerator_EXAMPLE_OUTPUT_DIR` when configuring the app build:

```sh
cmake --preset app-release \
  -DImageGenerator_EXAMPLE_OUTPUT_DIR=/tmp/entropy-image-examples

cmake --build build-release --target GenerateImageExamples
```

## JSON Fields

```json
{
  "output": "vector_3comp_3d.nrrd",
  "pixel_kind": "vector",
  "component_type": "float",
  "components": 3,
  "size": [32, 32, 16, 5],
  "spacing": [1.0, 1.0, 2.0, 1.0],
  "origin": [-16.0, -16.0, -16.0, 0.0],
  "direction": [1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0],
  "pattern": "moving_gaussian",
  "metadata": {
    "description": "3-component synthetic vector image"
  }
}
```

`pixel_kind` may be `scalar`, `vector`, or `complex`. `size` controls dimensionality: one value creates a 1D image, two
values create a 2D image, three values create a 3D image, and four values create a 4D image. For portable time-series
examples, use a canonical 4D shape with singleton unused spatial axes, such as `[x, 1, 1, t]` for 1D time series,
`[x, y, 1, t]` for 2D time series, and `[x, y, z, t]` for 3D time series. NRRD examples may also use
`entropy_time_axis=last` metadata to mark lower-dimensional `[x, t]` or `[x, y, t]` images as time series. Vector
images use `components` to set the number of components per pixel. Complex images write one
complex value per pixel and should use `float` or `double` component type.

Supported component types are `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`, `float`, and `double`.

Supported patterns are:

- `ramp`: smooth coordinate ramp with per-component offsets.
- `checker`: alternating blocks.
- `gaussian`: centered Gaussian blob.
- `component_ramp`: linear index plus a large per-component offset.
- `complex_phase`: rotating complex phase pattern for complex images.
- `time_ramp`: ramp with a strong contribution from the final/time axis.
- `temporal_sine`: sinusoidal signal over the final/time axis.
- `moving_gaussian`: Gaussian blob that moves over the final/time axis.
- `pulsing_gaussian`: 3D Gaussian blob whose width and intensity vary over time.
- `two_moving_gaussians`: two 3D blobs with opposite signs that move along different paths over time.
- `rotating_wave`: 3D wave pattern that rotates over time.
- `time_varying_warp_field`: 3-component vector field with pulsing expansion, twist, and wave motion over time.
- `sphere`: binary sphere centered in the image; set its physical radius with `sphere_radius`.

Command-line options can override common JSON fields:

- `--output`: output image file.
- `--pixel-kind` or `--pixel-type`: `scalar`, `vector`, or `complex`.
- `--component-type` or `--component`: output component type.
- `--components`: number of vector components.
- `--size` or `--dimensions`: image dimensions.
- `--spacing`: image spacing.
- `--origin`: image origin.
- `--direction`: row-major image direction matrix.
- `--pattern`: generated intensity pattern.
