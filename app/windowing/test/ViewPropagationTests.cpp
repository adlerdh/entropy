#include "windowing/ViewPropagation.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("view propagation only targets views with matching dimensions", "[windowing][view-propagation]")
{
  CHECK(windowing::viewTypesHaveMatchingDimensions(ViewType::Axial, ViewType::Coronal));
  CHECK(windowing::viewTypesHaveMatchingDimensions(ViewType::Oblique, ViewType::Sagittal));
  CHECK(windowing::viewTypesHaveMatchingDimensions(ViewType::ThreeD, ViewType::ThreeD));
  CHECK_FALSE(windowing::viewTypesHaveMatchingDimensions(ViewType::Axial, ViewType::ThreeD));
  CHECK_FALSE(windowing::viewTypesHaveMatchingDimensions(ViewType::ThreeD, ViewType::Coronal));
}

TEST_CASE("visible image propagation selects only dimensionality-relevant state", "[windowing][view-propagation]")
{
  const auto twoD = windowing::visibleImagePropagationPolicy(ViewType::Axial);
  CHECK(twoD.renderedImages);
  CHECK_FALSE(twoD.threeDImages);
  CHECK_FALSE(twoD.metricImages);

  const auto threeD = windowing::visibleImagePropagationPolicy(ViewType::ThreeD);
  CHECK_FALSE(threeD.renderedImages);
  CHECK(threeD.threeDImages);
  CHECK_FALSE(threeD.metricImages);
}

TEST_CASE("2D presentation propagation selects the active mode's state", "[windowing][view-propagation]")
{
  const auto images = windowing::presentationPropagationPolicy(ViewType::Axial, ViewRenderMode::Image);
  CHECK(images.renderedImages);
  CHECK_FALSE(images.metricImages);
  CHECK(images.renderMode);
  CHECK(images.intensityProjectionMode);
  CHECK_FALSE(images.threeDImages);
  CHECK_FALSE(images.threeDSceneContents);

  const auto comparison = windowing::presentationPropagationPolicy(ViewType::Coronal, ViewRenderMode::Difference);
  CHECK_FALSE(comparison.renderedImages);
  CHECK(comparison.metricImages);
  CHECK(comparison.renderMode);
  CHECK(comparison.intensityProjectionMode);
  CHECK_FALSE(comparison.threeDImages);
  CHECK_FALSE(comparison.threeDSceneContents);
}

TEST_CASE("3D presentation propagation selects only 3D state", "[windowing][view-propagation]")
{
  const auto policy = windowing::presentationPropagationPolicy(ViewType::ThreeD, ViewRenderMode::Difference);
  CHECK(policy.threeDImages);
  CHECK(policy.threeDSceneContents);
  CHECK_FALSE(policy.renderedImages);
  CHECK_FALSE(policy.metricImages);
  CHECK_FALSE(policy.renderMode);
  CHECK_FALSE(policy.intensityProjectionMode);
}
