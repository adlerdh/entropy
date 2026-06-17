#include "viewer_types/CameraSyncGroups.h"
#include "viewer_types/FrameImageSelection.h"
#include "viewer_types/FrameViewport.h"
#include "viewer_types/ImageSelection.h"
#include "viewer_types/LayoutTypes.h"
#include "viewer_types/ViewModes.h"
#include "viewer_types/ViewTypes.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("viewer types public headers are self-contained")
{
  CHECK(CameraSyncMode::Rotation == CameraSyncMode::Rotation);
  CHECK(viewer_types::CameraSyncGroups{}.group(CameraSyncMode::Rotation, {}) == nullptr);
  CHECK(viewer_types::FrameImageSelection{}.renderedImages().empty());
  CHECK(viewer_types::FrameViewport{}.windowClipViewport() == glm::vec4{-1.0f, -1.0f, 2.0f, 2.0f});
  CHECK(viewer_types::image_selection::reorderSelectedImages({}, {}).empty());
  CHECK(LayoutKind::Custom == LayoutKind::Custom);
  CHECK(ViewType::Axial == ViewType::Axial);
  CHECK(ViewRenderMode::Image == ViewRenderMode::Image);
  CHECK(IntensityProjectionMode::None == IntensityProjectionMode::None);
}
