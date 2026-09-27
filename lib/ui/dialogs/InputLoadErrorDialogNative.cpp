#include "ui/dialogs/InputLoadErrorDialog.h"

namespace native_dialog
{
void showInputLoadErrorDialog(const InputLoadError& error)
{
  static_cast<void>(showMessageDialog(inputLoadErrorDialog(error)));
}
} // namespace native_dialog
