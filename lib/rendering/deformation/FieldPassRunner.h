#pragma once

#include "deformation/Types.h"
#include "deformation/CellVerification.h"
#include "deformation/FieldEvidence.h"
#include "deformation/QualityPolicy.h"
#include "deformation/VelocityLattice.h"
#include "rendering/deformation/FieldTextures.h"

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <span>

namespace rendering::deformation
{
/** @brief A candidate forward/inverse pair; neither member is a quality or acceptance certificate. */
struct FieldPair
{
  std::unique_ptr<FieldTexture> forward; //!< F(x) = x + u(x).
  std::unique_ptr<FieldTexture> inverse; //!< G(y) = y + v(y), integrated from negative velocity.
};

/** @brief Compact sampled diagnostics from one map direction, before distortion and cell checks. */
struct ReducedDirectionQuality
{
  std::size_t requested = 0;
  std::size_t evaluated = 0;
  std::size_t outside = 0;
  std::size_t nonFinite = 0; //!< Non-finite diagnostic texels; invalid source vectors appear as outside coverage.
  double minDeterminant = std::numeric_limits<double>::infinity();
  double maxDeterminant = -std::numeric_limits<double>::infinity();
  double maxResidualMm = 0.0;
  double maxResidualVoxels = 0.0;
};

/** @brief CPU checks of both represented displacement fields; not a global injectivity proof. */
struct PairCellVerification
{
  ::deformation::CellVerification forward;
  ::deformation::CellVerification inverse;
  [[nodiscard]] bool complete() const noexcept
  {
    return forward.complete() && inverse.complete();
  }
};

/** @brief Result of a bounded candidate attempt; pair is populated only after acceptance. */
struct CandidateResult
{
  FieldPair pair;
  ::deformation::QualityReport report;
  PairCellVerification cells;
  ::deformation::RefinementEvidence refinement;
  ::deformation::ProtectionEvidence protection;
  ::deformation::CandidateAssessment assessment{
    ::deformation::CandidateDecision::Refine,
    ::deformation::QualityReason::MissingEvidence};
  unsigned attempts = 0;
  bool canceled = false;
};

/** @brief GPU proof that protected-cell corner vectors are exactly zero, or a request for CPU bounds. */
struct ProtectedCorePrecheck
{
  bool checked = false;
  bool exactlyZero = false;
};

/**
 * @brief GL 3.3 fragment-pass backend for native-2D and 3D stationary velocity flows.
 * @details All vector values are LPS mm, not voxel offsets. Operations require
 * identical field grids; cross-grid composition is not implemented. Coordinates
 * are recentered in double precision before float32 shader arithmetic. Field
 * sampling uses bilinear or trilinear interpolation with valid, finite
 * contributors; native-2D vectors must also be tangent to the plane. Outside
 * coverage is invalid, never clamped or replaced by identity. An output may
 * not alias any input texture object. Volume passes finish every destination
 * layer before high-level operations swap ping-pong textures.
 *
 * Use on one thread with a current owning GL context and loaded entry points,
 * including destruction. GL state is restored on return and on exceptions.
 * Caller-owned low-level outputs may be partially written if a GL failure occurs;
 * pair-producing operations publish only complete pairs and never mutate inputs.
 */
class FieldPassRunner final
{
public:
  /** @brief Poll between passes; true cancels unpublished pair work. Must not modify GL state or input resources. */
  using Cancel = std::function<bool()>;

  /**
   * @brief Compile embedded shaders and allocate a framebuffer/VAO.
   * @param workspaceBytes Upper bound on concurrently owned texture bytes in each
   * high-level operation, including result textures and uploaded coefficients,
   * but excluding caller-owned inputs, driver overhead, and host transfer buffers.
   */
  explicit FieldPassRunner(std::size_t workspaceBytes);
  /** @brief Release GL resources in the current context. */
  ~FieldPassRunner();
  FieldPassRunner(const FieldPassRunner&) = delete;
  FieldPassRunner& operator=(const FieldPassRunner&) = delete;

