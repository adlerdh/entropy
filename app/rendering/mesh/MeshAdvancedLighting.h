#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Availability state for an optional advanced mesh-lighting feature
 */
enum class MeshAdvancedLightingFeatureState
{
  Disabled,    //!< User did not request the feature
  Unavailable, //!< User requested the feature, but the renderer cannot run it yet
  Enabled      //!< Feature is requested and supported by the renderer
};

/**
 * @brief Renderer capabilities for optional mesh-lighting passes
 */
struct MeshAdvancedLightingCapabilities
{
  bool shadowMapPassAvailable = false;        //!< Shadow-map render pass is implemented and usable
  bool ambientOcclusionPassAvailable = false; //!< Ambient occlusion render pass is implemented and usable
};

/**
 * @brief User-facing settings for optional mesh shadows
 */
struct MeshShadowSettings
{
  bool enabled = false;          //!< Request shadow-map based lighting
  uint32_t mapSizePixels = 1024; //!< Requested square shadow-map size in pixels
  float strength = 0.35f;        //!< Shadow contribution in [0, 1]
  float depthBias = 0.001f;      //!< Depth bias used to suppress self-shadowing
};

/**
 * @brief User-facing settings for optional mesh ambient occlusion
 */
struct MeshAmbientOcclusionSettings
{
  bool enabled = false;      //!< Request an ambient occlusion pass
  float radiusPixels = 8.0f; //!< Screen-space occlusion sample radius in pixels
  float strength = 0.5f;     //!< Occlusion contribution in [0, 1]
  uint32_t sampleCount = 16; //!< Requested number of AO samples
};

/**
 * @brief Renderer-wide settings for optional advanced mesh lighting
 */
struct MeshAdvancedLightingSettings
{
  MeshShadowSettings shadows;                    //!< Optional mesh shadow settings
  MeshAmbientOcclusionSettings ambientOcclusion; //!< Optional mesh ambient occlusion settings
};

/**
 * @brief Sanitized shadow-map settings safe for renderer use
 */
struct MeshShadowPlan
{
  MeshAdvancedLightingFeatureState state = MeshAdvancedLightingFeatureState::Disabled; //!< Shadow pass state
  uint32_t mapSizePixels = 1024;                                                       //!< Sanitized shadow-map size
  float strength = 0.35f;                                                              //!< Sanitized shadow strength
  float depthBias = 0.001f;                                                            //!< Sanitized shadow bias
};

/**
 * @brief Sanitized ambient occlusion settings safe for renderer use
 */
struct MeshAmbientOcclusionPlan
{
  MeshAdvancedLightingFeatureState state = MeshAdvancedLightingFeatureState::Disabled; //!< AO pass state
  float radiusPixels = 8.0f;                                                           //!< Sanitized radius in pixels
  float strength = 0.5f;                                                               //!< Sanitized AO strength
  uint32_t sampleCount = 16;                                                           //!< Sanitized AO sample count
};

/**
 * @brief Renderer decision for optional advanced mesh lighting
 */
struct MeshAdvancedLightingPlan
{
  MeshShadowPlan shadows;                    //!< Shadow-map pass decision
  MeshAmbientOcclusionPlan ambientOcclusion; //!< Ambient occlusion pass decision
};

/**
 * @brief Return whether a feature is requested but cannot run
 * @param state Feature state to inspect
 * @return True when the user requested an unavailable feature
 */
bool isRequestedButUnavailable(MeshAdvancedLightingFeatureState state) noexcept;

/**
 * @brief Clamp shadow-map settings and account for renderer capability
 * @param settings Requested shadow-map settings
 * @param capabilities Renderer capabilities
 * @return Sanitized shadow-map plan
 */
MeshShadowPlan meshShadowPlan(
  const MeshShadowSettings& settings,
  const MeshAdvancedLightingCapabilities& capabilities) noexcept;

/**
 * @brief Clamp ambient occlusion settings and account for renderer capability
 * @param settings Requested ambient occlusion settings
 * @param capabilities Renderer capabilities
 * @return Sanitized ambient occlusion plan
 */
MeshAmbientOcclusionPlan meshAmbientOcclusionPlan(
  const MeshAmbientOcclusionSettings& settings,
  const MeshAdvancedLightingCapabilities& capabilities) noexcept;

/**
 * @brief Clamp all advanced lighting settings and account for renderer capability
 * @param settings Requested advanced lighting settings
 * @param capabilities Renderer capabilities
 * @return Sanitized advanced lighting plan
 */
MeshAdvancedLightingPlan meshAdvancedLightingPlan(
  const MeshAdvancedLightingSettings& settings,
  const MeshAdvancedLightingCapabilities& capabilities) noexcept;

/**
 * @brief Build short diagnostics for requested advanced-lighting features that cannot run
 * @param plan Sanitized advanced lighting plan
 * @return Human-readable diagnostics for unavailable requested features
 */
std::vector<std::string> advancedLightingDiagnostics(const MeshAdvancedLightingPlan& plan);

} // namespace rendering::mesh
