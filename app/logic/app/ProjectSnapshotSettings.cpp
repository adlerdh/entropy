#include "logic/app/ProjectSnapshotSettings.h"

#include "image/Image.h"
#include "image/ImageDerivedData.h"
#include "logic/app/Data.h"

#include <algorithm>

namespace
{
GuiData::LayoutTabPlacement guiLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  return UiLayoutTabPlacement::Bottom == placement ? GuiData::LayoutTabPlacement::Bottom
                                                   : GuiData::LayoutTabPlacement::Top;
}

serialize::ProjectLayoutTabPlacement projectLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  return UiLayoutTabPlacement::Bottom == placement ? serialize::ProjectLayoutTabPlacement::Bottom
                                                   : serialize::ProjectLayoutTabPlacement::Top;
}

UiLayoutTabPlacement uiLayoutTabPlacement(serialize::ProjectLayoutTabPlacement placement)
{
  return serialize::ProjectLayoutTabPlacement::Bottom == placement ? UiLayoutTabPlacement::Bottom
                                                                   : UiLayoutTabPlacement::Top;
}
} // namespace

namespace project_snapshot
{
void syncLayoutTabGuiData(AppData& appData)
{
  appData.guiData().m_showLayoutTabs = appData.settings().showLayoutTabs();
  appData.guiData().m_layoutTabPlacement = guiLayoutTabPlacement(appData.settings().layoutTabPlacement());
}

serialize::ProjectInterfaceSettings interfaceSettings(const AppData& appData)
{
  return serialize::ProjectInterfaceSettings{
    .m_showLayoutTabs = appData.settings().showLayoutTabs(),
    .m_layoutTabPlacement = projectLayoutTabPlacement(appData.settings().layoutTabPlacement()),
    .m_showGlobalTimeControls = appData.settings().showGlobalTimeControls(),
    .m_synchronizeTimeSeries = appData.settings().synchronizeTimeSeries(),
    .m_imageValuePrecision = appData.guiData().m_imageValuePrecision,
    .m_coordsPrecision = appData.guiData().m_coordsPrecision,
    .m_txPrecision = appData.guiData().m_txPrecision,
    .m_percentilePrecision = appData.guiData().m_percentilePrecision};
}

void applyInterfaceSettings(AppData& appData, const serialize::ProjectInterfaceSettings& settings)
{
  appData.settings().setShowLayoutTabs(settings.m_showLayoutTabs);
  appData.settings().setLayoutTabPlacement(uiLayoutTabPlacement(settings.m_layoutTabPlacement));
  appData.settings().setShowGlobalTimeControls(settings.m_showGlobalTimeControls);
  appData.settings().setSynchronizeTimeSeries(settings.m_synchronizeTimeSeries);
  syncLayoutTabGuiData(appData);

  appData.guiData().m_imageValuePrecision = clampPrecision(settings.m_imageValuePrecision);
  appData.guiData().m_imageValuePrecisionFormat = precisionFormat(settings.m_imageValuePrecision);
  appData.guiData().m_coordsPrecision = clampPrecision(settings.m_coordsPrecision);
  appData.guiData().setCoordsPrecisionFormat();
  appData.guiData().m_txPrecision = clampPrecision(settings.m_txPrecision);
  appData.guiData().setTxPrecisionFormat();
  appData.guiData().m_percentilePrecision = clampPrecision(settings.m_percentilePrecision);
  appData.guiData().m_percentilePrecisionFormat = precisionFormat(settings.m_percentilePrecision);
}

bool componentRenderModeIsValidForImage(ComponentRenderMode mode, const Image& image)
{
  const uint32_t numComponents = image.header().numComponentsPerPixel();
  switch (mode) {
    case ComponentRenderMode::SingleComponent:
      return true;
    case ComponentRenderMode::Color:
      return 3 == numComponents || 4 == numComponents;
    case ComponentRenderMode::Minimum:
    case ComponentRenderMode::Mean:
    case ComponentRenderMode::Maximum:
    case ComponentRenderMode::Magnitude:
      return numComponents >= 2;
    case ComponentRenderMode::ComplexPhase:
    case ComponentRenderMode::ComplexReal:
    case ComponentRenderMode::ComplexImaginary:
      return isComplexValuedImage(image);
    case ComponentRenderMode::VectorDirectionColor:
    case ComponentRenderMode::VectorSignedNormalProjection:
    case ComponentRenderMode::VectorPlanarProjectionColor:
    case ComponentRenderMode::VectorJacobianDeterminant:
    case ComponentRenderMode::VectorGradientMagnitude:
    case ComponentRenderMode::VectorDivergence:
    case ComponentRenderMode::VectorCurlMagnitude:
    case ComponentRenderMode::VectorLaplacianMagnitude:
      return isVectorFieldCandidate(image);
  }

  return false;
}

serialize::ImageSettings imageSettings(const Image& image)
{
  const ImageSettings& imageSettings = image.settings();
  const auto thresholds = image.settings().thresholds();

  serialize::ImageSettings settings;
  settings.m_displayName = imageSettings.displayName();
  settings.m_globalVisibility = imageSettings.globalVisibility();
  settings.m_globalOpacity = imageSettings.globalOpacity();
  settings.m_borderColor = imageSettings.borderColor();
  settings.m_lockedToReference = imageSettings.isLockedToReference();
  settings.m_warpEnabled = imageSettings.warpEnabled();
  settings.m_warpStrength = imageSettings.warpStrength();
  settings.m_level = imageSettings.windowCenter();
  settings.m_window = imageSettings.windowWidth();
  settings.m_thresholdLow = thresholds.first;
  settings.m_thresholdHigh = thresholds.second;
  settings.m_opacity = imageSettings.opacity();
  settings.m_activeComponent = imageSettings.activeComponent();
  settings.m_componentRenderMode = toSerializedComponentRenderMode(imageSettings.componentRenderMode());
  settings.m_complexPhaseUnit = toSerializedComplexPhaseUnit(imageSettings.complexPhaseUnit());
  settings.m_complexPhaseRange = toSerializedComplexPhaseRange(imageSettings.complexPhaseRange());
  settings.m_vectorArrowOverlayVisible = imageSettings.vectorArrowOverlayVisible();
  settings.m_vectorArrowOverlayOnImage = imageSettings.vectorArrowOverlayOnImage();
  settings.m_vectorArrowOverlayDensity = imageSettings.vectorArrowOverlayDensity();
  settings.m_vectorArrowOverlayVoxelSpacing = imageSettings.vectorArrowOverlayVoxelSpacing();
  settings.m_vectorArrowOverlayMillimeterSpacing = imageSettings.vectorArrowOverlayMillimeterSpacing();
  settings.m_vectorArrowOverlaySpacingMode =
    toSerializedVectorArrowOverlaySpacingMode(imageSettings.vectorArrowOverlaySpacingMode());
  settings.m_vectorArrowOverlayColor = imageSettings.vectorArrowOverlayColor();
  settings.m_vectorArrowOverlayUseDirectionColor = imageSettings.vectorArrowOverlayUseDirectionColor();
  settings.m_vectorArrowOverlayLineThickness = imageSettings.vectorArrowOverlayLineThickness();
  settings.m_vectorArrowOverlayOpacity = imageSettings.vectorArrowOverlayOpacity();
  settings.m_vectorArrowOverlayScaleByMagnitude = imageSettings.vectorArrowOverlayScaleByMagnitude();
  settings.m_vectorArrowOverlayScaleFactor = imageSettings.vectorArrowOverlayScaleFactor();
  settings.m_vectorWarpedGridVisible = imageSettings.vectorWarpedGridVisible();
  settings.m_vectorWarpedGridOverlayOnImage = imageSettings.vectorWarpedGridOverlayOnImage();
  settings.m_vectorWarpedGridConvention =
    toSerializedVectorWarpedGridConvention(imageSettings.vectorWarpedGridConvention());
  settings.m_vectorWarpedGridPixelSpacing = imageSettings.vectorWarpedGridPixelSpacing();
  settings.m_vectorWarpedGridVoxelSpacing = imageSettings.vectorWarpedGridVoxelSpacing();
  settings.m_vectorWarpedGridMillimeterSpacing = imageSettings.vectorWarpedGridMillimeterSpacing();
  settings.m_vectorWarpedGridSpacingMode =
    toSerializedVectorArrowOverlaySpacingMode(imageSettings.vectorWarpedGridSpacingMode());
  settings.m_vectorWarpedGridLineThickness = imageSettings.vectorWarpedGridLineThickness();
  settings.m_vectorWarpedGridScaleFactor = imageSettings.vectorWarpedGridScaleFactor();
  settings.m_vectorWarpedGridForegroundColor = imageSettings.vectorWarpedGridForegroundColor();
  settings.m_vectorWarpedGridBackgroundColor = imageSettings.vectorWarpedGridBackgroundColor();
  settings.m_vectorPlanarProjectionSignedColors = imageSettings.vectorPlanarProjectionSignedColors();
  settings.m_vectorLogJacobianDeterminant = imageSettings.vectorLogJacobianDeterminant();
  settings.m_ignoreAlpha = imageSettings.ignoreAlpha();
  settings.m_colorInterpolationMode = imageSettings.colorInterpolationMode();
  settings.m_activeTimePoint = imageSettings.activeTimePoint();
  settings.m_timePlaybackLoop = imageSettings.timePlaybackLoop();
  settings.m_timePlaybackPlaying = imageSettings.timePlaybackPlaying();
  settings.m_timePlaybackSpeed = imageSettings.timePlaybackSpeed();
  settings.m_componentLevels.reserve(imageSettings.numComponents());
  settings.m_componentWindows.reserve(imageSettings.numComponents());
  settings.m_componentThresholdLows.reserve(imageSettings.numComponents());
  settings.m_componentThresholdHighs.reserve(imageSettings.numComponents());
  settings.m_componentVisibility.reserve(imageSettings.numComponents());
  settings.m_componentOpacities.reserve(imageSettings.numComponents());
  settings.m_colorMapIndices.reserve(imageSettings.numComponents());
  settings.m_colorMapInverted.reserve(imageSettings.numComponents());
  settings.m_colorMapContinuous.reserve(imageSettings.numComponents());
  settings.m_colorMapLevels.reserve(imageSettings.numComponents());
  settings.m_colorMapHsvModifiers.reserve(imageSettings.numComponents());
  settings.m_interpolationModes.reserve(imageSettings.numComponents());
  settings.m_foregroundThresholdLows.reserve(imageSettings.numComponents());
  settings.m_foregroundThresholdHighs.reserve(imageSettings.numComponents());
  for (uint32_t component = 0; component < imageSettings.numComponents(); ++component) {
    const auto componentThresholds = imageSettings.thresholds(component);
    const auto foregroundThresholds = imageSettings.foregroundThresholds(component);
    settings.m_componentLevels.push_back(imageSettings.windowCenter(component));
    settings.m_componentWindows.push_back(imageSettings.windowWidth(component));
    settings.m_componentThresholdLows.push_back(componentThresholds.first);
    settings.m_componentThresholdHighs.push_back(componentThresholds.second);
    settings.m_componentVisibility.push_back(imageSettings.visibility(component));
    settings.m_componentOpacities.push_back(imageSettings.opacity(component));
    settings.m_colorMapIndices.push_back(imageSettings.colorMapIndex(component));
    settings.m_colorMapInverted.push_back(imageSettings.isColorMapInverted(component));
    settings.m_colorMapContinuous.push_back(imageSettings.colorMapContinuous(component));
    settings.m_colorMapLevels.push_back(imageSettings.colorMapQuantizationLevels(component));
    settings.m_colorMapHsvModifiers.push_back(imageSettings.colorMapHsvModFactors(component));
    settings.m_interpolationModes.push_back(imageSettings.interpolationMode(component));
    settings.m_foregroundThresholdLows.push_back(foregroundThresholds.first);
    settings.m_foregroundThresholdHighs.push_back(foregroundThresholds.second);
  }
  settings.m_edgeDetectionMethod = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                                     ? serialize::ProjectEdgeDetectionMethod::Pixel
                                     : serialize::ProjectEdgeDetectionMethod::Voxel;
  settings.m_showEdges = imageSettings.showAnyEdges();
  settings.m_thresholdEdges = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                                ? imageSettings.thresholdPixelEdges()
                                : imageSettings.thresholdEdges();
  settings.m_thinPixelEdges = imageSettings.thinPixelEdges();
  settings.m_overlayEdges = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                              ? imageSettings.overlayPixelEdges()
                              : imageSettings.overlayEdges();
  settings.m_colormapEdges =
    EdgeDetectionMethod::Voxel == imageSettings.edgeDetectionMethod() && imageSettings.colormapEdges();
  settings.m_edgeMagnitude = imageSettings.edgeMagnitude();
  settings.m_pixelEdgeScale = imageSettings.pixelEdgeScale();
  settings.m_pixelEdgeThreshold = imageSettings.pixelEdgeThreshold();
  settings.m_edgeColor = imageSettings.edgeColor();
  settings.m_edgeOpacity = imageSettings.edgeOpacity();
  settings.m_useDistanceMapForRaycasting = imageSettings.useDistanceMapForRaycasting();
  settings.m_isosurfacesVisible = imageSettings.isosurfacesVisible();
  settings.m_applyImageColormapToIsosurfaces = imageSettings.applyImageColormapToIsosurfaces();
  settings.m_showIsocontoursIn2D = imageSettings.showIsocontoursIn2D();
  settings.m_isocontourLineWidthIn2D = imageSettings.isoContourLineWidthIn2D();
  settings.m_isosurfaceOpacityModulator = imageSettings.isosurfaceOpacityModulator();
  return settings;
}

serialize::SegSettings segmentationSettings(const Image& seg)
{
  serialize::SegSettings settings;
  const ImageSettings& segSettings = seg.settings();
  settings.m_displayName = segSettings.displayName();
  settings.m_visibility = segSettings.visibility();
  settings.m_opacity = seg.settings().opacity();
  settings.m_activeComponent = segSettings.activeComponent();
  settings.m_componentVisibility.reserve(segSettings.numComponents());
  settings.m_componentOpacities.reserve(segSettings.numComponents());
  settings.m_labelTableIndices.reserve(segSettings.numComponents());
  settings.m_interpolationModes.reserve(segSettings.numComponents());
  for (uint32_t component = 0; component < segSettings.numComponents(); ++component) {
    settings.m_componentVisibility.push_back(segSettings.visibility(component));
    settings.m_componentOpacities.push_back(segSettings.opacity(component));
    settings.m_labelTableIndices.push_back(segSettings.labelTableIndex(component));
    settings.m_interpolationModes.push_back(segSettings.interpolationMode(component));
  }
  return settings;
}

void applyImageSettings(Image& image, const serialize::ImageSettings& settings)
{
  ImageSettings& imageSettings = image.settings();
  if (!settings.m_displayName.empty()) {
    imageSettings.setDisplayName(settings.m_displayName);
  }

  imageSettings.setGlobalVisibility(settings.m_globalVisibility);
  imageSettings.setGlobalOpacity(settings.m_globalOpacity);
  imageSettings.setBorderColor(settings.m_borderColor);
  imageSettings.setLockedToReference(settings.m_lockedToReference);
  imageSettings.setWarpEnabled(settings.m_warpEnabled);
  imageSettings.setWarpStrength(settings.m_warpStrength);
  if (settings.m_activeComponent < imageSettings.numComponents()) {
    imageSettings.setActiveComponent(settings.m_activeComponent);
  }
  imageSettings.setActiveTimePoint(image.timeAxis().clamp(settings.m_activeTimePoint));
  imageSettings.setTimePlaybackLoop(settings.m_timePlaybackLoop);
  imageSettings.setTimePlaybackPlaying(settings.m_timePlaybackPlaying && image.isTimeSeries());
  imageSettings.setTimePlaybackSpeed(settings.m_timePlaybackSpeed);
  imageSettings.setWindowCenter(settings.m_level);
  if (settings.m_window > 0.0) {
    imageSettings.setWindowWidth(settings.m_window);
  }
  imageSettings.setThresholdLow(settings.m_thresholdLow);
  imageSettings.setThresholdHigh(settings.m_thresholdHigh);
  imageSettings.setOpacity(settings.m_opacity);
  const ComponentRenderMode componentMode = fromSerializedComponentRenderMode(settings.m_componentRenderMode);
  imageSettings.setComponentRenderMode(
    componentRenderModeIsValidForImage(componentMode, image) ? componentMode : ComponentRenderMode::SingleComponent);
  if (ComponentRenderMode::ComplexReal == imageSettings.componentRenderMode()) {
    imageSettings.setActiveComponent(0);
  }
  else if (ComponentRenderMode::ComplexImaginary == imageSettings.componentRenderMode()) {
    imageSettings.setActiveComponent(1);
  }
  imageSettings.setComplexPhaseUnit(fromSerializedComplexPhaseUnit(settings.m_complexPhaseUnit));
  imageSettings.setComplexPhaseRange(fromSerializedComplexPhaseRange(settings.m_complexPhaseRange));
  imageSettings.setVectorArrowOverlayVisible(settings.m_vectorArrowOverlayVisible);
  imageSettings.setVectorArrowOverlayOnImage(settings.m_vectorArrowOverlayOnImage);
  imageSettings.setVectorArrowOverlayDensity(settings.m_vectorArrowOverlayDensity);
  imageSettings.setVectorArrowOverlayVoxelSpacing(settings.m_vectorArrowOverlayVoxelSpacing);
  imageSettings.setVectorArrowOverlayMillimeterSpacing(settings.m_vectorArrowOverlayMillimeterSpacing);
  imageSettings.setVectorArrowOverlaySpacingMode(
    fromSerializedVectorArrowOverlaySpacingMode(settings.m_vectorArrowOverlaySpacingMode));
  imageSettings.setVectorArrowOverlayColor(settings.m_vectorArrowOverlayColor);
  imageSettings.setVectorArrowOverlayUseDirectionColor(settings.m_vectorArrowOverlayUseDirectionColor);
  imageSettings.setVectorArrowOverlayLineThickness(settings.m_vectorArrowOverlayLineThickness);
  imageSettings.setVectorArrowOverlayOpacity(settings.m_vectorArrowOverlayOpacity);
  imageSettings.setVectorArrowOverlayScaleByMagnitude(settings.m_vectorArrowOverlayScaleByMagnitude);
  imageSettings.setVectorArrowOverlayScaleFactor(settings.m_vectorArrowOverlayScaleFactor);
  imageSettings.setVectorWarpedGridVisible(settings.m_vectorWarpedGridVisible);
  imageSettings.setVectorWarpedGridOverlayOnImage(settings.m_vectorWarpedGridOverlayOnImage);
  imageSettings.setVectorWarpedGridConvention(
    fromSerializedVectorWarpedGridConvention(settings.m_vectorWarpedGridConvention));
  imageSettings.setVectorWarpedGridPixelSpacing(settings.m_vectorWarpedGridPixelSpacing);
  imageSettings.setVectorWarpedGridVoxelSpacing(settings.m_vectorWarpedGridVoxelSpacing);
  imageSettings.setVectorWarpedGridMillimeterSpacing(settings.m_vectorWarpedGridMillimeterSpacing);
  imageSettings.setVectorWarpedGridSpacingMode(
    fromSerializedVectorArrowOverlaySpacingMode(settings.m_vectorWarpedGridSpacingMode));
  imageSettings.setVectorWarpedGridLineThickness(settings.m_vectorWarpedGridLineThickness);
  imageSettings.setVectorWarpedGridScaleFactor(settings.m_vectorWarpedGridScaleFactor);
  imageSettings.setVectorWarpedGridForegroundColor(settings.m_vectorWarpedGridForegroundColor);
  imageSettings.setVectorWarpedGridBackgroundColor(settings.m_vectorWarpedGridBackgroundColor);
  imageSettings.setVectorPlanarProjectionSignedColors(settings.m_vectorPlanarProjectionSignedColors);
  imageSettings.setVectorLogJacobianDeterminant(settings.m_vectorLogJacobianDeterminant);
  imageSettings.setIgnoreAlpha(settings.m_ignoreAlpha);
  imageSettings.setColorInterpolationMode(settings.m_colorInterpolationMode);
  const std::size_t numLevelComponents =
    std::min<std::size_t>(settings.m_componentLevels.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numLevelComponents; ++component) {
    imageSettings.setWindowCenter(static_cast<uint32_t>(component), settings.m_componentLevels.at(component));
  }
  const std::size_t numWindowComponents =
    std::min<std::size_t>(settings.m_componentWindows.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numWindowComponents; ++component) {
    const double windowWidth = settings.m_componentWindows.at(component);
    if (windowWidth > 0.0) {
      imageSettings.setWindowWidth(static_cast<uint32_t>(component), windowWidth);
    }
  }
  const std::size_t numThresholdLowComponents =
    std::min<std::size_t>(settings.m_componentThresholdLows.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numThresholdLowComponents; ++component) {
    imageSettings.setThresholdLow(static_cast<uint32_t>(component), settings.m_componentThresholdLows.at(component));
  }
  const std::size_t numThresholdHighComponents =
    std::min<std::size_t>(settings.m_componentThresholdHighs.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numThresholdHighComponents; ++component) {
    imageSettings.setThresholdHigh(static_cast<uint32_t>(component), settings.m_componentThresholdHighs.at(component));
  }
  const std::size_t numVisibilityComponents =
    std::min<std::size_t>(settings.m_componentVisibility.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numVisibilityComponents; ++component) {
    imageSettings.setVisibility(static_cast<uint32_t>(component), settings.m_componentVisibility.at(component));
  }
  const std::size_t numOpacityComponents =
    std::min<std::size_t>(settings.m_componentOpacities.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numOpacityComponents; ++component) {
    imageSettings.setOpacity(static_cast<uint32_t>(component), settings.m_componentOpacities.at(component));
  }
  const std::size_t numColorMapComponents =
    std::min<std::size_t>(settings.m_colorMapIndices.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numColorMapComponents; ++component) {
    imageSettings.setColorMapIndex(static_cast<uint32_t>(component), settings.m_colorMapIndices.at(component));
  }
  const std::size_t numColorMapInvertedComponents =
    std::min<std::size_t>(settings.m_colorMapInverted.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numColorMapInvertedComponents; ++component) {
    imageSettings.setColorMapInverted(static_cast<uint32_t>(component), settings.m_colorMapInverted.at(component));
  }
  const std::size_t numColorMapContinuousComponents =
    std::min<std::size_t>(settings.m_colorMapContinuous.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numColorMapContinuousComponents; ++component) {
    imageSettings.setColorMapContinuous(static_cast<uint32_t>(component), settings.m_colorMapContinuous.at(component));
  }
  const std::size_t numColorMapLevelComponents =
    std::min<std::size_t>(settings.m_colorMapLevels.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numColorMapLevelComponents; ++component) {
    imageSettings.setColorMapQuantization(
      static_cast<uint32_t>(component),
      static_cast<uint32_t>(settings.m_colorMapLevels.at(component)));
  }
  const std::size_t numColorMapHsvComponents =
    std::min<std::size_t>(settings.m_colorMapHsvModifiers.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numColorMapHsvComponents; ++component) {
    imageSettings.setColormapHsvModfactors(
      static_cast<uint32_t>(component),
      settings.m_colorMapHsvModifiers.at(component));
  }
  const std::size_t numInterpolationComponents =
    std::min<std::size_t>(settings.m_interpolationModes.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numInterpolationComponents; ++component) {
    imageSettings.setInterpolationMode(static_cast<uint32_t>(component), settings.m_interpolationModes.at(component));
  }
  const std::size_t numForegroundLowComponents =
    std::min<std::size_t>(settings.m_foregroundThresholdLows.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numForegroundLowComponents; ++component) {
    imageSettings.setForegroundThresholdLow(
      static_cast<uint32_t>(component),
      settings.m_foregroundThresholdLows.at(component));
  }
  const std::size_t numForegroundHighComponents =
    std::min<std::size_t>(settings.m_foregroundThresholdHighs.size(), imageSettings.numComponents());
  for (std::size_t component = 0; component < numForegroundHighComponents; ++component) {
    imageSettings.setForegroundThresholdHigh(
      static_cast<uint32_t>(component),
      settings.m_foregroundThresholdHighs.at(component));
  }
  imageSettings.setEdgeDetectionMethod(
    serialize::ProjectEdgeDetectionMethod::Pixel == settings.m_edgeDetectionMethod ? EdgeDetectionMethod::Pixel
                                                                                   : EdgeDetectionMethod::Voxel);
  imageSettings.setShowAnyEdges(settings.m_showEdges);
  imageSettings.setThresholdEdges(settings.m_thresholdEdges);
  imageSettings.setThresholdPixelEdges(settings.m_thresholdEdges);
  imageSettings.setThinPixelEdges(settings.m_thinPixelEdges);
  imageSettings.setOverlayEdges(settings.m_overlayEdges);
  imageSettings.setOverlayPixelEdges(settings.m_overlayEdges);
  imageSettings.setColormapEdges(
    serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod && settings.m_colormapEdges);
  imageSettings.setEdgeMagnitude(settings.m_edgeMagnitude);
  imageSettings.setPixelEdgeScale(settings.m_pixelEdgeScale);
  imageSettings.setPixelEdgeThreshold(settings.m_pixelEdgeThreshold);
  imageSettings.setEdgeColor(settings.m_edgeColor);
  imageSettings.setEdgeOpacity(settings.m_edgeOpacity);
  imageSettings.setUseDistanceMapForRaycasting(settings.m_useDistanceMapForRaycasting);
  imageSettings.setIsosurfacesVisible(settings.m_isosurfacesVisible);
  imageSettings.setApplyImageColormapToIsosurfaces(settings.m_applyImageColormapToIsosurfaces);
  imageSettings.setShowIsoscontoursIn2D(settings.m_showIsocontoursIn2D);
  imageSettings.setIsosurfaceWidthIn2d(settings.m_isocontourLineWidthIn2D);
  imageSettings.setIsosurfaceOpacityModulator(settings.m_isosurfaceOpacityModulator);
}

void applySegmentationSettings(Image& seg, const serialize::SegSettings& settings)
{
  ImageSettings& segSettings = seg.settings();
  if (!settings.m_displayName.empty()) {
    segSettings.setDisplayName(settings.m_displayName);
  }
  segSettings.setVisibility(settings.m_visibility);
  segSettings.setOpacity(settings.m_opacity);
  if (settings.m_activeComponent < segSettings.numComponents()) {
    segSettings.setActiveComponent(settings.m_activeComponent);
  }
  const std::size_t numVisibilityComponents =
    std::min<std::size_t>(settings.m_componentVisibility.size(), segSettings.numComponents());
  for (std::size_t component = 0; component < numVisibilityComponents; ++component) {
    segSettings.setVisibility(static_cast<uint32_t>(component), settings.m_componentVisibility.at(component));
  }
  const std::size_t numOpacityComponents =
    std::min<std::size_t>(settings.m_componentOpacities.size(), segSettings.numComponents());
  for (std::size_t component = 0; component < numOpacityComponents; ++component) {
    segSettings.setOpacity(static_cast<uint32_t>(component), settings.m_componentOpacities.at(component));
  }
  const std::size_t numLabelTableComponents =
    std::min<std::size_t>(settings.m_labelTableIndices.size(), segSettings.numComponents());
  for (std::size_t component = 0; component < numLabelTableComponents; ++component) {
    segSettings.setLabelTableIndex(static_cast<uint32_t>(component), settings.m_labelTableIndices.at(component));
  }
  const std::size_t numInterpolationComponents =
    std::min<std::size_t>(settings.m_interpolationModes.size(), segSettings.numComponents());
  for (std::size_t component = 0; component < numInterpolationComponents; ++component) {
    segSettings.setInterpolationMode(static_cast<uint32_t>(component), settings.m_interpolationModes.at(component));
  }
}
} // namespace project_snapshot
