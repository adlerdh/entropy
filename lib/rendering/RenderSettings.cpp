#include "rendering/RenderSettings.h"

#include "rendering/physics/XrayAttenuation.h"

namespace rendering
{

RenderSettings::RenderSettings()
  : m_snapCrosshairs(CrosshairsSnapping::Disabled)
  , m_modulateSegmentationOpacityWithImageOpacity2d(true)
  , m_modulateSegmentationOpacityWithImageOpacity3d(true)
  , m_imageGrayFloatingPointInterpolationPolicy(FloatingPointLinearInterpolationPolicy::FixedFunction)
  , m_isocontourFloatingPointInterpolationPolicy(FloatingPointLinearInterpolationPolicy::Automatic)
  , m_opacityMixMode(false)
  , m_intensityProjectionSlabThickness(10.0f)
  , m_doMaxExtentIntensityProjection(true)
  , m_xrayIntensityWindow(1.0f)
  , m_xrayIntensityLevel(0.5f)
  , m_xrayEnergyKeV(xray::defaultEnergyKeV())
  , m_waterMassAttenCoeff(xray::linearAttenuationCoefficients(xray::defaultEnergyKeV()).water_cmInv)
  , m_airMassAttenCoeff(xray::linearAttenuationCoefficients(xray::defaultEnergyKeV()).air_cmInv)
  , m_2dBackgroundColor(0.1f)
  , m_3dBackgroundColor(0.1f, 0.1f, 0.1f, 1.0f)
  , m_3dTransparentIfNoHit(true)
  , m_crosshairsColor(0.05f, 0.6f, 1.0f, 1.0f)
  , m_showCrosshairs(true)
  , m_showCrosshairsInLightboxViews(true)
  , m_anatomicalLabelColor(0.695f, 0.870f, 0.090f, 1.0f)
  , m_showAnatomicalLabels(true)
  , m_showAnatomicalLabelsInLightboxViews(true)
  , m_showScaleBars(true)
  , m_showScaleBarsInLightboxViews(false)
  , m_scaleBarColor(0.380392f, 0.858824f, 0.250980f, 1.0f)
  , m_scaleBarPosition(ScaleBarPosition::BottomRight)
  , m_scaleBarOrientation(ScaleBarOrientation::Horizontal)
  , m_scaleBarTicks(ScaleBarTicks::Automatic)
  , m_scaleBarTargetFraction(0.2f)
  , m_scaleBarMarginPx(12.0f)
  , m_showLightboxOffsetLabels(true)
  , m_lightboxOffsetLabelColor(0.75f, 0.75f, 0.75f, 0.8f)
  , m_renderFrontFaces(true)
  , m_renderBackFaces(true)
  , m_raycastSamplingFactor(0.8f)
  , m_useDistanceMapForRaycasting(true)
  , m_distanceMapForegroundLowerPercentile(0.5f)
  , m_distanceMapForegroundUpperPercentile(1.0f)
  , m_adaptiveRaycastSamplingEnabled(false)
  , m_adaptiveRaycastTargetFrameRate(30.0f)
  , m_adaptiveRaycastEffectiveSamplingFactor(0.8f)
  , m_adaptiveRaycastMeasuredFrameRate(0.0f)
  , m_raycastBackgroundEdgeBrighteningEnabled(false)
  , m_showImagePlanesIn3D(true)
  , m_showSegmentationsOnImagePlanesIn3D(true)
  , m_showIsocontoursOnImagePlanesIn3D(true)
  , m_imagePlaneOpacity(1.0f)
  , m_modulateImagePlaneOpacityWithViewAngle(true)
  , m_shadeImagePlanesIn3D(true)
  , m_imagePlaneLightingAmbient(0.30f)
  , m_imagePlaneLightingDiffuse(0.50f)
  , m_imagePlaneLightingSpecular(0.20f)
  , m_imagePlaneLightingSpecularPower(16.0f)
  , m_lightingAmbient(0.30f)
  , m_lightingDiffuse(0.50f)
  , m_lightingSpecular(0.20f)
  , m_lightingSpecularPower(16.0f)
  , m_isosurfaceMeshRenderingEnabled(true)
  , m_smoothSegmentationMeshes(true)
  , m_smoothIsosurfaceMeshes(true)
  , m_meshSmoothingIterations(25)
  , m_meshSmoothingPassBand(0.1f)
  , m_meshPickingEnabled(true)
  , m_meshClipPlaneEnabled(false)
  , m_meshClipPlaneWorld(1.0f, 0.0f, 0.0f, 0.0f)
  , m_showCrosshairsIn3D(true)
  , m_crosshairs3DGlyphDiameterVoxelDiagonals(1.0f)
  , m_crosshairs3DGlyphLengthVoxelDiagonals(16.0f)
  , m_showThreeDCameraFrustumIn2DViews(false)
  , m_reverseThreeDRotateAboutEye(false)
  , m_threeDCameraFrustumColor(0x7c / 255.0f, 0x5e / 255.0f, 0xd5 / 255.0f, 0xa2 / 255.0f)
  , m_lastInteractedThreeDViewUid(std::nullopt)
  , m_segMasking(SegMaskingForRaycasting::Disabled)
  , m_numCheckerboardSquares(10)
  , m_overlayMagentaCyan(false)
  , m_quadrants(true, true)
  , m_useSquare(true)
  , m_flashlightRadius(0.15f)
  , m_flashlightOverlays(true)
{
}

void RenderSettings::setXrayEnergy(float energyKeV)
{
  const auto coefficients = xray::linearAttenuationCoefficients(energyKeV);
  if (coefficients.water_cmInv <= 0.0f || coefficients.air_cmInv <= 0.0f) {
    return;
  }

  m_xrayEnergyKeV = energyKeV;
  m_airMassAttenCoeff = coefficients.air_cmInv;
  m_waterMassAttenCoeff = coefficients.water_cmInv;
}

} // namespace rendering
