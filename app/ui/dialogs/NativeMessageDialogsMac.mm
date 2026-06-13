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
    [alert setAlertStyle:NSAlertStyleWarning];
    [alert setMessageText:toNSString(dialog.title)];
    const std::string informativeText = model::combinedInformativeText(dialog.message, dialog.informativeText);
    [alert setInformativeText:toNSString(informativeText)];

    [alert addButtonWithTitle:toNSString(dialog.firstButton)];
    [alert addButtonWithTitle:toNSString(dialog.secondButton)];
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
}  // namespace native_dialog
