#pragma once

#include <functional>

class AppData;

/**
 * @brief Modal popup window for adding a new layout
 * @param appData
 * @param openAddLayoutPopup
 * @param recenterViews
 */
void renderAddLayoutModalPopup(
  AppData& appData, bool openAddLayoutPopup, const std::function<void(void)>& recenterViews);

void renderAboutDialogModalPopup(bool open);

void renderConfirmCloseAppPopup(AppData& appData);
