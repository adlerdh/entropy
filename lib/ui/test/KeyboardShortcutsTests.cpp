#include "ui/windows/KeyboardShortcuts.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

namespace
{
void checkRowsAreComplete(const std::vector<ui::KeyboardShortcutRow>& rows)
{
  REQUIRE_FALSE(rows.empty());
  for (const auto& row : rows) {
    CHECK_FALSE(row.section.empty());
    CHECK_FALSE(row.shortcut.empty());
    CHECK_FALSE(row.action.empty());
    CHECK_FALSE(row.details.empty());
  }
}

void checkShortcutActionPairsAreUnique(const std::vector<ui::KeyboardShortcutRow>& rows)
{
  std::set<std::pair<std::string, std::string>> seen;

  for (const auto& row : rows) {
    CHECK(seen.emplace(row.shortcut, row.action).second);
  }
}

void checkDescriptionsDoNotEndWithPeriods(const std::vector<ui::KeyboardShortcutRow>& rows)
{
  for (const auto& row : rows) {
    REQUIRE_FALSE(row.details.empty());
    CHECK(row.details.back() != '.');
  }
}
} // namespace

TEST_CASE("Keyboard shortcut catalog has complete rows", "[ui][keyboard-shortcuts]")
{
  checkRowsAreComplete(ui::keyboardShortcutRows());
  checkRowsAreComplete(ui::threeDViewControlRows());
}

TEST_CASE("Keyboard shortcut catalog keeps shortcut action pairs unique", "[ui][keyboard-shortcuts]")
{
  checkShortcutActionPairsAreUnique(ui::keyboardShortcutRows());
  checkShortcutActionPairsAreUnique(ui::threeDViewControlRows());
}

TEST_CASE("Keyboard shortcut descriptions do not end with periods", "[ui][keyboard-shortcuts]")
{
  checkDescriptionsDoNotEndWithPeriods(ui::keyboardShortcutRows());
  checkDescriptionsDoNotEndWithPeriods(ui::threeDViewControlRows());
}
