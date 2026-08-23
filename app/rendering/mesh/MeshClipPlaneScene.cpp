#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshDrawOptions.h"

#include <vector>

std::vector<rendering::mesh::MeshClipPlane> Rendering::meshClipPlanes() const
{
  const RenderData& renderData = m_appData.renderData();
  if (!renderData.m_meshClipPlaneEnabled) {
    return {};
  }

  return {rendering::mesh::MeshClipPlane{.worldPlane = renderData.m_meshClipPlaneWorld, .enabled = true}};
}
