#include "rendering/RenderDerivedData.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("render-derived data has complete fixed raycast payload defaults", "[rendering][derived-data]")
{
  const rendering::RenderDerivedData data;
  CHECK(data.isosurfaces.numIsos == 0);
  CHECK(data.isosurfaces.values.size() == 8u);
  CHECK(data.isosurfaces.opacities.size() == 8u);
  CHECK(data.isosurfaces.rimOpacityStrengths.size() == 8u);
  CHECK(data.isosurfaces.rimEmissionStrengths.size() == 8u);
  CHECK(data.isosurfaces.rimPowers.size() == 8u);
  CHECK(data.isosurfaces.colors.size() == 8u);
}

TEST_CASE("render-derived data invalidation is independent of GPU resources", "[rendering][derived-data]")
{
  const uuids::uuid first = uuids::uuid::from_string("11111111-2222-3333-4444-555555555555").value();
  const uuids::uuid second = uuids::uuid::from_string("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee").value();
  rendering::RenderDerivedData data;
  data.imageUniforms.emplace(first, rendering::RenderDerivedData::ImageUniforms{});
  data.imageUniforms.emplace(second, rendering::RenderDerivedData::ImageUniforms{});
  data.isosurfaces.numIsos = 2;

  data.removeImage(first);
  CHECK_FALSE(data.imageUniforms.contains(first));
  CHECK(data.imageUniforms.contains(second));

  data.clear();
  CHECK(data.imageUniforms.empty());
  CHECK(data.isosurfaces.numIsos == 0);
  CHECK(data.isosurfaces.values.size() == 8u);
}
