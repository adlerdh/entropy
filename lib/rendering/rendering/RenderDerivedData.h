#pragma once

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <uuid.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rendering
{

/**
 * @brief Context-free shader inputs derived from host image and presentation state
 *
 * This cache owns neither persistent settings nor GPU resources. A host adapter may rebuild or discard it at any time.
 */
struct RenderDerivedData
{
  struct ImageUniforms
  {
    glm::vec2 cmapSlopeIntercept{1.0f, 0.0f};
    int cmapQuantLevels = 0;
    glm::vec3 hsvModFactors{0.0f, 1.0f, 1.0f};
    glm::mat4 imgTexture_T_world{1.0f};
    glm::mat4 world_T_imgTexture{1.0f};
    glm::mat4 segTexture_T_world{1.0f};
    glm::vec3 voxelSpacing{1.0f};
    glm::vec3 subjectBoxMinCorner{0.0f};
    glm::vec3 subjectBoxMaxCorner{0.0f};
    glm::mat3 textureGradientStep{1.0f};
    glm::vec2 slopeIntercept_normalized_T_texture{1.0f, 0.0f};
    std::vector<glm::vec2> slopeInterceptRgba_normalized_T_texture{
      {1.0f, 0.0f},
      {1.0f, 0.0f},
      {1.0f, 0.0f},
      {1.0f, 0.0f}};
    float slope_native_T_texture{1.0f};
    glm::vec2 largestSlopeIntercept{1.0f, 0.0f};
    glm::vec2 minMax{0.0f, 1.0f};
    glm::vec2 thresholds{0.0f, 1.0f};
    std::vector<glm::vec2> thresholdsRgba{{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};
    std::vector<glm::vec2> minMaxRgba{{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};
    float imgOpacity{0.0f};
    std::vector<float> imgOpacityRgba = {0.0f, 0.0f, 0.0f, 0.0f};
    float segOpacity{0.0f};
    bool showVoxelEdges = false;
    bool showScreenPixelEdges = false;
    bool hardEdges = false;
    bool thinPixelEdges = true;
    bool overlayEdges = false;
    bool colormapEdges = false;
    float voxelEdgeScale = 4.0f;
    float voxelEdgeThreshold = 0.25f;
    float pixelEdgeScale = 2.0f;
    float pixelEdgeThreshold = 0.2f;
    glm::vec4 edgeColor{0.0f};
  };

  struct IsosurfaceData
  {
    IsosurfaceData();

    int numIsos{0};
    std::vector<float> values;
    std::vector<float> opacities;
    std::vector<float> rimOpacityStrengths;
    std::vector<float> rimEmissionStrengths;
    std::vector<float> rimPowers;
    std::vector<glm::vec3> colors;
  };

  void clear() noexcept;
  void removeImage(const uuids::uuid& imageUid) noexcept;

  std::unordered_map<uuids::uuid, ImageUniforms> imageUniforms;
  IsosurfaceData isosurfaces;
};

} // namespace rendering