  /** @brief Write zero displacement, valid only inside the domain's valid extent. */
  void identity(FieldTexture& output);
  /** @brief Copy finite tangent vectors and validity; inputs and output must be disjoint. */
  void copy(const FieldTexture& input, FieldTexture& output);
  /** @brief Evaluate cubic generator coefficients, then continuous support/protection; at most 32 protected regions. */
  void velocity(const ::deformation::VelocityLattice& lattice, FieldTexture& output);
  /** @brief Write a signed, scaled velocity as the initial Euler displacement; finite scale required. */
  void seed(const FieldTexture& velocity, float scale, FieldTexture& output);
  /** @brief Write outer(inner(x)) - x, with validity propagated through both samples. */
  void compose(const FieldTexture& outer, const FieldTexture& inner, FieldTexture& output);
  /** @brief Compose only the requested stored sample box; samples outside it keep their previous values.
   * @details Inputs remain complete resident textures. The caller must initialize output outside
   * the tile and include any displaced input dependencies in those textures.
   */
  void composeTile(
    const FieldTexture& outer,
    const FieldTexture& inner,
    FieldTexture& output,
    const ::deformation::IndexExtent& tile);
  /**
   * @brief Integrate +/- velocity with scaling and squaring; returns nullopt-equivalent empty members on cancellation.
   * @details Uses +/-v/2^s then s self-compositions, with s in [0, 20]. Larger s
   * does not guarantee improved accuracy: interpolation and float32 error remain.
   * Returns a candidate only; no convergence or diffeomorphism claim is made.
   */
  [[nodiscard]] FieldPair exponential(const FieldTexture& velocity, unsigned squarings, const Cancel& cancel = {});
  /** @brief Return {increment.forward o previous.forward, previous.inverse o increment.inverse}; empty on cancellation.
   */
  [[nodiscard]] FieldPair accumulate(const FieldPair& previous, const FieldPair& increment, const Cancel& cancel = {});
  /**
   * @brief Write sampled {det(J), inverse residual mm, inverse residual voxels, valid}.
   * @details J is the intrinsic 2x2 or 3x3 central difference at +/- one sample, using
   * physical spacing and the domain frame. The residual samples inverse(forward(x)).
   * Missing stencil/round-trip coverage produces W=0, not a passing diagnostic.
   * Run with directions exchanged for the other round trip. These samples neither
   * verify cells nor supply singular values or an acceptance report.
   */
  void quality(const FieldTexture& forward, const FieldTexture& inverse, FieldTexture& output);
  /**
   * @brief Reduce a quality texture to counts and extrema over its one-sample-inset valid extent.
   * @details GPU gather passes stop while each compact output tile represents at
   * most 2^20 source samples, so float32 tile counts are exact integers. The CPU
   * sums those compact tiles in size_t. This is sampled evidence only: singular
   * values, protected motion, convergence, and cell checks are not included.
   */
  [[nodiscard]] ReducedDirectionQuality reduceQuality(const FieldTexture& qualityMap, bool inset = true);
  /**
   * @brief Build sampled quality evidence for one map direction without full-field readback.
   * @details Stretch fields are conservative algebraic lower/upper bounds from
   * the physical Jacobian's determinant and Frobenius norm. Returned evidence
   * is not cell verification, convergence evidence, or a publication decision.
   */
  [[nodiscard]] ::deformation::DirectionQuality analyzeDirection(
    const FieldTexture& forward,
    const FieldTexture& inverse);
  /** @brief Analyze both directions; protection, convergence, and cells remain explicitly unchecked. */
  [[nodiscard]] ::deformation::QualityReport sampledReport(const FieldPair& pair);
  /** @brief Read candidate fields for bounded double-precision interior-cell verification. */
  [[nodiscard]] PairCellVerification verifyCells(
    const FieldPair& pair,
    const ::deformation::QualityPolicy& policy,
    unsigned maxDepth = 3,
    std::size_t maxSamples = 4'000'000);
  /** @brief Read both candidate and refined pairs for physical center/off-grid comparison. */
  [[nodiscard]] ::deformation::RefinementEvidence
  compareRefinement(const FieldPair& candidate, const FieldPair& refined, std::size_t maxPoints = 2'000'000);
  /** @brief Compare same-grid pair refinements through compact GPU reductions. */
  [[nodiscard]] ::deformation::RefinementEvidence reduceRefinement(
    const FieldPair& candidate,
    const FieldPair& refined);
  /** @brief Read the increment pair to bound displacement throughout protected cores. */
  [[nodiscard]] ::deformation::ProtectionEvidence measureProtectedCores(
    const FieldPair& increment,
    std::span<const ::deformation::ProtectedRegion> regions,
    double targetErrorMm,
    unsigned maxDepth = 5,
    std::size_t maxCells = 1'000'000);
  /** @brief Compact check for exact zero displacement at every protected-cell corner. */
  [[nodiscard]] ProtectedCorePrecheck precheckProtectedCores(
    const FieldPair& increment,
    std::span<const ::deformation::ProtectedRegion> regions);
  /**
   * @brief Integrate and assess a velocity against an immutable accepted pair.
   * @details Each attempt starts from the same velocity and compares squaring
   * counts s and s+1. Only inverse/convergence retries increase s. Hard
   * failures and incomplete cell/protection evidence stop without publishing.
   * The returned pair is populated only on acceptance; previous is never
   * modified. A caller must handle exceptions as nonpublication too.
   */
  [[nodiscard]] CandidateResult acceptVelocity(
    const FieldTexture& velocity,
    const FieldPair* previous,
    std::span<const ::deformation::ProtectedRegion> regions,
    const ::deformation::QualityPolicy& policy = {},
    unsigned firstSquarings = 5,
    unsigned maxAttempts = 3,
    const Cancel& cancel = {});

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
} // namespace rendering::deformation
