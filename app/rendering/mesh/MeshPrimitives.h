#pragma once

#include "rendering/mesh/MeshData.h"

namespace rendering::mesh
{

/**
 * @brief Create a flat-shaded cube centered at the origin
 * @param edgeLength Cube edge length
 * @return Triangle mesh with duplicated face vertices and outward normals
 * @throw Propagates allocation failures
 */
MeshData makeCubeMesh(float edgeLength);

/**
 * @brief Create a smooth sphere centered at the origin
 * @param radius Sphere radius
 * @param rings Number of interior latitude rings
 * @param segments Number of longitude segments
 * @return Triangle mesh with shared vertices and outward normals
 * @throw Propagates allocation failures
 */
MeshData makeSphereMesh(float radius, uint32_t rings = 12, uint32_t segments = 24);

/**
 * @brief Create a smooth cylinder centered at the origin and aligned with the z-axis
 * @param radius Cylinder radius
 * @param height Cylinder height
 * @param segments Number of radial segments
 * @return Triangle mesh with side and cap vertices and outward normals
 * @throw Propagates allocation failures
 */
MeshData makeCylinderMesh(float radius, float height, uint32_t segments = 24);

} // namespace rendering::mesh
