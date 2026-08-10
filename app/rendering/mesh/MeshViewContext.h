#pragma once

#include "rendering/mesh/MeshRenderer.h"

class View;

namespace rendering::mesh
{

class MeshGpuStore;

/**
 * @brief Build the camera and GPU-lookup context used by mesh draw passes for one view
 * @param gpuStore Uploaded mesh buffer store for the current OpenGL context
 * @param view View whose camera defines the mesh projection
 * @param lighting Ambient, diffuse, specular, and specular power for 3D ADS lighting
 * @return Mesh draw context for renderer calls
 */
MeshDrawContext meshDrawContextForView(
  const MeshGpuStore& gpuStore,
  const View& view,
  const glm::vec4& lighting = glm::vec4{0.35f, 0.55f, 0.10f, 16.0f});

} // namespace rendering::mesh
