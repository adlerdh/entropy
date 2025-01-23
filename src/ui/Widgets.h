#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "common/PublicTypes.h"

#include <functional>
#include <glm/fwd.hpp>
#include <utility>
#include <uuid.h>

class AppData;
class ImageColorMap;
class ImageTransformations;
class LandmarkGroup;
class ParcellationLabelTable;

void renderActiveImageSelectionCombo(
  size_t numImages,
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex,
  bool showText = true
);

/**
 * @brief Render child window that shows the labels for a given segmentation label table
 * @param[in] tableIndex Index of the label table
 * @param[in,out] labelTable Pointer to the label table
 * @param[in] updateLabelColorTableTexture Function to update the label table texture
 */
void renderSegLabelsChildWindow(
  size_t tableIndex,
  ParcellationLabelTable* labelTable,
  const std::function<void(size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(size_t labelIndex)>& moveCrosshairsToSegLabelCentroid
);

/**
 * @brief renderLandmarkChildWindow
 * @param imageTransformations
 * @param activeLmGroup
 * @param worldCrosshairsPos
 * @param setWorldCrosshairsPos
 * @param recenterAllViews
 */
void renderLandmarkChildWindow(
  const AppData& appData,
  const ImageTransformations& imageTransformations,
  LandmarkGroup* activeLmGroup,
  const glm::vec3& worldCrosshairsPos,
  const std::function<void(const glm::vec3& worldCrosshairsPos)>& setWorldCrosshairsPos,
  const AllViewsRecenterType& recenterAllViews
);

/**
 * @brief renderPaletteWindow
 * @param name
 * @param showPaletteWindow
 * @param getNumImageColorMaps
 * @param getImageColorMap
 * @param getCurrentImageColormapIndex
 * @param setCurrentImageColormapIndex
 * @param updateImageUniforms
 */
void renderPaletteWindow(
  const char* name,
  bool* showPaletteWindow,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<size_t(void)>& getCurrentImageColorMapIndex,
  const std::function<void(std::size_t cmapIndex)>& setCurrentImageColormapIndex,
  const std::function<bool(void)>& getImageColorMapInverted,
  const std::function<bool(void)>& getImageColorMapContinuous,
  const std::function<int(void)>& getImageColorMapLevels,
  const glm::vec3& hsvModFactors,
  const std::function<void(void)>& updateImageUniforms
);

void renderColorMapWindow();

#endif // UI_WIDGETS_H
