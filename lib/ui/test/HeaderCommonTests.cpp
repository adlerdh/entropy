#include "ui/headers/HeaderCommon.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Image role suffixes show active state for a single image", "[ui][headers]")
{
  CHECK(ui::headers::imageRoleSuffix(true, true, 1) == " (active)");
  CHECK(ui::headers::imageRoleSuffix(false, true, 1) == " (active)");
  CHECK(ui::headers::imageRoleSuffix(true, false, 1).empty());

  CHECK(ui::headers::imageRoleSuffixShortReference(true, true, 1) == "(act.)");
  CHECK(ui::headers::imageRoleSuffixShortReference(false, true, 1) == "(act.)");
  CHECK(ui::headers::imageRoleSuffixShortReference(true, false, 1).empty());
}

TEST_CASE("Image role suffixes show reference and active state for multiple images", "[ui][headers]")
{
  CHECK(ui::headers::imageRoleSuffix(true, true, 2) == " (reference + active)");
  CHECK(ui::headers::imageRoleSuffix(true, false, 2) == " (reference)");
  CHECK(ui::headers::imageRoleSuffix(false, true, 2) == " (active)");
  CHECK(ui::headers::imageRoleSuffix(false, false, 2).empty());

  CHECK(ui::headers::imageRoleSuffixShortReference(true, true, 2) == "(ref. + act.)");
  CHECK(ui::headers::imageRoleSuffixShortReference(true, false, 2) == "(ref.)");
  CHECK(ui::headers::imageRoleSuffixShortReference(false, true, 2) == "(act.)");
  CHECK(ui::headers::imageRoleSuffixShortReference(false, false, 2).empty());
}
