#include "rendering/mesh/MeshDdpPolicy.h"

#include <algorithm>
#include <string>
#include <vector>

namespace rendering::mesh
{

uint32_t sanitizedDdpPeelPasses(const uint32_t requestedPasses, const uint32_t maxPasses) noexcept
{
  if (maxPasses == 0) {
    return 0;
  }

  return std::clamp(requestedPasses, 1u, maxPasses);
}

MeshDdpPlan meshDdpPlanForRenderList(const MeshRenderList& list, const MeshDdpSettings& settings) noexcept
{
  static constexpr uint32_t k_hardMaxPeelPasses = 32u;
  const uint32_t renderableCount = static_cast<uint32_t>(list.alphaOverDdp.size());
  const uint32_t peelPasses = sanitizedDdpPeelPasses(settings.maxPeelPasses, k_hardMaxPeelPasses);
  return MeshDdpPlan{
    .active = settings.enabled && renderableCount > 0,
    .peelPasses = settings.enabled && renderableCount > 0 ? peelPasses : 0,
    .renderableCount = renderableCount};
}

MeshDdpPlan meshDdpPlanWithExtraRenderables(
  const MeshDdpPlan& plan,
  const MeshDdpSettings& settings,
  const uint32_t extraRenderableCount) noexcept
{
  if (extraRenderableCount == 0u) {
    return plan;
  }

  static constexpr uint32_t k_hardMaxPeelPasses = 32u;
  const uint32_t renderableCount = plan.renderableCount + extraRenderableCount;
  const uint32_t peelPasses = sanitizedDdpPeelPasses(settings.maxPeelPasses, k_hardMaxPeelPasses);
  return MeshDdpPlan{
    .active = settings.enabled,
    .peelPasses = settings.enabled ? peelPasses : 0u,
    .renderableCount = renderableCount};
}

bool shouldContinueDdpPeeling(
  const uint32_t completedPasses,
  const MeshDdpPlan& plan,
  const bool anySamplesPassed) noexcept
{
  return plan.active && completedPasses < plan.peelPasses && anySamplesPassed;
}

std::vector<std::string> meshDdpDiagnostics(const MeshDdpPlan& plan, const MeshDdpSettings& settings)
{
  std::vector<std::string> diagnostics;
  if (!settings.enabled && plan.renderableCount > 0) {
    diagnostics.emplace_back("Mesh DDP is disabled; alpha-over mesh surfaces will not use order independent blending");
  }
  if (plan.active && plan.peelPasses < settings.maxPeelPasses) {
    diagnostics.emplace_back("Mesh DDP peel pass count was clamped to the renderer limit");
  }
  return diagnostics;
}

} // namespace rendering::mesh
