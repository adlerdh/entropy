#pragma once

#include "deformation/FieldDomain.h"
#include "rendering/gl/GLTexture.h"

#include <glm/vec4.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace rendering::deformation
{
/**
 * @brief One geometry-tagged RGBA32F texture, with explicit allocation limits.
 * @details XYZ holds LPS displacement/velocity in mm; W is validity (exactly 1
 * for usable samples, 0 otherwise). Diagnostics use the layout documented by
 * FieldPassRunner::quality instead. Samples are x-fastest, at integer indices.
 * Storage supports native 2D and 3D; Stage 2 numerical passes support only 2D.
 * Construction initializes every component to zero (invalid, not identity).
 *
 * Requires a current GL 3.3 context and loaded entry points throughout its
 * lifetime. Not thread-safe; destroy before releasing the owning GL context.
 * Upload/readback preserve caller GL state. The byte limit is per texture,
 * excluding driver overhead and the temporary host buffer used for transfer.
 */
class FieldTexture final
{
public:
  /** @brief Allocate bounded storage; throws invalid_argument for limits, runtime_error for GL allocation failure. */
  FieldTexture(const ::deformation::FieldDomain& domain, std::size_t maxBytes);
  FieldTexture(const FieldTexture&) = delete;
  FieldTexture& operator=(const FieldTexture&) = delete;

  /** @brief Return immutable physical geometry. */
  [[nodiscard]] const ::deformation::FieldDomain& domain() const noexcept;
  /** @brief Return the checked RGBA32F byte count, without driver overhead. */
  [[nodiscard]] std::size_t bytes() const noexcept;
  /** @brief Expose a read-only GL wrapper for sampling and framebuffer attachment; do not mutate its storage. */
  [[nodiscard]] const GLTexture& texture() const noexcept;
  /**
   * @brief Replace all samples; size must match sampleCount().
   * @details Values are not sanitized: shaders invalidate non-finite or off-plane
   * vectors. Non-finite uploads are useful for diagnosing damaged imported data.
   */
  void upload(std::span<const glm::vec4> samples);
  /** @brief Synchronize and return all samples, including invalid entries; intended for tests and diagnostics. */
  [[nodiscard]] std::vector<glm::vec4> readback() const;

private:
  ::deformation::FieldDomain m_domain;
  std::size_t m_bytes;
  GLTexture m_texture;
};
} // namespace rendering::deformation
