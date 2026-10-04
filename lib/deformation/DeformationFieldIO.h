#pragma once

#include "deformation/FieldDomain.h"

#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace deformation
{
/** @brief One native field asset: LPS-mm XYZ displacement and W validity. */
struct FieldAsset
{
  FieldDomain domain;
  MapDirection role;
  std::vector<glm::vec4> samples;
};

/** @brief CRC32 of canonical little-endian RGBA32F payload bytes. */
[[nodiscard]] std::uint32_t nativeFieldPayloadChecksum(const std::vector<glm::vec4>& samples) noexcept;

/** @brief Reject malformed validity or non-finite/off-plane usable vectors before exporting an accepted map. */
void validateFieldForExport(const FieldAsset& asset);

/**
 * @brief Write an uncompressed float32 NRRD with explicit native dimension,
 * full physical frame, vector basis, validity, role, and payload checksum.
 * @details The vector axis is fastest. The caller stages the destination when
 * atomic publication is required. Existing files are overwritten.
 */
void writeNativeField(const std::filesystem::path& path, const FieldAsset& asset);

/**
 * @brief Read only this versioned native field profile, checking geometry,
 * payload length, checksum, and a caller supplied byte limit before allocation.
 */
[[nodiscard]] FieldAsset readNativeField(const std::filesystem::path& path, std::size_t maxBytes);
} // namespace deformation
