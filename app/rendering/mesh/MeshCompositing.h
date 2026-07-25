#pragma once

namespace rendering::mesh
{

/**
 * @brief Compositing path requested for a mesh renderable
 */
enum class MeshCompositingMode
{
  Opaque,
  AlphaOverDdp,
  Additive,
  Multiplicative
};

} // namespace rendering::mesh
