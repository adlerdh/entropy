#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace rendering::mesh
{

/**
 * @brief Runtime and geometry options for CPU mesh generation backends
 */
struct MeshGenerationOptions
{
  /**
   * @brief Maximum number of CPU threads the backend may use
   *
   * A value of zero chooses a conservative automatic count that leaves CPU capacity for the UI, texture uploads, and
   * other background work. Thread count affects execution only and is not part of a geometry cache key.
   */
  std::size_t threadCount = 0;

  bool smoothSurface = true;         //!< Apply boundary-preserving smoothing to the extracted surface
  uint32_t smoothingIterations = 25; //!< Windowed-sinc iterations when smoothing is enabled
  double smoothingPassBand = 0.1;    //!< Windowed-sinc pass band in the open interval (0, 2]

  bool operator==(const MeshGenerationOptions&) const = default;
};

/** Fold geometry-changing generation options into a backend's base algorithm version. */
inline uint64_t meshGenerationAlgorithmVersion(uint64_t baseVersion, const MeshGenerationOptions& options) noexcept
{
  const auto combine = [](uint64_t seed, const uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
  };
  uint64_t version = combine(baseVersion, static_cast<uint64_t>(options.smoothSurface));
  version = combine(version, static_cast<uint64_t>(options.smoothingIterations));
  return combine(version, std::bit_cast<uint64_t>(options.smoothingPassBand));
}

} // namespace rendering::mesh
