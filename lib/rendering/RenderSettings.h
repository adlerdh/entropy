#pragma once

#include "common/Types.h"
#include "rendering/mesh/MeshAdvancedLighting.h"
#include "rendering/mesh/MeshDdpPolicy.h"
#include "rendering/mesh/MeshMaterial.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <uuid.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rendering
{

/**
 * @brief Persistent, context-free rendering presentation settings.
 *
 * This value object is safe to construct and serialize without an OpenGL context. It intentionally
 * owns no textures, buffers, shader programs, caches, jobs, or other runtime rendering state.
 */
struct RenderSettings
{
  enum class LocalNccPresentation
  {
    Dissimilarity,
    Correlation
  };

  enum class LocalNccInvalidStyle
  {
    Transparent,
    Gray
  };

  enum class SegMaskingForRaycasting
  {
    SegMasksIn,
    SegMasksOut,
    Disabled
  };

  struct MetricParams
  {
    std::size_t m_colorMapIndex = 0;
    glm::vec2 m_cmapSlopeIntercept{1.0f, 0.0f};
    glm::vec2 m_slopeIntercept{1.0f, 0.0f};
    bool m_invertCmap = false;
    bool m_cmapContinuous = true;
    int m_cmapQuantizationLevels = 8;
    bool m_doMasking = false;
    bool m_volumetric = false;
  };

  struct LandmarkParams
  {
    float strokeWidth = 1.0f;
    glm::vec3 textColor{0.0f};
    bool renderOnTopOfAllImagePlanes = false;
  };

  struct AnnotationParams
  {
    glm::vec3 textColor{0.0f};
    bool renderOnTopOfAllImagePlanes = false;
    bool hidePolygonVertices = false;
  };

  struct SliceIntersectionParams
  {
    float strokeWidth = 1.0f;
    bool renderInactiveImageViewIntersections = true;
    bool renderInactiveImageViewIntersectionsInLightboxViews = false;
  };

  RenderSettings();
  void setXrayEnergy(float energyKeV);

  CrosshairsSnapping m_snapCrosshairs;
  bool m_modulateSegmentationOpacityWithImageOpacity2d;
  bool m_modulateSegmentationOpacityWithImageOpacity3d;
  FloatingPointLinearInterpolationPolicy m_imageGrayFloatingPointInterpolationPolicy;
  FloatingPointLinearInterpolationPolicy m_isocontourFloatingPointInterpolationPolicy;
  bool m_opacityMixMode;
  float m_intensityProjectionSlabThickness;
  bool m_doMaxExtentIntensityProjection;
  float m_xrayIntensityWindow;
  float m_xrayIntensityLevel;
  float m_xrayEnergyKeV;
  float m_waterMassAttenCoeff;
  float m_airMassAttenCoeff;
  glm::vec3 m_2dBackgroundColor;
  glm::vec4 m_3dBackgroundColor;
  bool m_3dTransparentIfNoHit;
  glm::vec4 m_crosshairsColor;
  bool m_showCrosshairs;
  bool m_showCrosshairsInLightboxViews;
  glm::vec4 m_anatomicalLabelColor;
  bool m_showAnatomicalLabels;
  bool m_showAnatomicalLabelsInLightboxViews;
  float m_anatomicalLabelScale = 1.0f;
  AnatomicalLabelType m_anatomicalLabelType = AnatomicalLabelType::Automatic;
  QuadrupedBodyRegion m_quadrupedBodyRegion = QuadrupedBodyRegion::Automatic;
  bool m_showScaleBars;
  bool m_showScaleBarsInLightboxViews;
  glm::vec4 m_scaleBarColor;
  ScaleBarPosition m_scaleBarPosition;
  ScaleBarOrientation m_scaleBarOrientation;
  ScaleBarTicks m_scaleBarTicks;
  float m_scaleBarTargetFraction;
  float m_scaleBarMarginPx;
  bool m_showLightboxOffsetLabels;
  glm::vec4 m_lightboxOffsetLabelColor;
  bool m_renderFrontFaces;
  bool m_renderBackFaces;
  float m_raycastSamplingFactor;
  bool m_useDistanceMapForRaycasting;
  float m_distanceMapForegroundLowerPercentile;
  float m_distanceMapForegroundUpperPercentile;
  bool m_adaptiveRaycastSamplingEnabled;
  float m_adaptiveRaycastTargetFrameRate;
  float m_adaptiveRaycastEffectiveSamplingFactor;
  float m_adaptiveRaycastMeasuredFrameRate;
  bool m_raycastBackgroundEdgeBrighteningEnabled;
  bool m_showImagePlanesIn3D;
  bool m_showSegmentationsOnImagePlanesIn3D;
  bool m_showIsocontoursOnImagePlanesIn3D;
  float m_imagePlaneOpacity;
  bool m_modulateImagePlaneOpacityWithViewAngle;
  bool m_shadeImagePlanesIn3D;
  float m_imagePlaneLightingAmbient;
  float m_imagePlaneLightingDiffuse;
  float m_imagePlaneLightingSpecular;
  float m_imagePlaneLightingSpecularPower;
  float m_lightingAmbient;
  float m_lightingDiffuse;
  float m_lightingSpecular;
  float m_lightingSpecularPower;
  bool m_isosurfaceMeshRenderingEnabled;
  mesh::MeshAdvancedLightingSettings m_meshAdvancedLightingSettings;
  mesh::MeshSurfaceMaterialSettings m_meshSurfaceMaterialSettings;
  bool m_smoothSegmentationMeshes;
  bool m_smoothIsosurfaceMeshes;
  uint32_t m_meshSmoothingIterations;
  float m_meshSmoothingPassBand;
  mesh::MeshDdpSettings m_meshDdpSettings;
  bool m_meshPickingEnabled;
  bool m_meshCutawayEnabled;
  bool m_showCrosshairsIn3D;
  float m_crosshairs3DGlyphDiameterScenePercent;
  float m_crosshairs3DGlyphLengthScenePercent;
  bool m_showThreeDCameraFrustumIn2DViews;
  bool m_reverseThreeDRotateAboutEye;
  bool m_synchronizeThreeDCameras = false;
  glm::vec4 m_threeDCameraFrustumColor;
  std::optional<uuids::uuid> m_lastInteractedThreeDViewUid;
  SegMaskingForRaycasting m_segMasking;
  SegmentationOutlineStyle m_segOutlineStyle = SegmentationOutlineStyle::ViewPixel;
  float m_segInteriorOpacity = 0.2f;
  float m_segInterpCutoff = 0.5f;
  MetricParams m_squaredDifferenceParams;
  MetricParams m_localNccParams;
  MetricParams m_localLinearResidualParams;
  MetricParams m_jointHistogramParams;
  int m_localNccPatchRadius = 3;
  float m_localNccSampleSpacing = 1.0f;
  float m_localNccMinValidFraction = 0.75f;
  float m_localNccVarianceEpsilon = 1.0e-5f;
  bool m_localNccIgnoreNegativeCorrelation = true;
  LocalNccPresentation m_localNccPresentation = LocalNccPresentation::Dissimilarity;
  LocalNccInvalidStyle m_localNccInvalidStyle = LocalNccInvalidStyle::Transparent;
  int m_localLinearResidualPatchRadius = 3;
  float m_localLinearResidualSampleSpacing = 1.0f;
  float m_localLinearResidualMinValidFraction = 0.75f;
  float m_localLinearResidualVarianceEpsilon = 1.0e-5f;
  LocalNccInvalidStyle m_localLinearResidualInvalidStyle = LocalNccInvalidStyle::Transparent;
  int m_numCheckerboardSquares;
  bool m_overlayMagentaCyan;
  glm::ivec2 m_quadrants;
  bool m_useSquare;
  bool m_asciiEnabled = false;
  glm::vec2 m_asciiCellSizePx{8.0f, 16.0f};
  int m_asciiCharsetIndex = 0;
  glm::vec3 m_asciiFgColor{1.0f};
  glm::vec3 m_asciiBgColor{0.0f};
  float m_asciiBgAlpha = 1.0f;
  bool m_asciiUseColormap = false;
  bool m_asciiSpatialMode = false;
  float m_asciiSpatialExponent = 1.0f;
  bool m_asciiAtlasNeedsRebuild = false;
  float m_flashlightRadius;
  bool m_flashlightOverlays;
  bool m_manualFramerateLimiter = false;
  double m_targetFrameTimeSeconds = 1.0 / 60.0;
  LandmarkParams m_globalLandmarkParams;
  AnnotationParams m_globalAnnotationParams;
  SliceIntersectionParams m_globalSliceIntersectionParams;
};

} // namespace rendering
