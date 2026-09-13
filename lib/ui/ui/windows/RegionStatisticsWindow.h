#pragma once

class AppData;
class RegionStatisticsController;

/** Render the dockable per-segmentation region statistics and histogram window. */
void renderRegionStatisticsWindow(AppData& appData, RegionStatisticsController& controller);
