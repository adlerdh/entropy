#include "ui/dialogs/NativeMessageDialogs.h"
#include "ui/dialogs/NativeMessageDialogModel.h"

#import <AppKit/AppKit.h>

namespace {
/**
 * @brief Convert UTF-8 text to an NSString for AppKit APIs.
 *
 * @param text UTF-8 encoded text.
 * @return Objective-C string view of the supplied text.
 */
NSString* toNSString(const std::string& text) {
  return [NSString stringWithUTF8String:text.c_str()];
}
}  // namespace

namespace native_dialog {
std::optional<MessageDialogResult> showMessageDialog(const MessageDialog& dialog) {
  @autoreleasepool {
    NSAlert* alert = [[NSAlert alloc] init];
    switch (dialog.severity) {
      case MessageDialogSeverity::Information:
        [alert setAlertStyle:NSAlertStyleInformational];
        break;
      case MessageDialogSeverity::Warning:
        [alert setAlertStyle:NSAlertStyleWarning];
        break;
      case MessageDialogSeverity::Error:
        [alert setAlertStyle:NSAlertStyleCritical];
        break;
    }
    [alert setMessageText:toNSString(dialog.title)];
    const std::string informativeText = model::combinedInformativeText(dialog.message, dialog.informativeText);
    [alert setInformativeText:toNSString(informativeText)];

    [alert addButtonWithTitle:toNSString(dialog.firstButton)];
    if (!dialog.secondButton.empty()) {
      [alert addButtonWithTitle:toNSString(dialog.secondButton)];
    }
    if (!dialog.thirdButton.empty()) {
      [alert addButtonWithTitle:toNSString(dialog.thirdButton)];
    }

    const NSModalResponse response = [alert runModal];
    if (NSAlertFirstButtonReturn == response) {
      return MessageDialogResult::FirstButton;
    }
    if (NSAlertSecondButtonReturn == response) {
      return MessageDialogResult::SecondButton;
    }
    if (NSAlertThirdButtonReturn == response) {
      return MessageDialogResult::ThirdButton;
    }
  }

  return std::nullopt;
}

void showErrorMessageDialog(const std::string& title, const std::string& message, const std::string& informativeText) {
  static_cast<void>(showMessageDialog({title, message, informativeText, "OK", "", "", MessageDialogSeverity::Error}));
}
}  // namespace native_dialog
