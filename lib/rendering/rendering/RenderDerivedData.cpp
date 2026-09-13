#include "rendering/RenderDerivedData.h"

#include <cstddef>

namespace rendering
{

RenderDerivedData::IsosurfaceData::IsosurfaceData()
  : values(8, 0.0f)
  , opacities(8, 0.0f)
  , rimOpacityStrengths(8, 0.0f)
  , rimEmissionStrengths(8, 0.0f)
  , rimPowers(8, 2.0f)
  , colors(8, glm::vec3{0.0f})
{
}

void RenderDerivedData::clear() noexcept
{
  imageUniforms.clear();
  isosurfaces = IsosurfaceData{};
}

void RenderDerivedData::removeImage(const uuids::uuid& imageUid) noexcept
{
  imageUniforms.erase(imageUid);
}

} // namespace rendering
