#pragma once

#include "mesh/MeshTransform.h"

#include <uuid.h>
#include <cstdint>

class AppData;
class Image;
namespace deformation_warp
{
/// Hash the image geometry, affine transform, warp strength, and active forward-warp state for mesh cache invalidation.
/// The image must correspond to imageUid. This is a cache fingerprint, not a monotonically increasing revision.
uint64_t warpedGeometryVersion(const AppData& appData, const uuids::uuid& imageUid, const Image& image);

/// Copy imported geometry from image subject coordinates into display world coordinates, applying the image's affine
/// transform followed by its enabled forward deformation. Points without an applicable warp retain their affine
/// position. Repairs reflected winding and regenerates normals. Returns an error if the image is missing or
/// transformation fails.
std::expected<mesh::MeshGeometry, mesh::MeshTransformError>
prepareImportedGeometry(const AppData& data, const uuids::uuid& imageUid, const mesh::MeshGeometry& geometry);
} // namespace deformation_warp
