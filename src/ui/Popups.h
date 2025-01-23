#ifndef UI_POPUPS_H
#define UI_POPUPS_H

#include <functional>

class AppData;

/**
 * @brief Modal popup window for adding a new layout
 * @param appData
 * @param openAddLayoutPopup
 * @param recenterViews
 */
void renderAddLayoutModalPopup(
  AppData& appData, bool openAddLayoutPopup, const std::function<void(void)>& recenterViews
);

void renderAboutDialogModalPopup(bool open);

void renderConfirmCloseAppPopup(AppData& appData);

#endif // UI_POPUPS
