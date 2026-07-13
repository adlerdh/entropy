#include "rendering/vector/LightboxOverlayDrawing.h"

#include "rendering/helpers/LightboxOffsetLabelFormat.h"

#include "common/DirectionMaps.h"

#include "logic/app/Data.h"
#include "logic/app/DataHelper.h"
#include "logic/camera/CameraHelpers.h"

#include "windowing/View.h"

#include <glm/glm.hpp>

#include <nanovg.h>

#include <algorithm>
#include <string>

namespace
{

static const std::string ROBOTO_LIGHT("robotoLight");

} // namespace

void drawLightboxOffsetLabel(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  AppData& appData,
  const View& view,
  double unitReferenceLengthMm,
  const glm::vec4& color)
{
  const float offsetMm = data::computeViewOffsetDistance(
    appData,
    view.offsetSetting(),
    helper::worldDirection(view.camera(), Directions::View::Front));

  static constexpr float sk_shadowBlur = 2.0f;
  static constexpr float sk_padding = 5.0f;

  const glm::vec2 miewportMinCorner(miewportViewBounds.bounds.xoffset, miewportViewBounds.bounds.yoffset);
  const glm::vec2 miewportSize(miewportViewBounds.bounds.width, miewportViewBounds.bounds.height);
  const float fontSizePixels = glm::clamp(0.065f * std::min(miewportSize.x, miewportSize.y), 9.0f, 14.0f);
  const std::string label = rendering::lightbox::formatOffsetDistanceMm(
    offsetMm,
    unitReferenceLengthMm,
    static_cast<int>(appData.guiData().m_coordsPrecision));

  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  nvgFontSize(nvg, fontSizePixels);
  nvgFontFace(nvg, ROBOTO_LIGHT.c_str());
  nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

  const glm::vec2 labelPos = miewportMinCorner + glm::vec2{sk_padding, sk_padding};

  nvgFontBlur(nvg, sk_shadowBlur);
  nvgFillColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, color.a));
  nvgText(nvg, labelPos.x, labelPos.y, label.c_str(), nullptr);

  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, nvgRGBAf(color.r, color.g, color.b, color.a));
  nvgText(nvg, labelPos.x, labelPos.y, label.c_str(), nullptr);

  nvgResetScissor(nvg);
}
