#include "ui/dialogs/NativeMessageDialogs.h"
#include "ui/dialogs/NativeMessageDialogModel.h"

#if defined(__linux__)
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

extern char** environ;
#endif

namespace
{
#if defined(__linux__)
std::optional<int> runDialogCommand(std::vector<char*>& argv)
{
  posix_spawn_file_actions_t actions;
  if (0 != posix_spawn_file_actions_init(&actions)) {
    return std::nullopt;
  }

  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

  pid_t pid = 0;
  const int spawnResult = posix_spawnp(&pid, argv.front(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (0 != spawnResult) {
    return std::nullopt;
  }

  int status = 0;
  if (pid != waitpid(pid, &status, 0) || !WIFEXITED(status)) {
    return std::nullopt;
  }
  return WEXITSTATUS(status);
}

std::vector<char*> argvFor(std::vector<std::string>& args)
{
  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  return argv;
}

std::optional<native_dialog::MessageDialogResult> showZenityDialog(
  const native_dialog::MessageDialog& dialog,
  const std::string& text)
{
  std::vector<std::string> args{"zenity"};
  const bool acknowledgementOnly = dialog.secondButton.empty();
  const char* acknowledgementDialog = "--warning";
  if (dialog.severity == native_dialog::MessageDialogSeverity::Information) acknowledgementDialog = "--info";
  if (dialog.severity == native_dialog::MessageDialogSeverity::Error) acknowledgementDialog = "--error";
  args.push_back(acknowledgementOnly ? acknowledgementDialog : "--question");
  args.insert(
    args.end(),
    {"--modal", "--width=440", "--title", dialog.title, "--text", text, "--ok-label", dialog.firstButton});
  if (!acknowledgementOnly) {
    args.insert(args.end(), {"--cancel-label", dialog.secondButton});
  }
  std::vector<char*> argv = argvFor(args);
  const auto exitCode = runDialogCommand(argv);
  if (!exitCode) {
    return std::nullopt;
  }
  if (0 == *exitCode) {
    return native_dialog::MessageDialogResult::FirstButton;
  }
  if (1 == *exitCode) {
    return native_dialog::MessageDialogResult::SecondButton;
  }
  return std::nullopt;
}

std::optional<native_dialog::MessageDialogResult> showKdialogDialog(
  const native_dialog::MessageDialog& dialog,
  const std::string& text)
{
  const bool acknowledgementOnly = dialog.secondButton.empty();
  const char* acknowledgementDialog = "--sorry";
  if (dialog.severity == native_dialog::MessageDialogSeverity::Information) acknowledgementDialog = "--msgbox";
  if (dialog.severity == native_dialog::MessageDialogSeverity::Error) acknowledgementDialog = "--error";
  const std::string dialogType = acknowledgementOnly ? acknowledgementDialog : "--warningyesno";
  std::vector<std::string> args{"kdialog", dialogType, text, "--title", dialog.title};
  if (!acknowledgementOnly) {
    args.insert(args.end(), {"--yes-label", dialog.firstButton, "--no-label", dialog.secondButton});
  }
  std::vector<char*> argv = argvFor(args);
  const auto exitCode = runDialogCommand(argv);
  if (!exitCode) {
    return std::nullopt;
  }
  if (0 == *exitCode) {
    return native_dialog::MessageDialogResult::FirstButton;
  }
  if (1 == *exitCode) {
    return native_dialog::MessageDialogResult::SecondButton;
  }
  return std::nullopt;
}
#endif
} // namespace

namespace native_dialog
{
std::optional<MessageDialogResult> showMessageDialog(const MessageDialog& dialog)
{
#if defined(__linux__)
  if (!dialog.thirdButton.empty()) {
    return std::nullopt;
  }

  const std::string text = model::fallbackMessageText(dialog);
  if (const auto result = showZenityDialog(dialog, text)) {
    return result;
  }
  return showKdialogDialog(dialog, text);
#else
  (void)dialog;
  return std::nullopt;
#endif
}

void showErrorMessageDialog(const std::string& title, const std::string& message, const std::string& informativeText)
{
  static_cast<void>(showMessageDialog(
    {.title = title,
     .message = message,
     .informativeText = informativeText,
     .firstButton = "OK",
     .secondButton = "",
     .thirdButton = "",
     .severity = MessageDialogSeverity::Error}));
}
} // namespace native_dialog
