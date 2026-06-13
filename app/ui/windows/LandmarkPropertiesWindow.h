#pragma once

#include "common/PublicTypes.h"

class AppData;

/**
 * @brief Render the landmark group management window.
 * @param appData Application data containing landmark groups and window visibility.
 * @param recenterAllViews Callback used by landmark controls that reposition views.
 */
void renderLandmarkPropertiesWindow(AppData& appData, const AllViewsRecenterType& recenterAllViews);
