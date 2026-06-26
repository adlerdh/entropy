# Image Generator

`ImageGenerator` creates synthetic ITK images for testing Entropy image loading and rendering. It supports scalar,
multi-component vector images, and true complex-valued images across 1D, 2D, and 3D image sizes.

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
  "size": [32, 32, 16],
  "spacing": [1.0, 1.0, 2.0],
  "origin": [-16.0, -16.0, -16.0],
  "direction": [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0],
  "pattern": "component_ramp",
  "metadata": {
    "description": "3-component synthetic vector image"
  }
}
```

`pixel_kind` may be `scalar`, `vector`, or `complex`. `size` controls dimensionality: one value creates a 1D image, two
values create a 2D image, and three values create a 3D image. Vector images use `components` to set the number of
components per pixel. Complex images write one complex value per pixel and should use `float` or `double` component
type.

Supported component types are `uint8`, `int16`, `uint16`, `int32`, `uint32`, `float`, and `double`.

Supported patterns are:

- `ramp`: smooth coordinate ramp with per-component offsets.
- `checker`: alternating blocks.
- `gaussian`: centered Gaussian blob.
- `component_ramp`: linear index plus a large per-component offset.
- `complex_phase`: rotating complex phase pattern for complex images.

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
