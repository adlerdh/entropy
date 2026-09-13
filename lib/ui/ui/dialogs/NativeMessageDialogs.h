#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace native_dialog
{
/**
 * @brief Button selected from a native message dialog.
 */
enum class MessageDialogResult : std::uint8_t
{
  FirstButton,  //!< The first/action button was selected
  SecondButton, //!< The second button was selected
  ThirdButton   //!< The optional third button was selected
};

/** @brief Visual severity used by the native platform dialog. */
enum class MessageDialogSeverity : std::uint8_t
{
  Information,
  Warning,
  Error
};

/**
 * @brief Definition of a native message dialog.
 */
struct MessageDialog
{
  std::string title;                                               //!< Dialog title or primary message
  std::string message;                                             //!< Main explanatory text
  std::string informativeText;                                     //!< Optional additional details
  std::string firstButton;                                         //!< First/action button label
  std::string secondButton;                                        //!< Second button label
  std::string thirdButton;                                         //!< Optional third button label
  MessageDialogSeverity severity = MessageDialogSeverity::Warning; //!< Native warning or error presentation
};

/**
 * @brief Show a native message dialog when the current platform supports one.
 *
 * @param dialog Dialog text and button labels.
 * @return Selected button, or std::nullopt when native message dialogs are unavailable.
 */
std::optional<MessageDialogResult> showMessageDialog(const MessageDialog& dialog);

/** @brief Show a native, one-button error dialog. */
void showErrorMessageDialog(
  const std::string& title,
  const std::string& message,
  const std::string& informativeText = {});
} // namespace native_dialog
