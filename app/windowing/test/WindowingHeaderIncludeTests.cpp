#include "windowing/GlfwCallbacks.h"
#include "windowing/GlfwWrapper.h"
#include "windowing/Layout.h"
#include "windowing/LayoutFileSerialization.h"
#include "windowing/LayoutPreset.h"
#include "windowing/LayoutSpec.h"
#include "windowing/LayoutSpecJson.h"
#include "windowing/View.h"
#include "windowing/ViewTypes.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("windowing public headers are self-contained", "[windowing][headers]")
{
  CHECK(LayoutKind::Custom == LayoutKind::Custom);
  CHECK(CameraSyncMode::Zoom == CameraSyncMode::Zoom);
  CHECK(EventProcessingMode::Wait == EventProcessingMode::Wait);
}
