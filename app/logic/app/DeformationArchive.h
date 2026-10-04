#pragma once

#include "deformation/EditHistory.h"
#include "logic/serialization/ProjectSerialization.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace deformation_archive
{
/**
 * @brief Stage one immutable revision bundle beside a project.
 * @details publish atomically renames the complete bundle into the asset tree.
 * Unless commit follows successful project publication, destruction removes
 * the bundle and leaves the old project reference intact.
 */
class StagedBundle final
{
public:
  StagedBundle(const std::filesystem::path& projectFile, std::string editId, const deformation::EditHistory& history);
  ~StagedBundle();
  StagedBundle(const StagedBundle&) = delete;
  StagedBundle& operator=(const StagedBundle&) = delete;

  [[nodiscard]] serialize::ProjectDeformationReference publish();
  void commit() noexcept;

private:
  std::filesystem::path m_staging;
  std::filesystem::path m_final;
  std::string m_editId;
  deformation::RevisionId m_cursor;
  bool m_published = false;
  bool m_committed = false;
};

/** @brief Verify a bundle and restore exact saved values, branches, and cursor. */
[[nodiscard]] deformation::EditHistory loadBundle(
  const serialize::ProjectDeformationReference& reference,
  std::size_t maxTotalFieldBytes);
} // namespace deformation_archive
