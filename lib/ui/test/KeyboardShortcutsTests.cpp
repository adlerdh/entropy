#include "ui/windows/KeyboardShortcuts.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

TEST_CASE("Keyboard shortcut catalog has complete rows", "[ui][keyboard-shortcuts]")
{
  const auto& rows = entropy::ui::keyboardShortcutRows();

  REQUIRE_FALSE(rows.empty());
  for (const auto& row : rows) {
    CHECK_FALSE(row.section.empty());
    CHECK_FALSE(row.shortcut.empty());
    CHECK_FALSE(row.action.empty());
    CHECK_FALSE(row.details.empty());
  }
}

TEST_CASE("Keyboard shortcut catalog keeps shortcut action pairs unique", "[ui][keyboard-shortcuts]")
{
  std::set<std::pair<std::string, std::string>> seen;

  for (const auto& row : entropy::ui::keyboardShortcutRows()) {
    CHECK(seen.emplace(row.shortcut, row.action).second);
  }
}

TEST_CASE("Keyboard shortcut descriptions do not end with periods", "[ui][keyboard-shortcuts]")
{
  for (const auto& row : entropy::ui::keyboardShortcutRows()) {
    REQUIRE_FALSE(row.details.empty());
    CHECK(row.details.back() != '.');
  }
}
