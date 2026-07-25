#pragma once

#include "rendering/mesh/MeshRenderList.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief User and renderer limits that control dual depth peeling
 */
struct MeshDdpSettings
{
  bool enabled = true;              //!< Whether alpha-over order-independent transparency is allowed
  uint32_t requestedPeelPasses = 8; //!< Requested number of front/back peel iterations
  uint32_t maxPeelPasses = 32;      //!< Hard safety cap for one frame
};

/**
 * @brief Sanitized DDP decision for one render list
 */
struct MeshDdpPlan
{
  bool active = false;          //!< True when a DDP pass should run
  uint32_t peelPasses = 0;      //!< Number of front/back peel iterations to execute
  uint32_t renderableCount = 0; //!< Number of alpha-over renderables that require DDP
};

/**
 * @brief Clamp the requested DDP peel count to a valid per-frame range
 * @param requestedPasses User or renderer requested pass count
 * @param maxPasses Renderer safety cap
 * @return Valid peel count in the range [1, maxPasses], or zero when maxPasses is zero
 */
uint32_t sanitizedDdpPeelPasses(uint32_t requestedPasses, uint32_t maxPasses) noexcept;

/**
 * @brief Decide whether dual depth peeling should be used for a render list
 * @param list Render list to inspect
 * @param settings DDP settings and limits
 * @return DDP pass plan
 */
MeshDdpPlan meshDdpPlanForRenderList(const MeshRenderList& list, const MeshDdpSettings& settings) noexcept;

/**
 * @brief Build short diagnostics that explain a DDP plan or fallback
 * @param plan Sanitized DDP plan
 * @param settings Settings used to produce the plan
 * @return Human-readable diagnostics for inactive or clamped DDP behavior
 */
std::vector<std::string> meshDdpDiagnostics(const MeshDdpPlan& plan, const MeshDdpSettings& settings);

} // namespace rendering::mesh
