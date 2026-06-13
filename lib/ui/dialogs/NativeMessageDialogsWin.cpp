#include "ui/dialogs/NativeMessageDialogs.h"
#include "ui/dialogs/NativeMessageDialogModel.h"

#ifndef WINVER
#define WINVER 0x0600
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

#include <array>
#include <vector>

namespace
{
constexpr int sk_firstButtonId = 1000;
constexpr int sk_secondButtonId = 1001;
constexpr int sk_thirdButtonId = 1002;

/**
 * @brief Convert UTF-8 text to a Windows wide string.
 *
 * @param text UTF-8 encoded text.
 * @return UTF-16 text suitable for Win32 APIs.
 */
std::wstring toWideString(const std::string& text)
{
  if (text.empty()) {
    return {};
  }

  const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (size <= 0) {
    return {};
  }

  std::wstring wideText(static_cast<std::size_t>(size - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(), size);
  return wideText;
}

/**
 * @brief Map a Task Dialog button identifier back to the public result enum.
 *
 * @param buttonId Win32 button ID returned by TaskDialogIndirect.
 * @return Matching dialog result, or std::nullopt for unknown IDs.
 */
std::optional<native_dialog::MessageDialogResult> resultFromButtonId(int buttonId)
{
  switch (buttonId) {
    case sk_firstButtonId:
      return native_dialog::MessageDialogResult::FirstButton;
    case sk_secondButtonId:
      return native_dialog::MessageDialogResult::SecondButton;
    case sk_thirdButtonId:
      return native_dialog::MessageDialogResult::ThirdButton;
    default:
      return std::nullopt;
  }
}

/**
 * @brief Select MessageBoxW button flags for the fallback dialog.
 *
 * @param numButtons Number of requested buttons.
 * @return Win32 MessageBoxW button flag.
 */
UINT messageBoxButtons(std::size_t numButtons)
{
  return numButtons >= 3 ? MB_YESNOCANCEL : MB_OKCANCEL;
}

/**
 * @brief Show a simpler MessageBoxW if the richer Task Dialog API fails.
 *
 * @param title Dialog title.
 * @param text Combined message text.
 * @param numButtons Number of requested buttons.
 * @return Selected button, or std::nullopt if the API returns an unexpected result.
 */
std::optional<native_dialog::MessageDialogResult>
showFallbackMessageBox(const std::wstring& title, const std::wstring& text, std::size_t numButtons)
{
  const int result = MessageBoxW(nullptr, text.c_str(), title.c_str(), MB_ICONWARNING | messageBoxButtons(numButtons));
  if (numButtons >= 3) {
    if (IDYES == result) return native_dialog::MessageDialogResult::FirstButton;
    if (IDNO == result) return native_dialog::MessageDialogResult::SecondButton;
    if (IDCANCEL == result) return native_dialog::MessageDialogResult::ThirdButton;
  }

  if (IDOK == result) return native_dialog::MessageDialogResult::FirstButton;
  if (IDCANCEL == result) return native_dialog::MessageDialogResult::SecondButton;
  return std::nullopt;
}
} // namespace

namespace native_dialog
{
std::optional<MessageDialogResult> showMessageDialog(const MessageDialog& dialog)
{
  const std::wstring title = toWideString(dialog.title);
  const std::wstring mainInstruction = toWideString(dialog.message);
  const std::wstring content = toWideString(dialog.informativeText);

  const std::vector<std::string> narrowButtonLabels = model::buttonLabels(dialog);
  std::vector<std::wstring> buttonLabels;
  buttonLabels.reserve(3);
  for (const auto& label : narrowButtonLabels) {
    buttonLabels.push_back(toWideString(label));
  }

  const std::array<int, 3> buttonIds{sk_firstButtonId, sk_secondButtonId, sk_thirdButtonId};
  std::vector<TASKDIALOG_BUTTON> buttons;
  buttons.reserve(buttonLabels.size());
  for (std::size_t i = 0; i < buttonLabels.size(); ++i) {
    buttons.push_back({buttonIds.at(i), buttonLabels.at(i).c_str()});
  }

  TASKDIALOGCONFIG config{};
  config.cbSize = sizeof(config);
  config.dwFlags = TDF_SIZE_TO_CONTENT;
  config.pszWindowTitle = title.c_str();
  config.pszMainInstruction = mainInstruction.c_str();
  config.pszContent = content.empty() ? nullptr : content.c_str();
  config.pszMainIcon = TD_WARNING_ICON;
  config.cButtons = static_cast<UINT>(buttons.size());
  config.pButtons = buttons.data();
  config.nDefaultButton = sk_secondButtonId;

  int selectedButton = 0;
  const HRESULT result = TaskDialogIndirect(&config, &selectedButton, nullptr, nullptr);
  if (SUCCEEDED(result)) {
    return resultFromButtonId(selectedButton);
  }

  return showFallbackMessageBox(title, toWideString(model::fallbackMessageText(dialog)), buttonLabels.size());
}
} // namespace native_dialog
