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

/**
 * @brief Select the opaque or translucent path for a surface whose shader may vary fragment opacity
 */
inline MeshCompositingMode compositingModeForSurfaceAlpha(
  const float alpha,
  const bool opacityCanVary,
  const MeshCompositingMode translucentMode = MeshCompositingMode::AlphaOverDdp) noexcept
{
  return alpha >= 0.999f && !opacityCanVary ? MeshCompositingMode::Opaque : translucentMode;
}

} // namespace rendering::mesh
