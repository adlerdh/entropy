#pragma once

#include "logic/serialization/ProjectSerialization.h"
#include "deformation/EditHistory.h"
#include "viewer/ViewTypes.h"

#include <uuid.h>

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class AppData;

/// Capture application state for project saving and dirty checks without a window or OpenGL context.
/// Integration tests use these adapters to exercise the same snapshots as the application.
namespace project_snapshot
{
/// Original DICOM source descriptions indexed by image UUID.
using DicomSources = std::unordered_map<uuids::uuid, serialize::DicomSource>;
/// Native view orientations indexed by image UUID, used to reconstruct default layouts.
using NativeViews = std::unordered_map<uuids::uuid, ViewType>;

/** @brief An accepted editor history to publish with the next project snapshot. */
struct DeformationArchiveSource
{
  std::string editId;
  const deformation::EditHistory* history = nullptr;
};

/** @brief Verify and restore every referenced edit history without partial publication. */
[[nodiscard]] std::map<std::string, deformation::EditHistory> restoreDeformationHistories(
  const serialize::EntropyProject& project,
  std::size_t maxTotalFieldBytes);

/**
 * @brief Capture images, layouts, presentation settings, and registration results without writing files.
 * @param data Application state to snapshot.
 * @param dicomSources Optional DICOM provenance indexed by image UUID.
 * @param nativeViews Optional native orientations used when comparing layouts with their defaults.
 * @return Serializable project, or a default-constructed project when no images are loaded.
 */
serialize::EntropyProject
captureProject(const AppData& data, const DicomSources& dicomSources = {}, const NativeViews& nativeViews = {});

/**
 * @brief Capture an image's settings and associated segmentations, landmarks, annotations, surfaces, and warps.
 * @param data Application state owning the image and its associated assets.
 * @param dicomSources DICOM provenance indexed by image UUID.
 * @param imageUid Image to snapshot.
 * @param defaultBorderColor Optional generated default color used to omit an unchanged border color.
 * @return Serializable image; a missing image is logged and returns a default-constructed record.
 */
serialize::Image captureImage(
  const AppData& data,
  const DicomSources& dicomSources,
  const uuids::uuid& imageUid,
  const std::optional<glm::vec3>& defaultBorderColor = std::nullopt);

/// Callback that writes a snapshot to the supplied path and returns true on success.
using ProjectWriter = std::function<bool(const serialize::EntropyProject&, const std::filesystem::path&)>;

/**
 * @brief Persist generated warp assets, capture the project, and invoke its writer.
 * @param data Application state; generated warp paths and on-disk flags are updated on success.
 * @param normalizedFileName Normalized project destination; generated warps use an adjacent .assets directory.
 * @param dicomSources Optional DICOM provenance indexed by image UUID.
 * @param nativeViews Optional native orientations used to reconstruct default layouts.
 * @param writeProject Project writer, injectable for persistence tests.
 * @param deformationArchives Accepted edit histories to publish as immutable bundles before the project file.
 * @return Saved snapshot, or std::nullopt on asset-save failure, a missing reference image, or writer failure.
 * @details On failure, restores generated warp metadata and removes newly published warp and edit assets.
 * The supplied writer is responsible for publishing the project file safely.
 */
std::optional<serialize::EntropyProject> persistProject(
  AppData& data,
  const std::filesystem::path& normalizedFileName,
  const DicomSources& dicomSources = {},
  const NativeViews& nativeViews = {},
  const ProjectWriter& writeProject = serialize::save,
  const std::vector<DeformationArchiveSource>& deformationArchives = {});
} // namespace project_snapshot
