#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/imgui/TimePlayback.h"

#include "image/TimePlaybackController.h"
#include "logic/app/Data.h"
#include "rendering/TextureSetup.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/Scaling.h"

#include <IconsForkAwesome.h>
#include <imgui/imgui_internal.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <limits>
#include <unordered_map>
#include <utility>

namespace ui::imgui_detail
{
constexpr double k_maxTimePlaybackFramesPerSecond = 120.0;

void refreshTimeSeriesTexture(AppData& appData, const uuids::uuid& imageUid)
{
  refreshImageTexturesForActiveTimePoint(appData, imageUid);
}

void setTimePoint(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t timePoint)
{
  const uint32_t clamped = image.timeAxis().clamp(timePoint);
  if (image.settings().activeTimePoint() == clamped) {
    return;
  }
  image.settings().setActiveTimePoint(clamped);
  refreshTimeSeriesTexture(appData, imageUid);
}

void setTimePointWithSynchronization(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t timePoint)
{
  setTimePoint(appData, imageUid, image, timePoint);
  if (!appData.settings().synchronizeTimeSeries()) {
    return;
  }

  for (const uuids::uuid& otherUid : appData.imageUidsOrdered()) {
    if (otherUid == imageUid) {
      continue;
    }
    Image* other = appData.image(otherUid);
    if (other && other->isTimeSeries()) {
      setTimePoint(appData, otherUid, *other, timePoint);
    }
  }
}

void stopOtherTimeSeriesPlayback(AppData& appData, const uuids::uuid& playingImageUid)
{
  for (const uuids::uuid& otherUid : appData.imageUidsOrdered()) {
    if (otherUid == playingImageUid) {
      continue;
    }
    Image* other = appData.image(otherUid);
    if (other && other->isTimeSeries()) {
      other->settings().setTimePlaybackPlaying(false);
    }
  }
}

std::string timePointControlLabel(const Image& image, uint32_t timePoint, uint32_t timePrecision)
{
  std::ostringstream os;
  os << timePoint << " of " << (image.timeAxis().numTimePoints() - 1u);
  if (const auto value = image.timeAxis().value(timePoint)) {
    os << " (" << std::fixed << std::setprecision(timePrecision) << *value << " " << image.timeAxis().units() << ")";
  }
  return os.str();
}

float timePointControlLabelWidth(const Image& image, uint32_t timePrecision)
{
  const uint32_t lastTimePoint = image.timeAxis().numTimePoints() - 1u;
  return std::max(
    ImGui::CalcTextSize(timePointControlLabel(image, 0u, timePrecision).c_str()).x,
    ImGui::CalcTextSize(timePointControlLabel(image, lastTimePoint, timePrecision).c_str()).x);
}

void renderReservedWidthText(const char* text, float reservedWidth, ImU32 color)
{
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float height = ImGui::GetFrameHeight();
  const float offsetY = 0.5f * (height - ImGui::GetTextLineHeight());
  ImGui::Dummy(ImVec2{reservedWidth, height});
  ImGui::GetWindowDrawList()->AddText(ImVec2{pos.x, pos.y + offsetY}, color, text);
}

void renderReservedWidthText(const char* text, float reservedWidth)
{
  renderReservedWidthText(text, reservedWidth, ImGui::GetColorU32(ImGuiCol_Text));
}

std::string playbackRateLabel(const Image& image)
{
  const double speed = std::min(
    image.settings().timePlaybackSpeed(),
    image.timeAxis().maxPlaybackSpeedForFrameRate(k_maxTimePlaybackFramesPerSecond));
  const double framePeriod = image.timeAxis().playbackFramePeriodSeconds(speed);
  const double framesPerSecond = playbackFramesPerSecond(framePeriod);
  const char* frameWord = std::abs(framesPerSecond - 1.0) < 1.0e-6 ? "frame" : "frames";
  std::ostringstream os;
  os << std::fixed << std::setprecision(1) << speed << "x (" << framesPerSecond << ' ' << frameWord << "/sec)";
  return os.str();
}

std::optional<uuids::uuid> globalTimeControlImageUid(AppData& appData)
{
  if (const auto activeUid = appData.activeImageUid()) {
    if (const Image* activeImage = appData.image(*activeUid); activeImage && activeImage->isTimeSeries()) {
      return activeUid;
    }
  }

  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    if (const Image* image = appData.image(imageUid); image && image->isTimeSeries()) {
      return imageUid;
    }
  }
  return std::nullopt;
}

