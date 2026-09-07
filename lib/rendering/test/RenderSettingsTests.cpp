#include "rendering/RenderSettings.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("intensity projections use the full volume extent by default", "[rendering][intensity-projection]")
{
  const rendering::RenderSettings settings;

  CHECK(settings.m_doMaxExtentIntensityProjection);
  CHECK(settings.m_intensityProjectionSlabThickness == 10.0f);
}

TEST_CASE("3D image planes show segmentation and isocontour overlays by default", "[rendering][image-plane]")
{
  const rendering::RenderSettings settings;

  CHECK(settings.m_showImagePlanesIn3D);
  CHECK(settings.m_showSegmentationsOnImagePlanesIn3D);
  CHECK(settings.m_showIsocontoursOnImagePlanesIn3D);
}
