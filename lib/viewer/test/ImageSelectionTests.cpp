#include "viewer/ImageSelection.h"

#include <catch2/catch_test_macros.hpp>

#include <uuid.h>

#include <cstdint>
#include <list>
#include <vector>

namespace
{

namespace image_selection = viewer::image_selection;

uuids::uuid uuidFromIndex(uint8_t index)
{
  return uuids::uuid{{index, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

std::vector<uuids::uuid> asVector(const std::list<uuids::uuid>& imageUids)
{
  return {imageUids.begin(), imageUids.end()};
}

} // namespace

TEST_CASE("default image filtering keeps all images when render-all is enabled", "[viewer][image_selection]")
{
  const std::list imageUids{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3)};

  const auto filtered = image_selection::filteredDefaultRenderedImages(imageUids, true, {1});

  CHECK(asVector(filtered) == asVector(imageUids));
}

TEST_CASE("default image filtering keeps preferred zero-based image positions", "[viewer][image_selection]")
{
  const std::list imageUids{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3), uuidFromIndex(4)};

  const auto filtered = image_selection::filteredDefaultRenderedImages(imageUids, false, {0, 2});

  CHECK(asVector(filtered) == std::vector{uuidFromIndex(1), uuidFromIndex(3)});
}

TEST_CASE("selected images reorder to match current image order and drop missing UIDs", "[viewer][image_selection]")
{
  const std::list selected{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(4)};
  const std::vector ordered{uuidFromIndex(3), uuidFromIndex(2), uuidFromIndex(1)};

  const auto reordered = image_selection::reorderSelectedImages(selected, ordered);

  CHECK(asVector(reordered) == std::vector{uuidFromIndex(2), uuidFromIndex(1)});
}

TEST_CASE("selected image reordering can limit metric selections", "[viewer][image_selection]")
{
  const std::list selected{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3)};
  const std::vector ordered{uuidFromIndex(3), uuidFromIndex(2), uuidFromIndex(1)};

  const auto reordered = image_selection::reorderSelectedImages(selected, ordered, 2);

  CHECK(asVector(reordered) == std::vector{uuidFromIndex(3), uuidFromIndex(2)});
}
