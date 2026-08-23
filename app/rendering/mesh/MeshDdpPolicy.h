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
  bool enabled = true;        //!< Whether alpha-over order-independent transparency is allowed
  bool untilComplete = true;  //!< Stop early when an occlusion query reports no newly peeled back fragments
  uint32_t maxPeelPasses = 8; //!< Maximum front/back peel iterations executed in one frame
};

/**
 * @brief Sanitized DDP decision for one render list
 */
struct MeshDdpPlan
{
  bool active = false;          //!< True when a DDP pass should run
  bool untilComplete = true;    //!< Whether an occlusion query may terminate peeling early
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
 * @brief Decide whether another DDP peel iteration should run
 * @param completedPasses Number of peel iterations already completed
 * @param plan Sanitized DDP plan
 * @param anySamplesPassed Result of the latest completion occlusion query
 * @return True when the configured pass limit and completion policy allow another iteration
 */
bool shouldContinueDdpPeeling(uint32_t completedPasses, const MeshDdpPlan& plan, bool anySamplesPassed) noexcept;

/**
 * @brief Build short diagnostics that explain a DDP plan or fallback
 * @param plan Sanitized DDP plan
 * @param settings Settings used to produce the plan
 * @return Human-readable diagnostics for inactive or clamped DDP behavior
 */
std::vector<std::string> meshDdpDiagnostics(const MeshDdpPlan& plan, const MeshDdpSettings& settings);

} // namespace rendering::mesh
