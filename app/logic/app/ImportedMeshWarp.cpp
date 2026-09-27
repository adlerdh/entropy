#include "logic/app/ImportedMeshWarp.h"
#include "logic/app/Data.h"
#include "logic/app/DeformationWarp.h"
#include <bit>
#include <stdexcept>
namespace deformation_warp
{
class ForwardWarpTransform final : public mesh::IPointTransform
{
public:
  ForwardWarpTransform(const AppData& appData, const uuids::uuid& imageUid) : m_appData{appData}, m_imageUid{imageUid}
  {
  }

  [[nodiscard]] std::expected<glm::dvec3, std::string> transformPoint(const glm::dvec3& point) const override
  {
    const glm::vec4 result =
      deformation_warp::forwardWarpDisplayWorldPosition(m_appData, m_imageUid, glm::vec4{glm::vec3{point}, 1.0f});
    return glm::dvec3{result} / static_cast<double>(result.w);
  }

private:
  const AppData& m_appData;
  uuids::uuid m_imageUid;
};

void hashCombine(uint64_t& seed, const uint64_t value)
{
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6u) + (seed >> 2u);
}

uint64_t warpedGeometryVersion(const AppData& appData, const uuids::uuid& imageUid, const Image& image)
{
  uint64_t version = 1;
  hashCombine(version, image.geometryRevision());
  const glm::mat4& transform = image.transformations().worldDef_T_subject();

  for (glm::length_t column = 0; column < 4; ++column) {
    for (glm::length_t row = 0; row < 4; ++row) {
      hashCombine(version, std::bit_cast<uint32_t>(transform[column][row]));
    }
  }

  hashCombine(version, std::bit_cast<uint32_t>(image.settings().warpStrength()));

  if (const auto warpUid = appData.imageToActiveForwardWarpUid(imageUid)) {
    hashCombine(version, std::hash<uuids::uuid>{}(*warpUid));
    if (const Image* warp = appData.warpField(*warpUid)) {
      hashCombine(version, warp->pixelDataRevision());
      hashCombine(version, warp->geometryRevision());
      hashCombine(version, warp->settings().activeTimePoint());
      const auto& warpTransform = warp->transformations().worldDef_T_subject();
      for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
          hashCombine(version, std::bit_cast<uint32_t>(warpTransform[column][row]));
        }
      }
    }
  }
  return version;
}
} // namespace deformation_warp

std::expected<mesh::MeshGeometry, mesh::MeshTransformError> deformation_warp::prepareImportedGeometry(
  const AppData& data,
  const uuids::uuid& imageUid,
  const mesh::MeshGeometry& geometry)
{
  const Image* image = data.image(imageUid);
  if (!image)
    return std::unexpected(
      mesh::MeshTransformError{mesh::MeshTransformErrorCode::InvalidGeometry, "Missing image for imported mesh"});
  const ForwardWarpTransform deformation{data, imageUid};
  return mesh::transformGeometry(geometry, glm::dmat4{image->transformations().worldDef_T_subject()}, &deformation);
}
