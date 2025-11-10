#pragma once

/**
 * @brief Settings for UI controls for a view or lightbox layout
 */
struct UiControls
{
  UiControls() = default;

  explicit UiControls(bool visible)
    : m_hasImageComboBox(visible)
    , m_hasViewTypeComboBox(visible)
    , m_hasShaderTypeComboBox(visible)
    , m_hasMipTypeComboBox(visible)
  {
  }

  bool m_hasImageComboBox = false;
  bool m_hasViewTypeComboBox = false;
  bool m_hasShaderTypeComboBox = false;
  bool m_hasMipTypeComboBox = false;
};
