#include "logic/serialization/ProjectSerialization.h"
#include "logic/serialization/SerializationHelpers.h"

namespace serialize
{

void to_json(json& j, const serialize::ImageSettings& settings)
{
  const serialize::ImageSettings defaults;

  json componentValues = json::array();
  const std::size_t componentCount = imageComponentSettingsCount(settings);
  for (std::size_t i = 0; i < componentCount; ++i) {
    json component = imageComponentSettingsToJson(settings, i);
    removeDefaultEntries(component, imageComponentDefaultsToJson());
    if (!component.empty()) {
      component["index"] = i;
      componentValues.push_back(std::move(component));
    }
  }

  j = json::object();

  json display = json::object();
  addIfChanged(display, "name", settings.m_displayName, defaults.m_displayName);
  addIfChanged(display, "visible", settings.m_globalVisibility, defaults.m_globalVisibility);
  addIfChanged(display, "opacity", settings.m_globalOpacity, defaults.m_globalOpacity);
  if (settings.m_hasBorderColor) {
    display["borderColor"] = vec3ToJson(settings.m_borderColor);
  }
  addIfNotEmpty(j, "display", std::move(display));

  json transform = json::object();
  addIfChanged(transform, "lockedToReference", settings.m_lockedToReference, defaults.m_lockedToReference);
  addIfChanged(transform, "warpEnabled", settings.m_warpEnabled, defaults.m_warpEnabled);
  addIfChanged(transform, "warpStrength", settings.m_warpStrength, defaults.m_warpStrength);
  addIfChanged(transform, "allowExaggeratedWarp", settings.m_allowExaggeratedWarp, defaults.m_allowExaggeratedWarp);
  addIfNotEmpty(j, "transform", std::move(transform));

  json time = json::object();
  addIfChanged(time, "activePoint", settings.m_activeTimePoint, defaults.m_activeTimePoint);
  addIfChanged(time, "playbackLoop", settings.m_timePlaybackLoop, defaults.m_timePlaybackLoop);
  addIfChanged(time, "playbackPlaying", settings.m_timePlaybackPlaying, defaults.m_timePlaybackPlaying);
  addIfChanged(time, "playbackSpeed", settings.m_timePlaybackSpeed, defaults.m_timePlaybackSpeed);
  addIfNotEmpty(j, "time", std::move(time));

  json components = json::object();
  addIfChanged(components, "active", settings.m_activeComponent, defaults.m_activeComponent);
  if (settings.m_hasComponentRenderMode) {
    components["renderMode"] = enumToName(settings.m_componentRenderMode, k_componentRenderModeNames);
  }
  addIfChanged(
    components,
    "complexPhaseUnit",
    enumToName(settings.m_complexPhaseUnit, k_complexPhaseUnitNames),
    enumToName(defaults.m_complexPhaseUnit, k_complexPhaseUnitNames));
  addIfChanged(
    components,
    "complexPhaseRange",
    enumToName(settings.m_complexPhaseRange, k_complexPhaseRangeNames),
    enumToName(defaults.m_complexPhaseRange, k_complexPhaseRangeNames));
  addIfChanged(components, "ignoreAlpha", settings.m_ignoreAlpha, defaults.m_ignoreAlpha);
  addIfChanged(
    components,
    "colorInterpolationMode",
    enumToName(settings.m_colorInterpolationMode, k_interpolationModeNames),
    enumToName(defaults.m_colorInterpolationMode, k_interpolationModeNames));
  addIfNotEmpty(components, "values", std::move(componentValues));
  addIfNotEmpty(j, "components", std::move(components));

  json vectorRendering = json::object();
  addIfChanged(
    vectorRendering,
    "planarProjectionSignedColors",
    settings.m_vectorPlanarProjectionSignedColors,
    defaults.m_vectorPlanarProjectionSignedColors);
  addIfChanged(
    vectorRendering,
    "logJacobianDeterminant",
    settings.m_vectorLogJacobianDeterminant,
    defaults.m_vectorLogJacobianDeterminant);
  addIfNotEmpty(j, "vectorRendering", std::move(vectorRendering));

  json vectorArrows = json::object();
  addIfChanged(vectorArrows, "visible", settings.m_vectorArrowOverlayVisible, defaults.m_vectorArrowOverlayVisible);
  addIfChanged(vectorArrows, "onImage", settings.m_vectorArrowOverlayOnImage, defaults.m_vectorArrowOverlayOnImage);
  addIfChanged(vectorArrows, "density", settings.m_vectorArrowOverlayDensity, defaults.m_vectorArrowOverlayDensity);
  addIfChanged(
    vectorArrows,
    "voxelSpacing",
    settings.m_vectorArrowOverlayVoxelSpacing,
    defaults.m_vectorArrowOverlayVoxelSpacing);
  addIfChanged(
    vectorArrows,
    "millimeterSpacing",
    settings.m_vectorArrowOverlayMillimeterSpacing,
    defaults.m_vectorArrowOverlayMillimeterSpacing);
  addIfChanged(
    vectorArrows,
    "spacingMode",
    enumToName(settings.m_vectorArrowOverlaySpacingMode, k_vectorArrowOverlaySpacingModeNames),
    enumToName(defaults.m_vectorArrowOverlaySpacingMode, k_vectorArrowOverlaySpacingModeNames));
  addIfChanged(
    vectorArrows,
    "color",
    vec3ToJson(settings.m_vectorArrowOverlayColor),
    vec3ToJson(defaults.m_vectorArrowOverlayColor));
  addIfChanged(
    vectorArrows,
    "useDirectionColor",
    settings.m_vectorArrowOverlayUseDirectionColor,
    defaults.m_vectorArrowOverlayUseDirectionColor);
  addIfChanged(
    vectorArrows,
    "lineThickness",
    settings.m_vectorArrowOverlayLineThickness,
    defaults.m_vectorArrowOverlayLineThickness);
  addIfChanged(vectorArrows, "opacity", settings.m_vectorArrowOverlayOpacity, defaults.m_vectorArrowOverlayOpacity);
  addIfChanged(
    vectorArrows,
    "scaleByMagnitude",
    settings.m_vectorArrowOverlayScaleByMagnitude,
    defaults.m_vectorArrowOverlayScaleByMagnitude);
  addIfChanged(
    vectorArrows,
    "scaleFactor",
    settings.m_vectorArrowOverlayScaleFactor,
    defaults.m_vectorArrowOverlayScaleFactor);
  addIfNotEmpty(j, "vectorArrows", std::move(vectorArrows));

  json warpedGrid = json::object();
  addIfChanged(warpedGrid, "visible", settings.m_vectorWarpedGridVisible, defaults.m_vectorWarpedGridVisible);
  addIfChanged(
    warpedGrid,
    "onImage",
    settings.m_vectorWarpedGridOverlayOnImage,
    defaults.m_vectorWarpedGridOverlayOnImage);
  addIfChanged(
    warpedGrid,
    "convention",
    enumToName(settings.m_vectorWarpedGridConvention, k_vectorWarpedGridConventionNames),
    enumToName(defaults.m_vectorWarpedGridConvention, k_vectorWarpedGridConventionNames));
  addIfChanged(
    warpedGrid,
    "pixelSpacing",
    settings.m_vectorWarpedGridPixelSpacing,
    defaults.m_vectorWarpedGridPixelSpacing);
  addIfChanged(
    warpedGrid,
    "voxelSpacing",
    settings.m_vectorWarpedGridVoxelSpacing,
    defaults.m_vectorWarpedGridVoxelSpacing);
  addIfChanged(
    warpedGrid,
    "millimeterSpacing",
    settings.m_vectorWarpedGridMillimeterSpacing,
    defaults.m_vectorWarpedGridMillimeterSpacing);
  addIfChanged(
    warpedGrid,
    "spacingMode",
    enumToName(settings.m_vectorWarpedGridSpacingMode, k_vectorArrowOverlaySpacingModeNames),
    enumToName(defaults.m_vectorWarpedGridSpacingMode, k_vectorArrowOverlaySpacingModeNames));
  addIfChanged(
    warpedGrid,
    "lineThickness",
    settings.m_vectorWarpedGridLineThickness,
    defaults.m_vectorWarpedGridLineThickness);
  addIfChanged(
    warpedGrid,
    "scaleFactor",
    settings.m_vectorWarpedGridScaleFactor,
    defaults.m_vectorWarpedGridScaleFactor);
  addIfChanged(
    warpedGrid,
    "foregroundColor",
    vec4ToJson(settings.m_vectorWarpedGridForegroundColor),
    vec4ToJson(defaults.m_vectorWarpedGridForegroundColor));
  addIfChanged(
    warpedGrid,
    "backgroundColor",
    vec4ToJson(settings.m_vectorWarpedGridBackgroundColor),
    vec4ToJson(defaults.m_vectorWarpedGridBackgroundColor));
  addIfNotEmpty(j, "warpedGrid", std::move(warpedGrid));

  json edges = json::object();
  addIfChanged(
    edges,
    "method",
    enumToName(settings.m_edgeDetectionMethod, k_edgeDetectionMethodNames),
    enumToName(defaults.m_edgeDetectionMethod, k_edgeDetectionMethodNames));
  addIfChanged(edges, "visible", settings.m_showEdges, defaults.m_showEdges);
  addIfChanged(edges, "hardEdges", settings.m_thresholdEdges, defaults.m_thresholdEdges);
  addIfChanged(edges, "thinPixelEdges", settings.m_thinPixelEdges, defaults.m_thinPixelEdges);
  addIfChanged(edges, "overlay", settings.m_overlayEdges, defaults.m_overlayEdges);
  addIfChanged(edges, "magnitude", settings.m_edgeMagnitude, defaults.m_edgeMagnitude);
  addIfChanged(edges, "pixelScale", settings.m_pixelEdgeScale, defaults.m_pixelEdgeScale);
  addIfChanged(edges, "pixelThreshold", settings.m_pixelEdgeThreshold, defaults.m_pixelEdgeThreshold);
  if (settings.m_hasEdgeColor) {
    edges["color"] = vec3ToJson(settings.m_edgeColor);
  }
  addIfChanged(edges, "opacity", settings.m_edgeOpacity, defaults.m_edgeOpacity);

  if (
    serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod &&
    settings.m_colormapEdges != defaults.m_colormapEdges)
  {
    edges["useColormap"] = settings.m_colormapEdges;
  }
  addIfNotEmpty(j, "edges", std::move(edges));

  json raycasting = json::object();
  addIfChanged(
    raycasting,
    "useDistanceMap",
    settings.m_useDistanceMapForRaycasting,
    defaults.m_useDistanceMapForRaycasting);
  addIfNotEmpty(j, "raycasting", std::move(raycasting));

  json isosurfaces = json::object();
  addIfChanged(isosurfaces, "visible", settings.m_isosurfacesVisible, defaults.m_isosurfacesVisible);
  addIfChanged(
    isosurfaces,
    "applyImageColormap",
    settings.m_applyImageColormapToIsosurfaces,
    defaults.m_applyImageColormapToIsosurfaces);
  addIfChanged(isosurfaces, "showContours2D", settings.m_showIsocontoursIn2D, defaults.m_showIsocontoursIn2D);
  addIfChanged(
    isosurfaces,
    "contourLineWidth2D",
    settings.m_isocontourLineWidthIn2D,
    defaults.m_isocontourLineWidthIn2D);
  addIfChanged(
    isosurfaces,
    "opacityModulator",
    settings.m_isosurfaceOpacityModulator,
    defaults.m_isosurfaceOpacityModulator);
  addIfNotEmpty(j, "isosurfaces", std::move(isosurfaces));
}

void from_json(const json& j, serialize::ImageSettings& settings)
{
  if (const auto display = j.find("display"); display != j.end() && display->is_object()) {
    if (const auto name = display->find("name"); name != display->end() && name->is_string()) {
      settings.m_displayName = name->get<std::string>();
    }
    if (const auto visible = display->find("visible"); visible != display->end() && visible->is_boolean()) {
      settings.m_globalVisibility = visible->get<bool>();
    }
    if (const auto opacity = display->find("opacity"); opacity != display->end() && opacity->is_number()) {
      settings.m_globalOpacity = opacity->get<double>();
    }
    if (const auto color = display->find("borderColor"); color != display->end() && color->is_array()) {
      settings.m_borderColor = vec3FromJson(*color);
      settings.m_hasBorderColor = true;
    }
  }

  if (const auto transform = j.find("transform"); transform != j.end() && transform->is_object()) {
    if (const auto locked = transform->find("lockedToReference"); locked != transform->end() && locked->is_boolean()) {
      settings.m_lockedToReference = locked->get<bool>();
    }
    if (const auto enabled = transform->find("warpEnabled"); enabled != transform->end() && enabled->is_boolean()) {
      settings.m_warpEnabled = enabled->get<bool>();
    }
    if (const auto strength = transform->find("warpStrength"); strength != transform->end() && strength->is_number()) {
      settings.m_warpStrength = strength->get<float>();
    }
    if (const auto exaggerated = transform->find("allowExaggeratedWarp");
        exaggerated != transform->end() && exaggerated->is_boolean())
    {
      settings.m_allowExaggeratedWarp = exaggerated->get<bool>();
    }
  }

  if (const auto time = j.find("time"); time != j.end() && time->is_object()) {
    if (const auto active = unsignedIntFromJson(time->value("activePoint", json{}))) {
      settings.m_activeTimePoint = *active;
    }
    if (const auto loop = time->find("playbackLoop"); loop != time->end() && loop->is_boolean()) {
      settings.m_timePlaybackLoop = loop->get<bool>();
    }
    if (const auto playing = time->find("playbackPlaying"); playing != time->end() && playing->is_boolean()) {
      settings.m_timePlaybackPlaying = playing->get<bool>();
    }
    if (const auto speed = time->find("playbackSpeed"); speed != time->end() && speed->is_number()) {
      settings.m_timePlaybackSpeed = speed->get<double>();
    }
  }

  if (const auto components = j.find("components"); components != j.end() && components->is_object()) {
    if (const auto active = unsignedIntFromJson(components->value("active", json{}))) {
      settings.m_activeComponent = *active;
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectComponentRenderMode>(
        components->value("renderMode", ""),
        k_componentRenderModeNames))
    {
      settings.m_componentRenderMode = *parsed;
      settings.m_hasComponentRenderMode = true;
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectComplexPhaseUnit>(
        components->value("complexPhaseUnit", ""),
        k_complexPhaseUnitNames))
    {
      settings.m_complexPhaseUnit = *parsed;
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectComplexPhaseRange>(
        components->value("complexPhaseRange", ""),
        k_complexPhaseRangeNames))
    {
      settings.m_complexPhaseRange = *parsed;
    }
    if (const auto ignoreAlpha = components->find("ignoreAlpha");
        ignoreAlpha != components->end() && ignoreAlpha->is_boolean())
    {
      settings.m_ignoreAlpha = ignoreAlpha->get<bool>();
    }
    if (
      const auto parsed =
        enumFromName<InterpolationMode>(components->value("colorInterpolationMode", ""), k_interpolationModeNames))
    {
      settings.m_colorInterpolationMode = *parsed;
    }

    if (const auto values = components->find("values"); values != components->end() && values->is_array()) {
      for (std::size_t componentIndex = 0; componentIndex < values->size(); ++componentIndex) {
        const json& value = values->at(componentIndex);
        if (!value.is_object()) {
          continue;
        }
        const std::size_t storedIndex = unsignedIntFromJson(value.value("index", json{})).value_or(componentIndex);
        if (const auto componentValue = value.find("level");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentLevels,
            settings.m_componentLevelIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_level);
        }
        if (const auto componentValue = value.find("window");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentWindows,
            settings.m_componentWindowIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_window);
        }
        if (const auto componentValue = value.find("thresholdLow");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentThresholdLows,
            settings.m_componentThresholdLowIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_thresholdLow);
        }
        if (const auto componentValue = value.find("thresholdHigh");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentThresholdHighs,
            settings.m_componentThresholdHighIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_thresholdHigh);
        }
        if (const auto componentValue = value.find("visible");
            componentValue != value.end() && componentValue->is_boolean())
        {
          setSparseVectorValue(
            settings.m_componentVisibility,
            settings.m_componentVisibilityIndices,
            storedIndex,
            componentValue->get<bool>(),
            true);
        }
        if (const auto componentValue = value.find("opacity");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentOpacities,
            settings.m_componentOpacityIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_opacity);
        }
        if (const auto componentValue = unsignedIntFromJson(value.value("colorMapIndex", json{}))) {
          setSparseVectorValue(
            settings.m_colorMapIndices,
            settings.m_colorMapIndexIndices,
            storedIndex,
            static_cast<std::size_t>(*componentValue),
            std::size_t{0});
        }
        if (const auto componentValue = value.find("colorMapInverted");
            componentValue != value.end() && componentValue->is_boolean())
        {
          setSparseVectorValue(
            settings.m_colorMapInverted,
            settings.m_colorMapInvertedIndices,
            storedIndex,
            componentValue->get<bool>(),
            false);
        }
        if (const auto componentValue = value.find("colorMapContinuous");
            componentValue != value.end() && componentValue->is_boolean())
        {
          setSparseVectorValue(
            settings.m_colorMapContinuous,
            settings.m_colorMapContinuousIndices,
            storedIndex,
            componentValue->get<bool>(),
            true);
        }
        if (const auto componentValue = unsignedIntFromJson(value.value("colorMapLevels", json{}))) {
          setSparseVectorValue(
            settings.m_colorMapLevels,
            settings.m_colorMapLevelIndices,
            storedIndex,
            static_cast<std::size_t>(*componentValue),
            std::size_t{8});
        }
        if (const auto componentValue = value.find("colorMapHsvModifier");
            componentValue != value.end() && componentValue->is_array())
        {
          setSparseVectorValue(
            settings.m_colorMapHsvModifiers,
            settings.m_colorMapHsvModifierIndices,
            storedIndex,
            vec3FromJson(*componentValue),
            glm::vec3{0.0f, 1.0f, 1.0f});
        }
        if (const auto componentValue = value.find("interpolationMode");
            componentValue != value.end() && componentValue->is_string())
        {
          if (
            const auto parsed =
              enumFromName<InterpolationMode>(componentValue->get<std::string>(), k_interpolationModeNames))
          {
            setSparseVectorValue(
              settings.m_interpolationModes,
              settings.m_interpolationModeIndices,
              storedIndex,
              *parsed,
              InterpolationMode::Linear);
          }
        }
        if (const auto componentValue = value.find("foregroundThresholdLow");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_foregroundThresholdLows,
            settings.m_foregroundThresholdLowIndices,
            storedIndex,
            componentValue->get<double>(),
            0.0);
        }
        if (const auto componentValue = value.find("foregroundThresholdHigh");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_foregroundThresholdHighs,
            settings.m_foregroundThresholdHighIndices,
            storedIndex,
            componentValue->get<double>(),
            0.0);
        }
      }

      const std::size_t activeComponent = settings.m_activeComponent;
      if (
        const auto componentValue =
          sparseVectorValue(settings.m_componentLevels, settings.m_componentLevelIndices, activeComponent))
      {
        settings.m_level = *componentValue;
      }
      if (
        const auto componentValue =
          sparseVectorValue(settings.m_componentWindows, settings.m_componentWindowIndices, activeComponent))
      {
        settings.m_window = *componentValue;
      }
      if (
        const auto componentValue = sparseVectorValue(
          settings.m_componentThresholdLows,
          settings.m_componentThresholdLowIndices,
          activeComponent))
      {
        settings.m_thresholdLow = *componentValue;
      }
      if (
        const auto componentValue = sparseVectorValue(
          settings.m_componentThresholdHighs,
          settings.m_componentThresholdHighIndices,
          activeComponent))
      {
        settings.m_thresholdHigh = *componentValue;
      }
      if (
        const auto componentValue =
          sparseVectorValue(settings.m_componentOpacities, settings.m_componentOpacityIndices, activeComponent))
      {
        settings.m_opacity = *componentValue;
      }
    }
  }

  if (const auto vectorRendering = j.find("vectorRendering");
      vectorRendering != j.end() && vectorRendering->is_object())
  {
    if (const auto signedColors = vectorRendering->find("planarProjectionSignedColors");
        signedColors != vectorRendering->end() && signedColors->is_boolean())
    {
      settings.m_vectorPlanarProjectionSignedColors = signedColors->get<bool>();
    }
    if (const auto logJacobian = vectorRendering->find("logJacobianDeterminant");
        logJacobian != vectorRendering->end() && logJacobian->is_boolean())
    {
      settings.m_vectorLogJacobianDeterminant = logJacobian->get<bool>();
    }
  }

  if (const auto arrows = j.find("vectorArrows"); arrows != j.end() && arrows->is_object()) {
    if (const auto visible = arrows->find("visible"); visible != arrows->end() && visible->is_boolean()) {
      settings.m_vectorArrowOverlayVisible = visible->get<bool>();
    }
    if (const auto onImage = arrows->find("onImage"); onImage != arrows->end() && onImage->is_boolean()) {
      settings.m_vectorArrowOverlayOnImage = onImage->get<bool>();
    }
    if (const auto density = arrows->find("density"); density != arrows->end() && density->is_number()) {
      settings.m_vectorArrowOverlayDensity = density->get<float>();
    }
    if (const auto spacing = arrows->find("voxelSpacing"); spacing != arrows->end() && spacing->is_number()) {
      settings.m_vectorArrowOverlayVoxelSpacing = spacing->get<float>();
    }
    if (const auto spacing = arrows->find("millimeterSpacing"); spacing != arrows->end() && spacing->is_number()) {
      settings.m_vectorArrowOverlayMillimeterSpacing = spacing->get<float>();
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectVectorArrowOverlaySpacingMode>(
        arrows->value("spacingMode", ""),
        k_vectorArrowOverlaySpacingModeNames))
    {
      settings.m_vectorArrowOverlaySpacingMode = *parsed;
    }
    if (const auto color = arrows->find("color"); color != arrows->end() && color->is_array()) {
      settings.m_vectorArrowOverlayColor = vec3FromJson(*color);
    }
    if (const auto directionColor = arrows->find("useDirectionColor");
        directionColor != arrows->end() && directionColor->is_boolean())
    {
      settings.m_vectorArrowOverlayUseDirectionColor = directionColor->get<bool>();
    }
    if (const auto thickness = arrows->find("lineThickness"); thickness != arrows->end() && thickness->is_number()) {
      settings.m_vectorArrowOverlayLineThickness = thickness->get<float>();
    }
    if (const auto opacity = arrows->find("opacity"); opacity != arrows->end() && opacity->is_number()) {
      settings.m_vectorArrowOverlayOpacity = opacity->get<float>();
    }
    if (const auto scale = arrows->find("scaleByMagnitude"); scale != arrows->end() && scale->is_boolean()) {
      settings.m_vectorArrowOverlayScaleByMagnitude = scale->get<bool>();
    }
    if (const auto scale = arrows->find("scaleFactor"); scale != arrows->end() && scale->is_number()) {
      settings.m_vectorArrowOverlayScaleFactor = scale->get<float>();
    }
  }

  if (const auto grid = j.find("warpedGrid"); grid != j.end() && grid->is_object()) {
    if (const auto visible = grid->find("visible"); visible != grid->end() && visible->is_boolean()) {
      settings.m_vectorWarpedGridVisible = visible->get<bool>();
    }
    if (const auto onImage = grid->find("onImage"); onImage != grid->end() && onImage->is_boolean()) {
      settings.m_vectorWarpedGridOverlayOnImage = onImage->get<bool>();
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectVectorWarpedGridConvention>(
        grid->value("convention", ""),
        k_vectorWarpedGridConventionNames))
    {
      settings.m_vectorWarpedGridConvention = *parsed;
    }
    if (const auto spacing = grid->find("pixelSpacing"); spacing != grid->end() && spacing->is_number()) {
      settings.m_vectorWarpedGridPixelSpacing = spacing->get<float>();
    }
    if (const auto spacing = grid->find("voxelSpacing"); spacing != grid->end() && spacing->is_number()) {
      settings.m_vectorWarpedGridVoxelSpacing = spacing->get<float>();
    }
    if (const auto spacing = grid->find("millimeterSpacing"); spacing != grid->end() && spacing->is_number()) {
      settings.m_vectorWarpedGridMillimeterSpacing = spacing->get<float>();
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectVectorArrowOverlaySpacingMode>(
        grid->value("spacingMode", ""),
        k_vectorArrowOverlaySpacingModeNames))
    {
      settings.m_vectorWarpedGridSpacingMode = *parsed;
    }
    if (const auto thickness = grid->find("lineThickness"); thickness != grid->end() && thickness->is_number()) {
      settings.m_vectorWarpedGridLineThickness = thickness->get<float>();
    }
    if (const auto scale = grid->find("scaleFactor"); scale != grid->end() && scale->is_number()) {
      settings.m_vectorWarpedGridScaleFactor = scale->get<float>();
    }
    if (const auto color = grid->find("foregroundColor"); color != grid->end() && color->is_array()) {
      settings.m_vectorWarpedGridForegroundColor = vec4FromJson(*color);
    }
    if (const auto color = grid->find("backgroundColor"); color != grid->end() && color->is_array()) {
      settings.m_vectorWarpedGridBackgroundColor = vec4FromJson(*color);
    }
  }

  if (const auto edges = j.find("edges"); edges != j.end() && edges->is_object()) {
    if (
      const auto parsed =
        enumFromName<serialize::ProjectEdgeDetectionMethod>(edges->value("method", ""), k_edgeDetectionMethodNames))
    {
      settings.m_edgeDetectionMethod = *parsed;
    }
    if (const auto visible = edges->find("visible"); visible != edges->end() && visible->is_boolean()) {
      settings.m_showEdges = visible->get<bool>();
    }
    if (const auto hard = edges->find("hardEdges"); hard != edges->end() && hard->is_boolean()) {
      settings.m_thresholdEdges = hard->get<bool>();
    }
    if (const auto thin = edges->find("thinPixelEdges"); thin != edges->end() && thin->is_boolean()) {
      settings.m_thinPixelEdges = thin->get<bool>();
    }
    if (const auto overlay = edges->find("overlay"); overlay != edges->end() && overlay->is_boolean()) {
      settings.m_overlayEdges = overlay->get<bool>();
    }
    if (serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod) {
      if (const auto colormap = edges->find("useColormap"); colormap != edges->end() && colormap->is_boolean()) {
        settings.m_colormapEdges = colormap->get<bool>();
      }
    }
    if (const auto magnitude = edges->find("magnitude"); magnitude != edges->end() && magnitude->is_number()) {
      settings.m_edgeMagnitude = magnitude->get<double>();
    }
    if (const auto scale = edges->find("pixelScale"); scale != edges->end() && scale->is_number()) {
      settings.m_pixelEdgeScale = scale->get<double>();
    }
    if (const auto threshold = edges->find("pixelThreshold"); threshold != edges->end() && threshold->is_number()) {
      settings.m_pixelEdgeThreshold = threshold->get<double>();
    }
    if (const auto color = edges->find("color"); color != edges->end() && color->is_array()) {
      settings.m_edgeColor = vec3FromJson(*color);
      settings.m_hasEdgeColor = true;
    }
    if (const auto opacity = edges->find("opacity"); opacity != edges->end() && opacity->is_number()) {
      settings.m_edgeOpacity = opacity->get<double>();
    }
  }

  if (const auto raycasting = j.find("raycasting"); raycasting != j.end() && raycasting->is_object()) {
    if (const auto useDistanceMap = raycasting->find("useDistanceMap");
        useDistanceMap != raycasting->end() && useDistanceMap->is_boolean())
    {
      settings.m_useDistanceMapForRaycasting = useDistanceMap->get<bool>();
    }
  }

  if (const auto isosurfaces = j.find("isosurfaces"); isosurfaces != j.end() && isosurfaces->is_object()) {
    if (const auto visible = isosurfaces->find("visible"); visible != isosurfaces->end() && visible->is_boolean()) {
      settings.m_isosurfacesVisible = visible->get<bool>();
    }
    if (const auto colormap = isosurfaces->find("applyImageColormap");
        colormap != isosurfaces->end() && colormap->is_boolean())
    {
      settings.m_applyImageColormapToIsosurfaces = colormap->get<bool>();
    }
    if (const auto contours = isosurfaces->find("showContours2D");
        contours != isosurfaces->end() && contours->is_boolean())
    {
      settings.m_showIsocontoursIn2D = contours->get<bool>();
    }
    if (const auto lineWidth = isosurfaces->find("contourLineWidth2D");
        lineWidth != isosurfaces->end() && lineWidth->is_number())
    {
      settings.m_isocontourLineWidthIn2D = lineWidth->get<double>();
    }
    if (const auto opacity = isosurfaces->find("opacityModulator");
        opacity != isosurfaces->end() && opacity->is_number())
    {
      settings.m_isosurfaceOpacityModulator = opacity->get<float>();
    }
  }
}

} // namespace serialize