void updateTimePlayback(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t activeTimePoint)
{
  // cppcheck-suppress threadsafety-threadsafety
  static thread_local std::unordered_map<uuids::uuid, TimePlaybackState> s_playbackStateByImage;

  if (!image.settings().timePlaybackPlaying()) {
    s_playbackStateByImage.erase(imageUid);
    return;
  }

  TimePlaybackState& state = s_playbackStateByImage[imageUid];
  const TimePlaybackUpdate update = updateTimePlaybackFrame(
    state,
    TimePlaybackInput{
      .playing = image.settings().timePlaybackPlaying(),
      .loop = image.settings().timePlaybackLoop(),
      .activeTimePoint = activeTimePoint,
      .numTimePoints = image.timeAxis().numTimePoints(),
      .framePeriodSeconds = image.timeAxis().playbackFramePeriodSeconds(image.settings().timePlaybackSpeed()),
      .nowSeconds = ImGui::GetTime()});
  if (update.playingChanged) {
    image.settings().setTimePlaybackPlaying(update.playing);
  }
  if (!update.playing) {
    s_playbackStateByImage.erase(imageUid);
  }
  if (!update.advanced) {
    return;
  }

  setTimePointWithSynchronization(appData, imageUid, image, update.timePoint);
}

bool anyTimeSeriesPlaybackRunning(const AppData& appData)
{
  return std::ranges::any_of(appData.imageUidsOrdered(), [&appData](const uuids::uuid& imageUid) {
    const Image* image = appData.image(imageUid);
    return image && image->isTimeSeries() && image->settings().timePlaybackPlaying();
  });
}

void updateAllTimeSeriesPlayback(AppData& appData)
{
  std::optional<uuids::uuid> playbackDriverUid;
  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    Image* image = appData.image(imageUid);
    if (!image || !image->isTimeSeries()) {
      continue;
    }

    if (image->settings().timePlaybackPlaying()) {
      if (playbackDriverUid) {
        image->settings().setTimePlaybackPlaying(false);
        updateTimePlayback(appData, imageUid, *image, image->timeAxis().clamp(image->settings().activeTimePoint()));
        continue;
      }
      playbackDriverUid = imageUid;
    }

    updateTimePlayback(appData, imageUid, *image, image->timeAxis().clamp(image->settings().activeTimePoint()));
  }
}

