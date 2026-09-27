#include "ui/ImGuiWrapper.h"

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "ui/imgui/ImageSelection.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>

using namespace ui::imgui_detail;

namespace ui::imgui_detail
{
bool imageIsOnlyNonWarpImage(const AppData& appData, const uuids::uuid& imageUid)
{
  const uuid_range_t warpCandidateUids = appData.warpFieldCandidateUidsOrdered();
  std::size_t nonWarpImageCount = 0;
  bool foundImage = false;
  for (const uuids::uuid& candidateUid : appData.imageUidsOrdered()) {
    const bool isOtherWarpCandidate =
      candidateUid != imageUid && std::find(std::begin(warpCandidateUids), std::end(warpCandidateUids), candidateUid) !=
                                    std::end(warpCandidateUids);
    if (isOtherWarpCandidate) {
      continue;
    }
    ++nonWarpImageCount;
    foundImage = foundImage || candidateUid == imageUid;
  }
  return foundImage && nonWarpImageCount == 1u;
}

} // namespace ui::imgui_detail

std::pair<std::string, std::string> ImGuiWrapper::getImageDisplayAndFileNames(std::size_t imageIndex) const
{
  static const std::string s_empty("<unknown>");

  if (const auto imageUid = m_appData.imageUid(imageIndex)) {
    if (const Image* image = m_appData.image(*imageUid)) {
      return {image->settings().displayName(), image->header().fileName().string()};
    }
  }

  return {s_empty.c_str(), s_empty.c_str()};
}

std::size_t ImGuiWrapper::getActiveImageIndex()
{
  if (0 == m_appData.numImages()) {
    return 0;
  }

  if (const auto imageUid = m_appData.activeImageUid()) {
    if (const auto index = m_appData.imageIndex(*imageUid)) {
      return *index;
    }
  }

  spdlog::warn("No valid active image for {} loaded images", m_appData.numImages());
  return 0;
}

void ImGuiWrapper::setActiveImageIndex(std::size_t index)
{
  if (const auto imageUid = m_appData.imageUid(index)) {
    if (!m_appData.setActiveImageUid(*imageUid)) {
      spdlog::warn("Cannot set active image to {}", *imageUid);
    }
  }
  else {
    spdlog::warn("Cannot set active image to invalid index {}", index);
  }
}

bool ImGuiWrapper::getImageHasActiveSeg(std::size_t index)
{
  if (const auto imageUid = m_appData.imageUid(index)) {
    return m_appData.isImageBeingSegmented(*imageUid);
  }
  else {
    spdlog::warn("Cannot get whether seg is active for invalid image index {}", index);
    return false;
  }
}

void ImGuiWrapper::setImageHasActiveSeg(std::size_t index, bool set)
{
  if (const auto imageUid = m_appData.imageUid(index)) {
    m_appData.setImageBeingSegmented(*imageUid, set);
  }
  else {
    spdlog::warn("Cannot set whether seg is active for invalid image index {}", index);
  }
}

MouseMode ImGuiWrapper::getMouseMode()
{
  return m_appData.state().mouseMode();
}

void ImGuiWrapper::setMouseMode(MouseMode mouseMode)
{
  m_callbackHandler.setMouseMode(mouseMode);
}

void ImGuiWrapper::cycleViewLayout(int step)
{
  m_appData.windowData().cycleCurrentLayout(step);
}

std::size_t ImGuiWrapper::getNumImageColorMaps()
{
  return m_appData.numImageColorMaps();
}

ImageColorMap* ImGuiWrapper::getImageColorMap(std::size_t cmapIndex)
{
  if (const auto cmapUid = m_appData.imageColorMapUid(cmapIndex)) {
    return m_appData.imageColorMap(*cmapUid);
  }
  return nullptr;
}

ParcellationLabelTable* ImGuiWrapper::getLabelTable(std::size_t tableIndex)
{
  if (const auto tableUid = m_appData.labelTableUid(tableIndex)) {
    return m_appData.labelTable(*tableUid);
  }
  return nullptr;
}

bool ImGuiWrapper::getImageIsVisibleSetting(std::size_t imageIndex)
{
  if (const auto imageUid = m_appData.imageUid(imageIndex)) {
    if (const Image* image = m_appData.image(*imageUid)) {
      return image->settings().visibility();
    }
  }
  return false;
}

bool ImGuiWrapper::getImageIsActive(std::size_t imageIndex)
{
  if (const auto imageUid = m_appData.imageUid(imageIndex)) {
    if (const auto activeImageUid = m_appData.activeImageUid()) {
      return (*imageUid == *activeImageUid);
    }
  }
  return false;
}

bool ImGuiWrapper::getImageIsReference(std::size_t imageIndex)
{
  if (m_appData.numImages() <= 1) {
    return false;
  }

  if (const auto imageUid = m_appData.imageUid(imageIndex)) {
    if (const auto refImageUid = m_appData.refImageUid()) {
      return (*imageUid == *refImageUid);
    }
  }
  return false;
}

glm::vec3 ImGuiWrapper::getImageIdentificationColor(std::size_t imageIndex)
{
  if (const auto imageUid = m_appData.imageUid(imageIndex)) {
    if (const Image* image = m_appData.image(*imageUid)) {
      return image->settings().borderColor();
    }
  }
  return glm::vec3{0.5f};
}

bool ImGuiWrapper::moveImageBackward(const uuids::uuid& imageUid)
{
  if (m_appData.moveImageBackwards(imageUid)) {
    m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
    return true;
  }
  return false;
}

bool ImGuiWrapper::moveImageForward(const uuids::uuid& imageUid)
{
  if (m_appData.moveImageForwards(imageUid)) {
    m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
    return true;
  }
  return false;
}

bool ImGuiWrapper::moveImageToBack(const uuids::uuid& imageUid)
{
  if (m_appData.moveImageToBack(imageUid)) {
    m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
    return true;
  }
  return false;
}

bool ImGuiWrapper::moveImageToFront(const uuids::uuid& imageUid)
{
  if (m_appData.moveImageToFront(imageUid)) {
    m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
    return true;
  }
  return false;
}

std::optional<uuids::uuid> ImGuiWrapper::activeImageUid()
{
  return m_appData.activeImageUid();
}

std::optional<uuids::uuid> ImGuiWrapper::activeSegUid()
{
  const auto imageUid = activeImageUid();
  return imageUid ? m_appData.imageToActiveSegUid(*imageUid) : std::nullopt;
}

std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid>> ImGuiWrapper::activeAnnotation()
{
  const auto imageUid = activeImageUid();
  if (!imageUid) {
    return {std::nullopt, std::nullopt};
  }
  return {*imageUid, m_appData.imageToActiveAnnotationUid(*imageUid)};
}

std::optional<uuids::uuid> ImGuiWrapper::activeLandmarkGroupUid()
{
  const auto imageUid = activeImageUid();
  return imageUid ? m_appData.imageToActiveLandmarkGroupUid(*imageUid) : std::nullopt;
}
