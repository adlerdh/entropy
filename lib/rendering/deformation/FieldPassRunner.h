#pragma once

#include "deformation/Types.h"
#include "deformation/VelocityLattice.h"
#include "rendering/deformation/FieldTextures.h"

#include <cstddef>
#include <functional>
#include <memory>

namespace rendering::deformation
{
/** @brief A candidate forward/inverse pair; neither member is a quality or acceptance certificate. */
struct FieldPair
{
  std::unique_ptr<FieldTexture> forward; //!< F(x) = x + u(x).
  std::unique_ptr<FieldTexture> inverse; //!< G(y) = y + v(y), integrated from negative velocity.
};

/**
 * @brief GL 3.3 fragment-pass backend for native-2D stationary velocity flows.
 * @details All vector values are LPS mm, not voxel offsets. Operations require
 * identical field grids and a native-2D plane; 3D and cross-grid composition are
 * deliberately rejected in Stage 2. Coordinates are recentered in double
 * precision before float32 shader arithmetic. Bilinear field sampling requires
 * valid, finite, tangent contributors; outside coverage is invalid, never clamped
 * or replaced by identity. An output may not alias any input texture object.
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
  /** @brief Evaluate cubic generator coefficients, then continuous support/protection; at most 32 protected disks. */
  void velocity(const ::deformation::VelocityLattice& lattice, FieldTexture& output);
  /** @brief Write a signed, scaled velocity as the initial Euler displacement; finite scale required. */
  void seed(const FieldTexture& velocity, float scale, FieldTexture& output);
  /** @brief Write outer(inner(x)) - x, with validity propagated through both samples. */
  void compose(const FieldTexture& outer, const FieldTexture& inner, FieldTexture& output);
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
   * @details J is the intrinsic 2x2 central difference at +/- one sample, using
   * physical spacing and the domain frame. The residual samples inverse(forward(x)).
   * Missing stencil/round-trip coverage produces W=0, not a passing diagnostic.
   * Run with directions exchanged for the other round trip. These samples neither
   * verify cells nor supply singular values or an acceptance report (Stage 3).
   */
  void quality(const FieldTexture& forward, const FieldTexture& inverse, FieldTexture& output);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};
} // namespace rendering::deformation
