#pragma once
#include "mesh/MeshTransform.h"
#include <uuid.h>
#include <cstdint>
class AppData;
class Image;
namespace deformation_warp
{
uint64_t warpedGeometryVersion(const AppData& data, const uuids::uuid& uid, const Image& image);
std::expected<mesh::MeshGeometry, mesh::MeshTransformError>
prepareImportedGeometry(const AppData& data, const uuids::uuid& imageUid, const mesh::MeshGeometry& geometry);
} // namespace deformation_warp
