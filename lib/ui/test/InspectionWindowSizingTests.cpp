#include "ui/windows/InspectionWindowSizing.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Inspector fit height preserves a desired height within its limits")
{
  CHECK(ui::fittedInspectionWindowHeight(240.0f, 1000.0f, 120.0f, 480.0f) == 240.0f);
}

TEST_CASE("Inspector fit height respects minimum and absolute maximum heights")
{
  CHECK(ui::fittedInspectionWindowHeight(80.0f, 2000.0f, 120.0f, 480.0f) == 120.0f);
  CHECK(ui::fittedInspectionWindowHeight(800.0f, 2000.0f, 120.0f, 480.0f) == 480.0f);
}

TEST_CASE("Inspector fit height cannot consume more than 45 percent of the viewport")
{
  CHECK(ui::fittedInspectionWindowHeight(800.0f, 600.0f, 120.0f, 480.0f) == 270.0f);
}
