#pragma once

#include "mesh/MeshTypes.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace mesh
{

/// Return a user-facing name for a mesh format
[[nodiscard]] std::string_view formatName(MeshFormat format) noexcept;

/// Return every recognized filename extension for a mesh format
[[nodiscard]] std::vector<std::string_view> formatExtensions(MeshFormat format);

/// Infer a mesh format from a path, including native extension-less FreeSurfer names
[[nodiscard]] std::optional<MeshFormat> formatFromPath(const std::filesystem::path& path);

/// Return whether a path uses a recognized native FreeSurfer surface filename
[[nodiscard]] bool isRecognizedFreeSurferSurfacePath(const std::filesystem::path& path);

} // namespace mesh
