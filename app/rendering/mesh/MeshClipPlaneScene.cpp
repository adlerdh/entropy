#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/RenderResources.h"
#include "rendering/RenderSettings.h"
#include "rendering/mesh/MeshDrawOptions.h"

#include <vector>

std::vector<rendering::mesh::MeshClipPlane> Rendering::meshClipPlanes() const
{
  const rendering::RenderSettings& renderSettings = m_appData.renderSettings();
  if (!renderSettings.m_meshClipPlaneEnabled) {
    return {};
  }

  return {rendering::mesh::MeshClipPlane{.worldPlane = renderSettings.m_meshClipPlaneWorld, .enabled = true}};
}
