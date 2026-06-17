#include "viewer_types/LayoutTypes.h"
#include "viewer_types/ViewModes.h"
#include "viewer_types/ViewTypes.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("viewer types public headers are self-contained")
{
  CHECK(CameraSyncMode::Rotation == CameraSyncMode::Rotation);
  CHECK(LayoutKind::Custom == LayoutKind::Custom);
  CHECK(ViewType::Axial == ViewType::Axial);
  CHECK(ViewRenderMode::Image == ViewRenderMode::Image);
  CHECK(IntensityProjectionMode::None == IntensityProjectionMode::None);
}
