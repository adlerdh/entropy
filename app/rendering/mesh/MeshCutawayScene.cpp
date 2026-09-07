#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "rendering/RenderSettings.h"
#include "rendering/mesh/MeshCutaway.h"
#include "windowing/View.h"

#include <glm/mat3x3.hpp>

rendering::mesh::MeshOctantCutaway Rendering::meshCutawayForView(const View& view) const
{
  if (!m_appData.renderSettings().m_meshCutawayEnabled) {
    return {};
  }

  const CoordinateFrame& crosshairs = m_appData.state().worldCrosshairs();
  const glm::vec3 crosshairsOrigin = crosshairs.worldOrigin();
  const glm::vec3 viewerPosition = view.camera().isOrthographic()
                                     ? crosshairsOrigin - helper::worldDirection(view.camera(), Directions::View::Front)
                                     : helper::worldOrigin(view.camera());
  return rendering::mesh::viewerFacingOctantCutaway(
           crosshairsOrigin,
           glm::mat3{crosshairs.world_T_frame()},
           viewerPosition)
    .value_or(rendering::mesh::MeshOctantCutaway{});
}
