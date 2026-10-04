# GPU deformation — Stage 3 numerical backend

This GL 3.3 backend lives in `Entropy::Rendering`. `Entropy::Deformation`
remains standalone with only GLM as a public dependency. The double-precision
RK4 oracle is shared by CPU/GPU tests, never linked into the production backend.

Native 2D and 3D velocity evaluation, integration, composition, sampled
diagnostics, and compact GPU quality reductions are implemented. Conservative
algebraic stretch bounds feed sampled per-direction evidence. Physical
refinement comparison, conservative protected-core bounds, interior-cell
verification, and bounded candidate acceptance are implemented. Same-grid
refinement and exact-zero protected-core checks use compact GPU reductions;
ambiguous protected cores and all final cell checks use bounded CPU readback.
A singleton-axis volume is still 3D.

Stage 5 adds `composeTile()` for exact local output updates. It writes only the
requested x/y rectangle and z layer range; input textures still contain the
complete field. `deformation::compositionDependencies()` computes a conservative
outer-input box from the inner displacement, including interpolation neighbors
and a one-sample rounding margin. Its caller-supplied sample cap rejects an
oversized dependency. `FieldReadback::checkpoint()` transfers one layer at a
time into a bounded full-resolution host pair. These APIs do not make the
numerical pipeline out of core.

The tested edit envelope is 17³ samples per direction: one RGBA32F field is
78,608 bytes, and the shared workspace reserves 64 MiB for numerical scratch
plus five field equivalents (393,040 bytes) for accepted maps, candidate,
and checkpoint transfer. The 3D edit and budget-exhaustion tests pass without
resolution reduction. GL driver overhead and caller-owned image or metric
reservations are outside this measurement. Larger domains require their own
measured budget and remain unsupported by this stage's gate.

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
gpu::FieldTexture velocity(domain, workspaceBytes);
passes.velocity(deformation::VelocityLattice(brush, controlSpacingMm), velocity);
auto result = passes.acceptVelocity(velocity, nullptr, brush.definition().protection);
if (result.pair.forward) {
  // Publish the pair as one accepted unit.
}
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
  reflected, and anisotropic native-2D and 3D frames are supported. Non-finite
  vectors invalidate a sample; native-2D normal motion exceeding 1e-6 mm also
  invalidates it.
- Cubic interpolation reconstructs the unweighted affine generator, then
  applies continuous support/protection. Strength is already in the generator
  coefficients. The initial uniform-backed implementation supports 32 protected
  regions; larger lists are rejected, not truncated.
- Field sampling is explicit bilinear or trilinear interpolation. All positive-weight
  contributors must be valid; zero-weight neighbors do not count. Outside the
  valid-center extent is invalid, never clamped or silently treated as identity.
- Every volume pass attaches and writes one destination z layer at a time. A
  complete sweep finishes before a result/scratch swap. Input and output texture
  objects must be disjoint even when different layers would be accessed.
- Scaling and squaring seeds `+/-v / 2^s` and self-composes `s` times, with
  `s` in [0, 20]. More squarings do not necessarily improve float32/interpolation
  accuracy. Field spacing and integration refinement remain caller choices.
- `compose(outer, inner)` evaluates `outer(inner(x))`. Paired accumulation is
  `{D o F, G o D_inverse}`; the inverse increment comes from negative velocity,
  not a negated finished displacement. Current passes require identical field grids;
  the broader metadata contract still permits different grids for later work.
- Outputs cannot alias any input texture object. Numerical passes restore the
  GL 3.3 raster, framebuffer, program, VAO, texture/sampler, pixel-transfer/PBO,
  clip-distance, and per-draw-buffer blend/write-mask state they affect.
  Private framebuffer attachments are released after each draw.
- Pair-producing operations poll cancellation between passes, returning two
  empty pointers on cancellation. Inputs remain unchanged; partial pairs are
  never published. This is not yet an application transaction/undo system.

