#ifndef UI_TOOLBARS_H
#define UI_TOOLBARS_H

#include "common/PublicTypes.h"
#include "common/SegmentationTypes.h"
#include "common/Types.h"

#include <uuid.h>

#include <functional>
#include <vector>

class AppData;

void renderModeToolbar(
  AppData& appData,
  const std::function<MouseMode(void)>& getMouseMode,
  const std::function<void(MouseMode)>& setMouseMode,
  const std::function<void(void)>& readjustViewport,
  const AllViewsRecenterType& recenterAllViews,
  const std::function<bool(void)>& getOverlayVisibility,
  const std::function<void(bool)>& setOverlayVisibility,
  const std::function<void(int step)>& cycleViews,

  size_t numImages,
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex
);

void renderSegToolbar(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t imageIndex)>& setActiveImageIndex,
  const std::function<bool(size_t imageIndex)>& getImageHasActiveSeg,
  const std::function<void(size_t imageIndex, bool set)>& setImageHasActiveSeg,
  const std::function<void(void)>& readjustViewport,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>&
    executeGraphCutsSeg,
  const std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>&
    executePoissonSeg
);

void renderAnnotationToolbar(
  AppData& appData,
  const FrameBounds& mindowFrameBounds,
  const std::function<void()> paintActiveAnnotation
);

#endif // UI_TOOLBARS_H
