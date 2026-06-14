#include "ui/UiControls.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("UI controls default to hidden optional controls", "[ui][controls]")
{
  const UiControls controls;

  CHECK_FALSE(controls.m_hasImageComboBox);
  CHECK_FALSE(controls.m_hasViewTypeComboBox);
  CHECK_FALSE(controls.m_hasShaderTypeComboBox);
  CHECK_FALSE(controls.m_hasMipTypeComboBox);
}

TEST_CASE("UI controls visible constructor toggles every optional control", "[ui][controls]")
{
  const UiControls visible{true};
  CHECK(visible.m_hasImageComboBox);
  CHECK(visible.m_hasViewTypeComboBox);
  CHECK(visible.m_hasShaderTypeComboBox);
  CHECK(visible.m_hasMipTypeComboBox);

  const UiControls hidden{false};
  CHECK_FALSE(hidden.m_hasImageComboBox);
  CHECK_FALSE(hidden.m_hasViewTypeComboBox);
  CHECK_FALSE(hidden.m_hasShaderTypeComboBox);
  CHECK_FALSE(hidden.m_hasMipTypeComboBox);
}
