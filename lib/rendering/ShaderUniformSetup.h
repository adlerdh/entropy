#pragma once

#include "rendering/utility/containers/Uniforms.h"

namespace rendering::shader_setup
{

/**
 * @brief Uniform declarations grouped by shader stage and rendering feature.
 *
 * These sets are shared by multiple shader program definitions. Keeping them in one structure makes the shader program
 * table declarative while avoiding repeated uniform construction code.
 */
struct ShaderUniformSet
{
  Uniforms vsClipWorldUniforms;
  Uniforms vsViewModeUniforms;
  Uniforms vsImageUniforms;
  Uniforms vsImageWarpedUniforms;
  Uniforms vsSegUniforms;
  Uniforms vsSegWarpedUniforms;
  Uniforms vsMetricUniforms;
  Uniforms vsMetricWarpedUniforms;
  Uniforms fsImageAdjustmentUniforms;
  Uniforms fsColorMapUniforms;
  Uniforms fsRenderModeUniforms;
  Uniforms fsIntensityProjectionUniforms;
  Uniforms fsDeformationUniforms;
  Uniforms fsImageGrayUniforms;
  Uniforms fsImageGrayWarpedUniforms;
  Uniforms fsImageColorUniforms;
  Uniforms fsImageColorWarpedUniforms;
  Uniforms fsVectorDirectionColorUniforms;
  Uniforms fsVectorSignedNormalProjectionUniforms;
  Uniforms fsVectorPlanarProjectionColorUniforms;
  Uniforms fsVectorWarpedGridUniforms;
  Uniforms fsEdgeUniforms;
  Uniforms fsEdgeWarpedUniforms;
  Uniforms fsXrayUniforms;
  Uniforms fsXrayWarpedUniforms;
  Uniforms fsSegAdjustmentUniforms;
  Uniforms fsSegNearestUniforms;
  Uniforms fsSegNearestWarpedUniforms;
  Uniforms fsSegLinearUniforms;
  Uniforms fsSegLinearWarpedUniforms;
  Uniforms fsIsoUniforms;
  Uniforms fsIsoWarpedUniforms;
  Uniforms fsMetricDeformationUniforms;
  Uniforms fsDiffUniforms;
  Uniforms fsDiffWarpedUniforms;
  Uniforms fsLocalNccUniforms;
  Uniforms fsLocalNccWarpedUniforms;
  Uniforms fsLocalLinearResidualUniforms;
  Uniforms fsLocalLinearResidualWarpedUniforms;
  Uniforms fsOverlayUniforms;
  Uniforms fsOverlayWarpedUniforms;
};

/**
 * @brief Build all reusable uniform sets for the main image-rendering shader programs.
 */
ShaderUniformSet buildShaderUniformSet();

} // namespace rendering::shader_setup
