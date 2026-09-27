#pragma once

#include "logic/serialization/ProjectSerialization.h"
#include "viewer/ViewTypes.h"
#include <unordered_map>
#include <functional>
#include <uuid.h>

class AppData;
namespace project_snapshot
{
using DicomSources = std::unordered_map<uuids::uuid, serialize::DicomSource>;
using NativeViews = std::unordered_map<uuids::uuid, ViewType>;

// These are the application's save/dirty-check adapters, independent of a window
// or GL context. Integration tests must call these instead of constructing JSON.
serialize::EntropyProject
captureProject(const AppData& data, const DicomSources& dicomSources = {}, const NativeViews& nativeViews = {});
serialize::Image captureImage(
  const AppData& data,
  const DicomSources& dicomSources,
  const uuids::uuid& imageUid,
  const std::optional<glm::vec3>& defaultBorderColor = std::nullopt);
using ProjectWriter = std::function<bool(const serialize::EntropyProject&, const std::filesystem::path&)>;
/// Publish generated warp assets and project atomically with metadata rollback on failure.
std::optional<serialize::EntropyProject> persistProject(
  AppData& data,
  const std::filesystem::path& normalizedFileName,
  const DicomSources& dicomSources = {},
  const NativeViews& nativeViews = {},
  const ProjectWriter& writeProject = serialize::save);
} // namespace project_snapshot
