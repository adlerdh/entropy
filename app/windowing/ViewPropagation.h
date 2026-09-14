#pragma once

#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"

namespace windowing
{

struct ViewPropagationPolicy
{
  bool renderedImages = false;
  bool threeDImages = false;
  bool metricImages = false;
  bool renderMode = false;
  bool threeDSceneContents = false;
  bool intensityProjectionMode = false;
};

constexpr bool viewTypesHaveMatchingDimensions(ViewType source, ViewType destination)
{
  return (ViewType::ThreeD == source) == (ViewType::ThreeD == destination);
}

constexpr ViewPropagationPolicy visibleImagePropagationPolicy(ViewType source)
{
  return ViewType::ThreeD == source ? ViewPropagationPolicy{.threeDImages = true}
                                    : ViewPropagationPolicy{.renderedImages = true};
}

constexpr ViewPropagationPolicy presentationPropagationPolicy(ViewType source, ViewRenderMode renderMode)
{
  if (ViewType::ThreeD == source) {
    return {.threeDImages = true, .threeDSceneContents = true};
  }

  return {
    .renderedImages = ViewRenderMode::Image == renderMode,
    .metricImages = isComparisonRenderMode(renderMode),
    .renderMode = true,
    .intensityProjectionMode = true};
}

} // namespace windowing