void renderGlobalTimeControl(AppData& appData)
{
  // cppcheck-suppress threadsafety-threadsafety
  static thread_local bool s_timePlaybackWasRunning = false;
  auto updatePlaybackAnimationState = [&appData]() {
    const bool playbackRunning = anyTimeSeriesPlaybackRunning(appData);
    if (playbackRunning) {
      appData.state().setAnimating(true);
    }
    else if (s_timePlaybackWasRunning) {
      appData.state().setAnimating(false);
    }
    s_timePlaybackWasRunning = playbackRunning;
  };

  updateAllTimeSeriesPlayback(appData);

  const auto imageUid = globalTimeControlImageUid(appData);
  if (!imageUid) {
    if (s_timePlaybackWasRunning) {
      appData.state().setAnimating(false);
      s_timePlaybackWasRunning = false;
    }
    return;
  }

  Image* image = appData.image(*imageUid);
  if (!image || !image->isTimeSeries()) {
    if (s_timePlaybackWasRunning) {
      appData.state().setAnimating(false);
      s_timePlaybackWasRunning = false;
    }
    return;
  }

  const uint32_t maxTimePoint = image->timeAxis().numTimePoints() - 1u;
  const uint32_t activeTimePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
  uint32_t requestedTimePoint = activeTimePoint;

  if (!appData.settings().showGlobalTimeControls()) {
    updatePlaybackAnimationState();
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    updatePlaybackAnimationState();
    return;
  }

  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoDocking;
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::SetNextWindowPos(
    ImVec2{
      viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
      viewport->WorkPos.y + viewport->WorkSize.y - ui::scaledPixel(12.0f)},
    ImGuiCond_Always,
    ImVec2{0.5f, 1.0f});

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::scaledPixel(5.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ui::scaledSize(8.0f, 6.0f));
  if (ImGui::Begin("GlobalTimeControl", nullptr, flags)) {
    const uint32_t timePrecision = appData.guiData().m_timeValuePrecision;
    const float timeLabelWidth = timePointControlLabelWidth(*image, timePrecision);
    const std::string timeLabel = timePointControlLabel(*image, activeTimePoint, timePrecision);
    const std::string rateLabel = playbackRateLabel(*image);
    const float rateLabelWidth = ImGui::CalcTextSize(rateLabel.c_str()).x;
    const ImVec2 buttonSize{ImGui::GetFrameHeight(), ImGui::GetFrameHeight()};

    bool playing = image->settings().timePlaybackPlaying();
    const std::string playbackButtonLabel =
      std::string{playing ? ICON_FK_PAUSE : ICON_FK_PLAY} + "##globalToggleTimePlayback";
    if (ImGui::IconButton(playbackButtonLabel.c_str(), buttonSize)) {
      playing = !playing;
      image->settings().setTimePlaybackPlaying(playing);
      if (playing) {
        stopOtherTimeSeriesPlayback(appData, *imageUid);
      }
      appData.state().setAnimating(playing || anyTimeSeriesPlaybackRunning(appData));
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(playing ? "Pause time-series playback" : "Play time series");
    }
    ImGui::SameLine();
    if (ImGui::IconButton(ICON_FK_FAST_BACKWARD "##globalFirstTimePoint", buttonSize)) {
      requestedTimePoint = 0u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to first time frame");
    }
    ImGui::SameLine();
    if (ImGui::IconButton(ICON_FK_STEP_BACKWARD "##globalPreviousTimePoint", buttonSize)) {
      requestedTimePoint =
        activeTimePoint == 0u ? (image->settings().timePlaybackLoop() ? maxTimePoint : 0u) : activeTimePoint - 1u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to previous time frame");
    }
    ImGui::SameLine();
    if (ImGui::IconButton(ICON_FK_STEP_FORWARD "##globalNextTimePoint", buttonSize)) {
      requestedTimePoint = activeTimePoint == maxTimePoint ? (image->settings().timePlaybackLoop() ? 0u : maxTimePoint)
                                                           : activeTimePoint + 1u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to next time frame");
    }
    ImGui::SameLine();
    if (ImGui::IconButton(ICON_FK_FAST_FORWARD "##globalLastTimePoint", buttonSize)) {
      requestedTimePoint = maxTimePoint;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to last time frame");
    }
    ImGui::SameLine();
    renderReservedWidthText(timeLabel.c_str(), timeLabelWidth);
    ImGui::SameLine();
    renderReservedWidthText(rateLabel.c_str(), rateLabelWidth, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    ImGui::SameLine();
    bool loop = image->settings().timePlaybackLoop();
    if (ImGui::Checkbox("Loop##globalTimePlaybackLoop", &loop)) {
      image->settings().setTimePlaybackLoop(loop);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Loop playback from the last frame back to the first frame");
    }

    if (requestedTimePoint != activeTimePoint) {
      setTimePointWithSynchronization(appData, *imageUid, *image, requestedTimePoint);
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  updatePlaybackAnimationState();
}

} // namespace ui::imgui_detail
