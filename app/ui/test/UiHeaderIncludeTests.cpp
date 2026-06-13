#include "ui/headers/Headers.h"
#include "ui/popups/Popups.h"
#include "ui/toolbars/Toolbars.h"
#include "ui/widgets/Widgets.h"
#include "ui/windows/Windows.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("public UI group headers are self-contained", "[ui][headers]")
{
  SUCCEED("UI group headers compiled successfully");
}
