#include "viewer/CameraSyncGroups.h"
#include "viewer/FrameHitGeometry.h"
#include "viewer/FrameImageSelection.h"
#include "viewer/FrameViewport.h"
#include "viewer/ImageSelection.h"
#include "viewer/LayoutTypes.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("viewer public headers are self-contained")
{
  CHECK(CameraSyncMode::Rotation == CameraSyncMode::Rotation);
  CHECK(viewer::CameraSyncGroups{}.group(CameraSyncMode::Rotation, {}) == nullptr);
  CHECK(viewer::FrameHitGeometry{}.m_worldPos == glm::vec4{0.0f});
  CHECK(viewer::FrameImageSelection{}.renderedImages().empty());
  CHECK(viewer::FrameViewport{}.windowClipViewport() == glm::vec4{-1.0f, -1.0f, 2.0f, 2.0f});
  CHECK(viewer::image_selection::reorderSelectedImages({}, {}).empty());
  CHECK(LayoutKind::Custom == LayoutKind::Custom);
  CHECK(ViewType::Axial == ViewType::Axial);
  CHECK(ViewRenderMode::Image == ViewRenderMode::Image);
  CHECK(IntensityProjectionMode::None == IntensityProjectionMode::None);
}
