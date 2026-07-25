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
 * @return Mesh draw context for renderer calls
 */
MeshDrawContext meshDrawContextForView(const MeshGpuStore& gpuStore, const View& view);

} // namespace rendering::mesh
