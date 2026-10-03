# GPU deformation — Stage 2

This GL 3.3 backend lives in `Entropy::Rendering`. `Entropy::Deformation`
remains standalone with only GLM as a public dependency. The double-precision
RK4 oracle is shared by CPU/GPU tests, never linked into the production backend.

Stage 2 implements **native 2D** velocity evaluation, integration, composition,
and sampled diagnostics. Storage and framebuffer layer attachment support 3D;
volumetric numerical passes are Stage 3. A singleton-axis volume is still 3D.

## API and contracts

Use a current GL 3.3 core context with loaded entry points, on one thread.
Destroy resources before releasing that context. Do not run these passes
inside active transform feedback or conditional rendering.

```cpp
#include "deformation/VelocityLattice.h"
#include "rendering/deformation/FieldPassRunner.h"

namespace gpu = rendering::deformation;
constexpr std::size_t workspaceBytes = std::size_t{64} * 1024 * 1024;
gpu::FieldPassRunner passes(workspaceBytes);
gpu::FieldTexture velocity(native2dDomain, workspaceBytes);
passes.velocity(deformation::VelocityLattice(brush, controlSpacingMm), velocity);
auto candidate = passes.exponential(velocity, 8);
gpu::FieldTexture diagnostics(native2dDomain, workspaceBytes);
passes.quality(*candidate.forward, *candidate.inverse, diagnostics);
// Candidate only: Stage 3 adds acceptance checks before publication.
```

- `FieldTexture` stores RGBA32F: XYZ is physical LPS displacement/velocity in
  millimeters, W is exactly 1 for usable samples and 0 otherwise. New storage
  is invalid zero data; `identity()` writes valid zero displacement. Transfers
  are x-fastest and synchronous. Do not resize storage through raw GL access.
- The texture limit is checked before allocation. Runner workspace covers its
  concurrently owned textures: three fields for an exponential, two for
  accumulation, one coefficient texture for velocity evaluation. Caller-owned
  inputs/outputs, driver overhead, and host transfer buffers are excluded.
- Integer indices denote sample centers. Physical geometry remains double on
  the CPU; relative origins are subtracted before float conversion. Oblique,
  reflected, and anisotropic native-2D frames are supported. Non-finite vectors
  or normal motion exceeding 1e-6 mm invalidate a sample.
- Cubic interpolation reconstructs the unweighted affine generator, then
  applies continuous support/protection. Strength is already in the generator
  coefficients. The initial uniform-backed implementation supports 32 protected
  disks; larger lists are rejected, not truncated.
- Field sampling is explicit bilinear interpolation. All positive-weight
  contributors must be valid; zero-weight neighbors do not count. Outside the
  valid-center extent is invalid, never clamped or silently treated as identity.
- Scaling and squaring seeds `+/-v / 2^s` and self-composes `s` times, with
  `s` in [0, 20]. More squarings do not necessarily improve float32/interpolation
  accuracy. Field spacing and integration refinement remain caller choices.
- `compose(outer, inner)` evaluates `outer(inner(x))`. Paired accumulation is
  `{D o F, G o D_inverse}`; the inverse increment comes from negative velocity,
  not a negated finished displacement. Stage 2 requires identical field grids;
  the broader metadata contract still permits different grids for later work.
- Outputs cannot alias any input texture object. Numerical passes restore the
  GL 3.3 raster, framebuffer, program, VAO, texture/sampler, pixel-transfer/PBO,
  clip-distance, and per-draw-buffer blend/write-mask state they affect.
  Private framebuffer attachments are released after each draw.
- Pair-producing operations poll cancellation between passes, returning two
  empty pointers on cancellation. Inputs remain unchanged; partial pairs are
  never published. This is not yet an application transaction/undo system.

`quality()` writes `{det(J), inverse residual mm, inverse residual voxels, valid}`.
It uses the intrinsic physical 2x2 central-difference stencil at one sample
spacing. Missing stencil/round-trip coverage means W=0, including the outermost
row and column. Exchange the maps to check the other round trip. Finite negative
determinants are reported, not hidden.

These diagnostics do **not** verify cells, compute singular values, or establish
diffeomorphism. They cannot satisfy the default `QualityPolicy`. Protected grid
centers remain exactly stationary, but dense interpolation near a core boundary
does not guarantee every off-grid point is stationary. Stage 3 must check those
errors, coverage, and convergence before accepting a candidate.

## Measured accuracy

Linux/GCC Debug, NVIDIA GTX 1080 Ti, GLSL 330 fragment shaders, compared with
converged double-precision endpoints. These are sampled regression measurements,
not global error bounds or medical safety limits. Each refinement evaluates its
own grid, so endpoint comparison points are not identical between resolutions.

For a 6-mm support radius, 1.1-mm coefficient spacing, and eight squarings:

| Motion | Endpoint error, 0.25-mm field | 0.125-mm field | 0.0625-mm field |
| --- | ---: | ---: | ---: |
| Push (1, 0.3) mm | 0.009591 mm | 0.003263 mm | 0.001208 mm |
| Inflate/deflate, exposure +/-0.4 | 0.004931 mm | 0.001333 mm | 0.000620 mm |
| Twirl, exposure 0.8 radians | 0.010137 mm | 0.003398 mm | 0.001401 mm |

Tests gate endpoints at 0.012 mm and sampled forward round trips at 0.015 mm,
requiring fine endpoint error below 60% of coarse error. An oblique anisotropic
affine test checks both round-trip directions and physical Jacobians.

A 0.7-mm protection transition produced 0.022621-mm endpoint error on a 0.25-mm
field. Refining to 0.0625 mm reduced that to 0.006683 mm (test limit 0.01 mm).
At fixed 0.125-mm spacing, a 1-radian twirl's error fell from 0.320018 to 0.021356
to 0.003980 mm with 0, 4, and 8 squarings. Field and integration resolution need
separate refinement tests; neither can substitute for the other.

## Build and test

The executable builds with rendering/deformation tests. CTest registration is
opt-in so CPU-only CI workers do not need a display. An explicitly requested
GPU run fails if GLFW/GL support is missing; it never skips.

```sh
cmake -S . -B build-debug -DEntropy_SUPERBUILD=OFF \
  -DEntropy_ENABLE_DEFORMATION_GPU_TESTS=ON
cmake --build build-debug --target TestDeformationGpu TestDeformation --parallel 4
ctest --test-dir build-debug -L '^deformation(-gpu)?$' --output-on-failure
# Direct invocation also works without CTest registration:
build-debug/bin/TestDeformationGpu --reporter compact
```

Headless graphics workers need a working GL 3.3 display, such as Xvfb/Mesa,
and explicit registration. Ordinary CPU-only CI does not exercise this suite.
The Linux results do not imply macOS or Windows runtime validation.
