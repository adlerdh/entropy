#include "viewer/FrameImageSelection.h"

#include <catch2/catch_test_macros.hpp>

#include <uuid.h>

#include <cstdint>
#include <list>
#include <vector>

namespace
{

uuids::uuid uuidFromIndex(uint8_t index)
{
  return uuids::uuid{{index, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

std::vector<uuids::uuid> asVector(const std::list<uuids::uuid>& imageUids)
{
  return {imageUids.begin(), imageUids.end()};
}

} // namespace

TEST_CASE("frame image selection filters default rendered images", "[viewer][frame_image_selection]")
{
  const std::list imageUids{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3)};
  viewer::FrameImageSelection selection;

  selection.setDefaultRenderAllImages(false);
  selection.setPreferredDefaultRenderedImages({0, 2});
  selection.setRenderedImages(imageUids, true);

  CHECK(asVector(selection.renderedImages()) == std::vector{uuidFromIndex(1), uuidFromIndex(3)});

  selection.setDefaultRenderAllImages(true);
  selection.setRenderedImages(imageUids, true);

  CHECK(asVector(selection.renderedImages()) == asVector(imageUids));
}

TEST_CASE("frame image selection inserts rendered images in application order", "[viewer][frame_image_selection]")
{
  const std::vector ordered{uuidFromIndex(3), uuidFromIndex(2), uuidFromIndex(1)};
  viewer::FrameImageSelection selection;

  selection.setImageRendered(uuidFromIndex(1), ordered, true);
  selection.setImageRendered(uuidFromIndex(3), ordered, true);
  selection.setImageRendered(uuidFromIndex(2), ordered, true);
  selection.setImageRendered(uuidFromIndex(2), ordered, true);

  CHECK(selection.isImageRendered(uuidFromIndex(2)));
  CHECK(asVector(selection.renderedImages()) == ordered);

  selection.setImageRendered(uuidFromIndex(2), ordered, false);

  CHECK_FALSE(selection.isImageRendered(uuidFromIndex(2)));
  CHECK(asVector(selection.renderedImages()) == std::vector{uuidFromIndex(3), uuidFromIndex(1)});
}

TEST_CASE("frame image selection ignores rendered images outside application order", "[viewer][frame_image_selection]")
{
  const std::vector ordered{uuidFromIndex(1), uuidFromIndex(2)};
  viewer::FrameImageSelection selection;

  selection.setImageRendered(uuidFromIndex(3), ordered, true);

  CHECK(selection.renderedImages().empty());
}

TEST_CASE("frame image selection keeps at most two metric images", "[viewer][frame_image_selection]")
{
  const std::vector ordered{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3)};
  viewer::FrameImageSelection selection;

  selection.setImageUsedForMetric(uuidFromIndex(1), ordered, true);
  selection.setImageUsedForMetric(uuidFromIndex(2), ordered, true);
  selection.setImageUsedForMetric(uuidFromIndex(3), ordered, true);

  CHECK(selection.isImageUsedForMetric(uuidFromIndex(3)));
  CHECK(asVector(selection.metricImages()) == std::vector{uuidFromIndex(1), uuidFromIndex(3)});

  selection.setImageUsedForMetric(uuidFromIndex(1), ordered, false);

  CHECK_FALSE(selection.isImageUsedForMetric(uuidFromIndex(1)));
  CHECK(asVector(selection.metricImages()) == std::vector{uuidFromIndex(3)});
}

TEST_CASE("frame image selection ignores metric images outside application order", "[viewer][frame_image_selection]")
{
  const std::vector ordered{uuidFromIndex(1), uuidFromIndex(2)};
  viewer::FrameImageSelection selection;
  selection.setMetricImages({uuidFromIndex(1), uuidFromIndex(2)});

  selection.setImageUsedForMetric(uuidFromIndex(3), ordered, true);

  CHECK(asVector(selection.metricImages()) == ordered);
}

TEST_CASE("frame image selection exposes visible images by render mode", "[viewer][frame_image_selection]")
{
  viewer::FrameImageSelection selection;
  selection.setRenderedImages({uuidFromIndex(1)}, false);
  selection.setMetricImages({uuidFromIndex(2), uuidFromIndex(3)});

  CHECK(asVector(selection.visibleImages(ViewRenderMode::Image)) == std::vector{uuidFromIndex(1)});
  CHECK(
    asVector(selection.visibleImages(ViewRenderMode::Difference)) == std::vector{uuidFromIndex(2), uuidFromIndex(3)});
  CHECK(selection.visibleImages(ViewRenderMode::Disabled).empty());
}

TEST_CASE("frame image selection reorders selections and drops missing images", "[viewer][frame_image_selection]")
{
  viewer::FrameImageSelection selection;
  selection.setRenderedImages({uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(4)}, false);
  selection.setMetricImages({uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3)});

  selection.updateImageOrdering({uuidFromIndex(3), uuidFromIndex(2), uuidFromIndex(1)});

  CHECK(asVector(selection.renderedImages()) == std::vector{uuidFromIndex(2), uuidFromIndex(1)});
  CHECK(asVector(selection.metricImages()) == std::vector{uuidFromIndex(3), uuidFromIndex(2)});
}