`quality()` writes `{det(J), inverse residual mm, inverse residual voxels, valid}`.
It uses the intrinsic physical 2x2 or 3x3 central-difference stencil at one
sample spacing. Missing stencil/round-trip coverage means W=0, including the
outermost border. A singleton-axis 3D volume has no complete 3D stencil and
therefore produces invalid quality samples. Exchange the maps to check the
other round trip. Finite negative determinants are reported, not hidden.

`reduceQuality()` uses 4x4 or 4x4x4 gather passes to keep final readback compact.
Each readback tile represents at most 2^20 requested samples; the CPU sums
exact tile counts in `size_t`. It reports requested, evaluated, unavailable,
and non-finite diagnostic-texel counts plus determinant and residual extrema.
Invalid source vectors currently appear as unavailable coverage. The
default requested region is the valid extent inset by one sample on each active
axis, where a central-difference stencil can exist. Refinement and protection
passes explicitly request the complete valid extent.

`analyzeDirection()` combines this reduction with a second pass that bounds
physical directional stretch using determinant and Frobenius norm. The reported
minimum is a conservative lower bound and maximum a conservative upper bound,
not exact singular values. These float32 sampled diagnostics do **not** verify
cells or establish diffeomorphism. They cannot satisfy the default
`QualityPolicy` on their own. `sampledReport()` evaluates both directions and
leaves protection, convergence, and cell verification explicitly unchecked.
Protected grid centers remain exactly stationary, but dense interpolation near
a core boundary does not guarantee every off-grid point is stationary.

`acceptVelocity()` starts each attempt from the same velocity, checks both map
directions, bounds represented protected cores throughout intersecting cells,
verifies every represented cell, and compares with a separately integrated
result at one additional squaring. It retries only inverse or convergence
errors, up to the caller's attempt limit. Hard failures and missing evidence
return an empty pair. The previous pair is immutable. On cancellation or an
exception, no new pair is published. The caller must publish only a nonempty
accepted pair.

Final cell verification reads both fields to the CPU. The compact protected
pass can prove that all corners of every intersecting cell are exactly zero;
other cases use the adaptive CPU bound. Same-grid refinement uses compact
reductions over grid and cell centers. Since the difference of two multilinear
fields on the same grid is multilinear, its off-grid norm is bounded by the
largest corner difference. The reduction includes a float32 arithmetic
allowance. Cross-grid refinement uses the portable CPU comparison. A protected
core must fit inside the valid sample-center extent. The cell verifier proves
its stated interval bounds for
the represented bilinear/trilinear map; it is not a proof of global
injectivity. Singleton active axes have no verifiable cells and cannot pass
the default acceptance policy. Cross-grid composition remains unsupported;
portable cross-grid refinement comparison is available in `FieldEvidence`.

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

The acceptance regression covers identity on 9^2 and 9^3 oblique/reflected,
anisotropic grids with a million-millimeter physical origin, plus twenty
overlapping push attempts on 17^2 and 17^3 grids at
0.5-mm spacing. The accepted push per increment is (0.02, 0.008, 0) mm in 2D
and (0.02, 0.008, 0.004) mm in 3D, with 2.5-mm support and default quality
limits. The first five attempts must pass; later attempts may stop at the
inverse/interpolation error floor, leaving the accepted pair unchanged. This
is a tested fixture, not a general maximum. A 3x larger 2D push
failed inverse consistency after three overlaps at the same spacing; increasing
the squaring count did not remove that error. Coarser fields and narrow
protection transitions may require a finer grid. A request that does not pass
must stay unpublished.

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

Headless graphics workers need a working GL 3.3 display, such as Xvfb/Mesa.
Ubuntu CI invokes the executable under Xvfb; the manual desktop validation
workflow registers and runs it on macOS, Windows, and Linux. Those platform
runtime runs have not yet been observed for this implementation.
