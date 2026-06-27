# Deformation Warp Examples

These specs generate paired reference images, moving images, and deformation fields for testing
Entropy's render-time image deformation path.

Generate them with:

```sh
cmake --build build-release --target GenerateDeformationWarpExamples
```

By default the files are written to `build-release/deformation-warp-examples`.

The translation examples are exact sampling-field tests. The moving image has the same voxel data
as the reference image, but its physical origin is shifted. The deformation field stores that same
physical shift, so rendering the moving image with deformation strength `1.0x` should align it with
the reference image.

The nonlinear example is a visual stress test rather than an exact image-pair ground truth. It is
intended for checking deformation strength, warped grids, vector arrows, and segmentation-overlay
sampling behavior. Because it combines expansion, twist, and wave terms additively, it can fold or
tear when the render-time deformation strength is too high.

The sphere-expansion example is a smooth ground-truth sampling-field test. The reference image is a
radius-10 sphere and the moving image is a radius-5 sphere, both centered in the image. Applying
`05_3d_sphere_expansion_field.nrrd` to the moving image at deformation strength `1.0x` should make
the moving sphere match the reference sphere.
