# Deformation library — Stages 0 and 1

`Entropy::Deformation` is a standalone C++23 library. Its only public dependency
is GLM. It does not link Entropy's application, image IO, ITK, OpenGL, or a window
system. Catch2 3 is needed only when building tests.

Headers and implementations live together. Include public headers as
`deformation/FieldDomain.h`, not through an extra nested directory.

## Build on its own

Use installed GLM and Catch2 packages, or point CMake at the packages already
provided by Entropy's dependency build:

```sh
cmake -S lib/deformation -B build-deformation \
  -DCMAKE_BUILD_TYPE=Debug \
  -Dglm_DIR=/path/to/glm/package \
  -DCatch2_DIR=/path/to/Catch2/package
cmake --build build-deformation
ctest --test-dir build-deformation --output-on-failure
```

For a library-only build, set `-DBUILD_TESTING=OFF` on first configuration, or
`-DEntropyDeformation_BUILD_TESTING=OFF` explicitly. Another CMake project can
also use `add_subdirectory` and link `Entropy::Deformation`; no installation
step is required. The normal Entropy build includes this target and registers
`TestDeformation` with its test and coverage infrastructure.

## Geometry contract

```cpp
#include "deformation/FieldDomain.h"

deformation::DomainGeometry geometry;
geometry.size = {128, 128, 64};
geometry.spacing = {0.5, 0.5, 2.0};
deformation::FieldDomain domain{geometry};
const auto positionMm = domain.indexToPhysical({10.0, 20.0, 3.0});
```

Positions and physical vectors use LPS millimeters. Field values must not be
confused with voxel offsets: `indexVectorToPhysical` converts the latter.
Points include the origin; vectors do not. The origin is the center of sample
zero, not its outer corner. Directions are matrix columns and may be reflected.

Native 2D is explicit: set dimension to `SpatialDimension::Plane`, size z to
one, and inactive spacing z to one. The first two direction columns define
the plane. Normal motion is rejected, apart from a documented small numerical
residue when converting physical coordinates. A one-slice `Volume` still has
three spatial degrees of freedom.

The valid extent describes a closed interval between sample centers, specified
by half-open integer sample bounds. It does not assert interpolation support,
per-sample validity, or coverage by another map. Geometry conversions allow
extrapolation; `containsIndex` checks extent membership separately.

Construction validates geometry and rejects invalid input with `ContractError`,
whose `reason()` is machine-readable. Conversion rejects non-finite results.
There is no silent clamping, orthogonalization, unit guessing, or resampling.
Byte-allocation overflow must still be checked by a future buffer owner;
`sampleCount()` checks only the product of sample dimensions.

## Paired-map contract

`MapPairDescriptor` owns two domain values and a shared history-scoped revision
and numerical-policy version. The forward domain samples `F(x) = x + u(x)`;
the inverse domain samples `G(y) = y + v(y)`. They may use different grids.
Neither direction can be revised independently through the descriptor.

This is metadata, not a sampled field or a claim of inverse consistency. Dense
field views, arbitrary image masks, displacement interpolation, and the GPU
integration backend are later stages. Policy versions are identities, not acceptance certificates.
Revision tokens are scoped to an owning history, not globally unique IDs.

Tests provide small scalar and analytic field fixtures without an image-library
dependency. They cover geometry, native 2D, paired metadata, invalid inputs,
composition order, and the failure of independently scaled inverse maps. Each
public header is also compiled in isolation.

## Stage 1: canonical brush motion

`BrushStep` owns a validated recipe for a stationary velocity integrated over
normalized time [0, 1]. `PushMotion` contains a physical drag displacement;
`RadialMotion` contains signed expansion exposure (rate times active seconds);
`TwirlMotion` contains a unit physical axis and signed angular exposure in radians.
Common strength multiplies the velocity, never a previously integrated field.

```cpp
#include "deformation/BrushStep.h"
#include "deformation/VelocityLattice.h"

deformation::BrushDefinition definition;
definition.centerMm = {20.0, 30.0, 40.0};
definition.radiusMm = 8.0;
definition.strength = 0.5;
definition.motion = deformation::PushMotion{{1.0, 0.0, 0.0}};
const deformation::BrushStep brush{definition};
const deformation::VelocityLattice lattice{brush, 2.0};
const auto velocity = lattice.velocity({21.0, 30.0, 40.0});
```

The support is a physical disk or sphere with a compact C2 radial envelope.
Protection currently uses analytic disks/spheres with an exactly stationary
core and a quintic transition. Protection remains fixed in reference space
for the increment. Image-mask protection and ellipsoidal support are not
implemented yet.

The lattice stores cubic cardinal B-spline coefficients of the **affine
generator**, not samples of the weighted velocity. This construction reproduces
the initial affine generators with a complete halo, then applies the continuous
support/protection envelope. It is not a general spline fit or a set of freely
editable handles. Coefficients are immutable, x-fastest physical vectors, ready
for a later texture upload. Allocation is explicitly budgeted.

`resampleStroke` turns captured physical positions/timestamps into canonical
segments, carrying unused distance/time across input events. Push uses each
segment's displacement; radial and angular tools multiply their rates by its
active duration. Place the brush at the segment midpoint. Stationary holds
therefore affect rate-based tools but do not keep pushing. Pauses and event gaps
break the path; cancellation emits nothing. Reversals retain their turning point.
This is a batch recipe builder, not a window-system event handler.

## Numerical reference and acceptance

`test/reference/ReferenceFlow` is deliberately linked only into the tests. It
provides double-precision RK4, bounded endpoint-refinement experiments,
chronological flow composition, reverse-order negative-velocity inversion,
and physical finite-difference Jacobians. Convergence means agreement between
successive refinements, not a rigorous mathematical error bound.

The public `QualityPolicy` API computes determinant/singular-value diagnostics,
physical and voxel-normalized inverse residuals, and reduces observed samples
into a report. `assessCandidate` checks both directions, coverage, protected
motion, and convergence. Hard failures take precedence over requests for more
numerical evidence. A fold cannot be enabled by relaxing a tolerance.

Default policy requires cell verification. Stage 1 does **not** implement that
verification: a report from the sampled CPU reference cannot pass this default.
Tests may explicitly request a sampled-only profile, whose acceptance means
only that those sampled checks passed. Thresholds are provisional engineering
values, not medical safety limits. A numerical backend must supply genuine
evidence; setting report flags does not perform the corresponding checks.

Stage 1 tests compare analytic and lattice motion, exact protected cores,
affine spline reproduction, event-rate consistency, stroke-refinement behavior,
analytic flow convergence, paired-map residuals, intrinsic 2D area ratios, 3D
volume ratios, directional distortion, and rejection/retry policy. The next
stage implements the GL 3.3 numerical backend; no UI or image resampling is
provided by this library yet.
