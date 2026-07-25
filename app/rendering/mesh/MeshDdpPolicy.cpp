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
  const uint32_t renderableCount = static_cast<uint32_t>(list.alphaOverDdp.size());
  const uint32_t peelPasses = sanitizedDdpPeelPasses(settings.requestedPeelPasses, settings.maxPeelPasses);
  return MeshDdpPlan{
    .active = settings.enabled && renderableCount > 0 && peelPasses > 0,
    .peelPasses = settings.enabled && renderableCount > 0 ? peelPasses : 0,
    .renderableCount = renderableCount};
}

std::vector<std::string> meshDdpDiagnostics(const MeshDdpPlan& plan, const MeshDdpSettings& settings)
{
  std::vector<std::string> diagnostics;
  if (!settings.enabled && plan.renderableCount > 0) {
    diagnostics.emplace_back("Mesh DDP is disabled; alpha-over mesh surfaces will not use order independent blending");
  }
  if (settings.maxPeelPasses == 0 && plan.renderableCount > 0) {
    diagnostics.emplace_back("Mesh DDP has no available peel passes");
  }
  if (plan.active && plan.peelPasses < settings.requestedPeelPasses) {
    diagnostics.emplace_back("Mesh DDP peel pass count was clamped to the renderer limit");
  }
  return diagnostics;
}

} // namespace rendering::mesh
