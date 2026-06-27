#include "rendering/VectorDrawing.h"

#include "rendering/LightboxOffsetLabelFormat.h"

#include "logic/app/DataHelper.h"
#include "common/DirectionMaps.h"
#include "common/Viewport.h"

#include "image/Image.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"
#include "logic/states/annotation/AnnotationStateHelpers.h"
#include "logic/states/annotation/AnnotationStateMachine.h"

#include "windowing/View.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <glad/glad.h>

#include <nanovg.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <list>
#include <limits>

namespace
{

static const NVGcolor s_black(nvgRGBA(0, 0, 0, 255));
static const NVGcolor s_grey25(nvgRGBA(63, 63, 63, 255));
static const NVGcolor s_grey40(nvgRGBA(102, 102, 102, 255));
static const NVGcolor s_grey50(nvgRGBA(127, 127, 127, 255));
static const NVGcolor s_grey60(nvgRGBA(153, 153, 153, 255));
static const NVGcolor s_grey75(nvgRGBA(195, 195, 195, 255));
static const NVGcolor s_yellowDull(nvgRGBA(128, 128, 0, 255));
static const NVGcolor s_yellow(nvgRGBA(255, 255, 0, 255));
static const NVGcolor s_red(nvgRGBA(255, 0, 0, 255));

static const std::string ROBOTO_LIGHT("robotoLight");

static constexpr float sk_outlineStrokeWidth = 2.0f;

bool isFiniteVec2(const glm::vec2& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y);
}

bool isInsideRect(const glm::vec2& value, const glm::vec2& min, const glm::vec2& size)
{
  return value.x >= min.x && value.y >= min.y && value.x < min.x + size.x && value.y < min.y + size.y;
}

void drawArrow(NVGcontext* nvg, const glm::vec2& start, const glm::vec2& end, const NVGcolor& color, float width)
{
  const glm::vec2 delta = end - start;
  const float length = glm::length(delta);
  if (length < 2.0f) {
    return;
  }

  const glm::vec2 dir = delta / length;
  const glm::vec2 normal{-dir.y, dir.x};
  const float headLength = glm::clamp(0.32f * length, std::max(5.0f, 3.0f * width), std::max(10.0f, 5.0f * width));
  const float headWidth = std::max(0.55f * headLength, 2.25f * width);
  const glm::vec2 headBase = end - headLength * dir;
  const glm::vec2 shaftEnd = end - std::max(0.5f * width, 1.0f) * dir;

  nvgStrokeWidth(nvg, width);
  nvgStrokeColor(nvg, color);
  nvgFillColor(nvg, color);

  nvgBeginPath(nvg);
  nvgMoveTo(nvg, start.x, start.y);
  nvgLineTo(nvg, shaftEnd.x, shaftEnd.y);
  nvgStroke(nvg);

  nvgBeginPath(nvg);
  nvgMoveTo(nvg, end.x, end.y);
  nvgLineTo(nvg, headBase.x + headWidth * normal.x, headBase.y + headWidth * normal.y);
  nvgLineTo(nvg, headBase.x - headWidth * normal.x, headBase.y - headWidth * normal.y);
  nvgClosePath(nvg);
  nvgFill(nvg);
}

float screenDistance(const Viewport& windowVP, const View& view, const glm::vec3& worldA, const glm::vec3& worldB)
{
  const glm::vec2 a = helper::miewport_T_world(windowVP, view.camera(), view.windowClip_T_viewClip(), worldA);
  const glm::vec2 b = helper::miewport_T_world(windowVP, view.camera(), view.windowClip_T_viewClip(), worldB);
  if (!isFiniteVec2(a) || !isFiniteVec2(b)) {
    return 0.0f;
  }
  return glm::length(b - a);
}

float screenPixelsPerMillimeter(const Viewport& windowVP, const View& view, const glm::vec3& worldCrosshairs)
{
  const glm::vec3 worldRight = helper::worldDirection(view.camera(), Directions::View::Right);
  const glm::vec3 worldUp = helper::worldDirection(view.camera(), Directions::View::Up);
  const float rightPx = screenDistance(windowVP, view, worldCrosshairs, worldCrosshairs + worldRight);
  const float upPx = screenDistance(windowVP, view, worldCrosshairs, worldCrosshairs + worldUp);
  const float meanPx = 0.5f * (rightPx + upPx);
  return meanPx > 0.0f ? meanPx : 1.0f;
}

float screenPixelsPerVoxel(
  const Viewport& windowVP,
  const View& view,
  const Image& image,
  const glm::vec3& worldCrosshairs)
{
  const glm::mat4 subject_T_world = image.transformations().subject_T_worldDef();
  const glm::mat4 pixel_T_subject = image.transformations().pixel_T_subject();
  const glm::mat4 subject_T_pixel = image.transformations().subject_T_pixel();
  const glm::mat4 world_T_subject = image.transformations().worldDef_T_subject();

  const glm::vec3 subjectBase{subject_T_world * glm::vec4{worldCrosshairs, 1.0f}};
  const glm::vec3 pixelBase{pixel_T_subject * glm::vec4{subjectBase, 1.0f}};
  const glm::vec3 worldBase{world_T_subject * glm::vec4{subjectBase, 1.0f}};

  std::array<float, 3> axisLengths{};
  for (int axis = 0; axis < 3; ++axis) {
    glm::vec3 pixelEnd = pixelBase;
    pixelEnd[axis] += 1.0f;
    const glm::vec3 subjectEnd{subject_T_pixel * glm::vec4{pixelEnd, 1.0f}};
    const glm::vec3 worldEnd{world_T_subject * glm::vec4{subjectEnd, 1.0f}};
    axisLengths[axis] = screenDistance(windowVP, view, worldBase, worldEnd);
  }

  std::sort(axisLengths.begin(), axisLengths.end(), std::greater<float>{});
  const float meanInPlanePx = 0.5f * (axisLengths[0] + axisLengths[1]);
  return meanInPlanePx > 0.0f ? meanInPlanePx : 1.0f;
}

float vectorArrowSpacingPixels(
  const ImageSettings& settings,
  const Viewport& windowVP,
  const View& view,
  const Image& image,
  const glm::vec3& worldCrosshairs)
{
  switch (settings.vectorArrowOverlaySpacingMode()) {
    case VectorArrowOverlaySpacingMode::Pixels:
      return std::max(settings.vectorArrowOverlayDensity(), 0.1f);
    case VectorArrowOverlaySpacingMode::Voxels:
      return std::max(settings.vectorArrowOverlayVoxelSpacing(), 0.1f) *
             screenPixelsPerVoxel(windowVP, view, image, worldCrosshairs);
    case VectorArrowOverlaySpacingMode::Millimeters:
      return std::max(settings.vectorArrowOverlayMillimeterSpacing(), 0.1f) *
             screenPixelsPerMillimeter(windowVP, view, worldCrosshairs);
  }

  return std::max(settings.vectorArrowOverlayDensity(), 0.1f);
}

glm::vec2 viewClip_T_miewport(const Viewport& windowVP, const View& view, const glm::vec2& miewportPos)
{
  const glm::vec2 viewportPos = helper::viewport_T_miewport(windowVP.height(), miewportPos);
  const glm::vec2 windowClipPos = helper::windowClip_T_viewport(windowVP, viewportPos);
  glm::vec4 viewClipPos = view.viewClip_T_windowClip() * glm::vec4{windowClipPos, 0.0f, 1.0f};
  viewClipPos /= viewClipPos.w;
  return glm::vec2{viewClipPos};
}

glm::vec2 checkerCoordForViewClip(const glm::vec2& viewClipPos, float numCheckers, float aspectRatio)
{
  const glm::vec2 checkerBase = numCheckers * 0.5f * (viewClipPos + glm::vec2{1.0f});
  return glm::mix(
    glm::vec2{checkerBase.x, checkerBase.y / aspectRatio},
    glm::vec2{checkerBase.x * aspectRatio, checkerBase.y},
    aspectRatio <= 1.0f ? 1.0f : 0.0f);
}

bool doRenderComparisonSample(
  ViewRenderMode renderMode,
  const glm::vec2& viewClipPos,
  const glm::vec2& checkerCoord,
  const glm::vec2& clipCrosshairs,
  const glm::ivec2& quadrants,
  bool showFixedImage,
  float aspectRatio,
  float flashlightRadius,
  bool flashlightMovingOnFixed)
{
  if (ViewRenderMode::Image == renderMode) {
    return true;
  }

  if (ViewRenderMode::Checkerboard == renderMode) {
    const bool checkerShowsFixed = std::fmod(std::floor(checkerCoord.x) + std::floor(checkerCoord.y), 2.0f) > 0.5f;
    return showFixedImage == checkerShowsFixed;
  }

  if (ViewRenderMode::Quadrants == renderMode) {
    const glm::bvec2 quadrant{viewClipPos.x <= clipCrosshairs.x, viewClipPos.y > clipCrosshairs.y};
    const bool quadrantShowsFixed =
      ((!static_cast<bool>(quadrants.x) || quadrant.x) == (!static_cast<bool>(quadrants.y) || quadrant.y));
    return showFixedImage == quadrantShowsFixed;
  }

  if (ViewRenderMode::Flashlight == renderMode) {
    const glm::vec2 delta{aspectRatio * (viewClipPos.x - clipCrosshairs.x), viewClipPos.y - clipCrosshairs.y};
    const float flashlightDistance = glm::length(delta);
    return (showFixedImage == (flashlightDistance > flashlightRadius)) || (flashlightMovingOnFixed && showFixedImage);
  }

  return false;
}
} // namespace

void startNvgFrame(NVGcontext* nvg, const Viewport& windowVP)
{
  if (!nvg) {
    return;
  }

  nvgShapeAntiAlias(nvg, true);

  // Sets the composite operation. NVG_SOURCE_OVER is the default.
  nvgGlobalCompositeOperation(nvg, NVG_SOURCE_OVER);

  // Sets the composite operation with custom pixel arithmetic.
  // Note: The default compositing factors for NanoVG are
  // sfactor = NVG_ONE and dfactor = NVG_ONE_MINUS_SRC_ALPHA
  nvgGlobalCompositeBlendFunc(nvg, NVG_SRC_ALPHA, NVG_ONE_MINUS_SRC_ALPHA);

  nvgBeginFrame(nvg, windowVP.width(), windowVP.height(), windowVP.devicePixelRatio().x);
  nvgSave(nvg);
}

void endNvgFrame(NVGcontext* nvg)
{
  if (!nvg) {
    return;
  }

  nvgRestore(nvg);
  nvgEndFrame(nvg);
}

void drawLoadingOverlay(NVGcontext* nvg, const Viewport& windowVP)
{
  using namespace std::chrono;
  /// @todo Progress indicators: https://github.com/ocornut/imgui/issues/1901

  static const NVGcolor s_greyTextColor(nvgRGBA(190, 190, 190, 255));
  static const NVGcolor s_greyShadowColor(nvgRGBA(64, 64, 64, 255));

  static constexpr float sk_arcAngle = 1.0f / 16.0f * NVG_PI;
  static const std::string sk_loadingText = "Loading images...";

  nvgFontSize(nvg, 64.0f);
  nvgFontFace(nvg, ROBOTO_LIGHT.c_str());

  nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  nvgFontBlur(nvg, 2.0f);
  nvgFillColor(nvg, s_greyShadowColor);
  nvgText(nvg, 0.5f * windowVP.width(), 0.5f * windowVP.height(), sk_loadingText.c_str(), nullptr);

  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, s_greyTextColor);
  nvgText(nvg, 0.5f * windowVP.width(), 0.5f * windowVP.height(), sk_loadingText.c_str(), nullptr);

  const auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  const float C = 2.0f * NVG_PI * static_cast<float>(ms.count() % 1000) / 1000.0f;
  const float radius = windowVP.width() / 16.0f;

  nvgStrokeWidth(nvg, 8.0f);
  nvgStrokeColor(nvg, s_greyTextColor);

  nvgBeginPath(nvg);
  nvgArc(nvg, 0.5f * windowVP.width(), 0.75f * windowVP.height(), radius, sk_arcAngle + C, C, NVG_CCW);
  nvgClosePath(nvg);
  nvgStroke(nvg);
}

void drawWindowOutline(NVGcontext* nvg, const Viewport& windowVP)
{
  static constexpr float k_pad = 1.0f;

  // Outline around window
  nvgStrokeWidth(nvg, sk_outlineStrokeWidth);
  nvgStrokeColor(nvg, s_grey50);
  //    nvgStrokeColor( nvg, s_red );

  nvgBeginPath(nvg);
  nvgRoundedRect(nvg, k_pad, k_pad, windowVP.width() - 2.0f * k_pad, windowVP.height() - 2.0f * k_pad, 3.0f);
  nvgClosePath(nvg);
  nvgStroke(nvg);

  //        nvgStrokeWidth( nvg, 2.0f );
  //        nvgStrokeColor( nvg, s_grey50 );
  //        nvgRect( nvg, pad, pad, windowVP.width() - pad, windowVP.height() - pad );
  //        nvgStroke( nvg );

  //        nvgStrokeWidth( nvg, 1.0f );
  //        nvgStrokeColor( nvg, s_grey60 );
  //        nvgRect( nvg, pad, pad, windowVP.width() - pad, windowVP.height() - pad );
  //        nvgStroke( nvg );
}

void drawViewOutline(NVGcontext* nvg, const FrameBounds& miewportViewBounds, const ViewOutlineMode& outlineMode)
{
  static constexpr float k_padOuter = 0.0f;
  //    static constexpr float k_padInner = 2.0f;
  static constexpr float k_padActive = 3.0f;

  auto drawRectangle = [&nvg, &miewportViewBounds](float pad, float width, const NVGcolor& color) {
    nvgStrokeWidth(nvg, width);
    nvgStrokeColor(nvg, color);

    nvgBeginPath(nvg);
    nvgRect(
      nvg,
      miewportViewBounds.bounds.xoffset + pad,
      miewportViewBounds.bounds.yoffset + pad,
      miewportViewBounds.bounds.width - 2.0f * pad,
      miewportViewBounds.bounds.height - 2.0f * pad);
    nvgClosePath(nvg);
    nvgStroke(nvg);
  };

  switch (outlineMode) {
    case ViewOutlineMode::Hovered: {
      drawRectangle(k_padActive, 2.0f, s_yellowDull);
      break;
    }
    case ViewOutlineMode::Selected: {
      drawRectangle(k_padActive, 2.0f, s_yellow);
      break;
    }
    case ViewOutlineMode::None: {
      break;
    }
  }

  drawRectangle(k_padOuter, sk_outlineStrokeWidth, s_grey50);
  //    drawRectangle( k_padInner, 1.0f, s_grey60 );
}

void drawImageViewIntersections(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairsOrigin,
  AppData& appData,
  const View& view,
  const ImageSegPairs& I,
  bool renderInactiveImageIntersections)
{
  // Line segment stipple length in pixels
  static constexpr float sk_stippleLen = 16.0f;

  // These are the crosshairs in which the origin have been offset according to the view:
  CoordinateFrame crosshairs = appData.state().worldCrosshairs();
  crosshairs.setWorldOrigin(worldCrosshairsOrigin);

  nvgLineCap(nvg, NVG_BUTT);
  nvgLineJoin(nvg, NVG_MITER);

  startNvgFrame(nvg, appData.windowData().viewport()); /*** START FRAME ***/

  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  // Render border for each image
  for (const auto& imgSegPair : I) {
    if (!imgSegPair.first) {
      continue;
    }

    const auto imgUid = *(imgSegPair.first);
    const Image* img = appData.image(imgUid);
    if (!img) {
      continue;
    }

    const auto activeImageUid = appData.activeImageUid();
    const bool isActive = (activeImageUid && (*activeImageUid == imgUid));

    //        if ( ! isActive && ! renderInactiveImageIntersections ) continue;
    if (!renderInactiveImageIntersections) {
      continue;
    }

    auto worldIntersections = view.computeImageSliceIntersection(img, crosshairs);
    if (!worldIntersections) {
      continue;
    }

    // The last point is the centroid of the intersection. Ignore the centroid and replace it with a
    // duplicate of the first point. We need to double-up that point in order for line stippling to
    // work correctly. Also, no need to close the path with nvgClosePath if the last point is
    // duplicated.
    worldIntersections->at(6) = worldIntersections->at(0);

    const glm::vec3 color = img->settings().borderColor();

    const float opacity = img->settings().displayImageAsColor()
                            ? static_cast<float>(img->settings().globalVisibility() * img->settings().globalOpacity())
                            : static_cast<float>(
                                img->settings().globalVisibility() * img->settings().globalOpacity() *
                                img->settings().visibility() * img->settings().opacity());

    nvgStrokeColor(nvg, nvgRGBAf(color.r, color.g, color.b, opacity));
    nvgStrokeWidth(nvg, isActive ? 1.5f : 1.0f);

    glm::vec2 lastPos;

    nvgBeginPath(nvg);

    for (size_t i = 0; i < worldIntersections->size(); ++i) {
      const glm::vec2 currPos = helper::miewport_T_world(
        appData.windowData().viewport(),
        view.camera(),
        view.windowClip_T_viewClip(),
        worldIntersections->at(i));

      if (0 == i) {
        // Move pen to the first point:
        nvgMoveTo(nvg, currPos.x, currPos.y);
        lastPos = currPos;
        continue;
      }

      if (isActive) {
        // The active image gets a stippled line pattern
        const float dist = glm::distance(lastPos, currPos);
        const uint32_t numLines = static_cast<uint32_t>(dist / sk_stippleLen);

        if (0 == numLines) {
          // At a minimum, draw one stipple line:
          nvgLineTo(nvg, currPos.x, currPos.y);
        }

        for (uint32_t j = 1; j <= numLines; ++j) {
          const float t = static_cast<float>(j) / static_cast<float>(numLines);
          const glm::vec2 pos = lastPos + t * (currPos - lastPos);

          // To create the stipple pattern, alternate drawing lines and
          // moving the pen on odd/even values of j:
          if (j % 2) {
            nvgLineTo(nvg, pos.x, pos.y);
          }
          else {
            nvgMoveTo(nvg, pos.x, pos.y);
          }
        }
      }
      else {
        // Non-active images get solid lines
        nvgLineTo(nvg, currPos.x, currPos.y);
      }

      lastPos = currPos;
    }

    nvgClosePath(nvg);
    nvgStroke(nvg);
  }

  nvgResetScissor(nvg);

  endNvgFrame(nvg); /*** END FRAME ***/
}

void drawAnatomicalLabels(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  bool isViewOblique,
  const glm::vec4& fontColor,
  const AnatomicalLabelType& anatLabelType,
  float labelScale,
  const std::array<AnatomicalLabelPosInfo, 2>& labelPosInfo)
{
  static constexpr float sk_fontMult = 0.03f;

  if (AnatomicalLabelType::Disabled == anatLabelType) {
    return;
  }

  auto getLabelAbbrev = [&anatLabelType](int labelIndex) -> const char* {
    switch (anatLabelType) {
      case AnatomicalLabelType::Cartesian: {
        switch (labelIndex) {
          case 0:
            return Directions::abbrev(Directions::Cartesian::PosX).c_str();
          case 1:
            return Directions::abbrev(Directions::Cartesian::PosY).c_str();
          case 2:
            return Directions::abbrev(Directions::Cartesian::PosZ).c_str();
          case 3:
            return Directions::abbrev(Directions::Cartesian::NegX).c_str();
          case 4:
            return Directions::abbrev(Directions::Cartesian::NegY).c_str();
          case 5:
            return Directions::abbrev(Directions::Cartesian::NegZ).c_str();
          default:
            return "";
        }
        break;
      }
      case AnatomicalLabelType::Human: {
        switch (labelIndex) {
          case 0:
            return Directions::abbrev(Directions::Anatomy::Left).c_str();
          case 1:
            return Directions::abbrev(Directions::Anatomy::Posterior).c_str();
          case 2:
            return Directions::abbrev(Directions::Anatomy::Superior).c_str();
          case 3:
            return Directions::abbrev(Directions::Anatomy::Right).c_str();
          case 4:
            return Directions::abbrev(Directions::Anatomy::Anterior).c_str();
          case 5:
            return Directions::abbrev(Directions::Anatomy::Inferior).c_str();
          default:
            return "";
        }
        break;
      }
      case AnatomicalLabelType::Rodent: {
        switch (labelIndex) {
          case 0:
            return Directions::abbrev(Directions::Animal::Left).c_str();
          case 1:
            return Directions::abbrev(Directions::Animal::Dorsal).c_str();
          case 2:
            return Directions::abbrev(Directions::Animal::Rostral).c_str();
          case 3:
            return Directions::abbrev(Directions::Animal::Right).c_str();
          case 4:
            return Directions::abbrev(Directions::Animal::Ventral).c_str();
          case 5:
            return Directions::abbrev(Directions::Animal::Caudal).c_str();
          default:
            return "";
        }
        break;
      }
      case AnatomicalLabelType::Disabled: {
        return "";
      }
    }
    return "";
  };

  const float inwardShiftMultiplier =
    (AnatomicalLabelType::Human == anatLabelType || AnatomicalLabelType::Cartesian == anatLabelType) ? 1.0f : 1.3f;

  const glm::vec2 miewportMinCorner(miewportViewBounds.bounds.xoffset, miewportViewBounds.bounds.yoffset);
  const glm::vec2 miewportSize(miewportViewBounds.bounds.width, miewportViewBounds.bounds.height);
  const glm::vec2 miewportMaxCorner = miewportMinCorner + miewportSize;

  // Clip against the view bounds, even though not strictly necessary with how lines are defined
  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  const float fontSizePixels =
    std::max(sk_fontMult * std::min(miewportViewBounds.bounds.width, miewportViewBounds.bounds.height), 8.0f) *
    glm::clamp(labelScale, 0.5f, 2.0f);

  // For inward shift of the labels:
  const glm::vec2 inwardFontShift{
    0.8f * inwardShiftMultiplier * fontSizePixels,
    0.8f * inwardShiftMultiplier * fontSizePixels};

  // For downward shift of the labels:
  const glm::vec2 vertFontShift{0.0f, 0.35f * fontSizePixels};

  nvgFontSize(nvg, fontSizePixels);
  nvgFontFace(nvg, ROBOTO_LIGHT.c_str());
  nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);

  // Render the translation vectors for the L (0), P (1), and S (2) labels:
  for (const auto& label : labelPosInfo) {
    const glm::vec2 miewportPositivePos = glm::clamp(
                                            label.miewportLabelPositions[0],
                                            miewportMinCorner + inwardFontShift,
                                            miewportMaxCorner - inwardFontShift) +
                                          vertFontShift;

    const glm::vec2 miewportNegativePos = glm::clamp(
                                            label.miewportLabelPositions[1],
                                            miewportMinCorner + inwardFontShift,
                                            miewportMaxCorner - inwardFontShift) +
                                          vertFontShift;

    const int idx = static_cast<int>(label.labelIndex);

    // Draw the text shadow:
    nvgFontBlur(nvg, 2.0f);
    nvgFillColor(nvg, s_black);
    nvgText(nvg, miewportPositivePos.x, miewportPositivePos.y, getLabelAbbrev(idx), nullptr);
    nvgText(nvg, miewportNegativePos.x, miewportNegativePos.y, getLabelAbbrev(idx + 3), nullptr);

    // Draw the text:
    const float alpha = (isViewOblique ? 0.5f : 1.0f) * fontColor.a;
    nvgFontBlur(nvg, 0.0f);
    nvgFillColor(nvg, nvgRGBAf(fontColor.r, fontColor.g, fontColor.b, alpha));
    nvgText(nvg, miewportPositivePos.x, miewportPositivePos.y, getLabelAbbrev(idx), nullptr);
    nvgText(nvg, miewportNegativePos.x, miewportNegativePos.y, getLabelAbbrev(idx + 3), nullptr);
  }

  nvgResetScissor(nvg);
}

void drawCircle(
  NVGcontext* nvg,
  const glm::vec2& miewportPos,
  float radius,
  const glm::vec4& fillColor,
  const glm::vec4& strokeColor,
  float strokeWidth)
{
  nvgStrokeWidth(nvg, strokeWidth);
  nvgStrokeColor(nvg, nvgRGBAf(strokeColor.r, strokeColor.g, strokeColor.b, strokeColor.a));
  nvgFillColor(nvg, nvgRGBAf(fillColor.r, fillColor.g, fillColor.b, fillColor.a));

  nvgBeginPath(nvg);
  nvgCircle(nvg, miewportPos.x, miewportPos.y, radius);
  nvgClosePath(nvg);
  nvgStroke(nvg);
  nvgFill(nvg);
}

void drawText(
  NVGcontext* nvg,
  const glm::vec2& miewportPos,
  const std::string& centeredString,
  const std::string& offsetString,
  const glm::vec4& textColor,
  float offset,
  float fontSizePixels)
{
  nvgFontFace(nvg, ROBOTO_LIGHT.c_str());

  // Draw centered text
  if (!centeredString.empty()) {
    nvgFontSize(nvg, 1.0f * fontSizePixels);
    nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    nvgFontBlur(nvg, 3.0f);
    nvgFillColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, textColor.a));
    nvgText(nvg, miewportPos.x, miewportPos.y, centeredString.c_str(), nullptr);

    nvgFontBlur(nvg, 0.0f);
    nvgFillColor(nvg, nvgRGBAf(textColor.r, textColor.g, textColor.b, textColor.a));
    nvgText(nvg, miewportPos.x, miewportPos.y, centeredString.c_str(), nullptr);
  }

  // Draw offset text
  if (!offsetString.empty()) {
    nvgFontSize(nvg, 1.15f * fontSizePixels);
    nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    nvgFontBlur(nvg, 3.0f);
    nvgFillColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, textColor.a));
    nvgText(nvg, offset + miewportPos.x, offset + miewportPos.y, offsetString.c_str(), nullptr);

    nvgFontBlur(nvg, 0.0f);
    nvgFillColor(nvg, nvgRGBAf(textColor.r, textColor.g, textColor.b, textColor.a));
    nvgText(nvg, offset + miewportPos.x, offset + miewportPos.y, offsetString.c_str(), nullptr);
  }
}

void drawLandmarks(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& I)
{
  static constexpr float sk_minSize = 4.0f;
  static constexpr float sk_maxSize = 128.0f;

  startNvgFrame(nvg, appData.windowData().viewport()); /*** START FRAME ***/

  // Clip against the view bounds
  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  const float strokeWidth = appData.renderData().m_globalLandmarkParams.strokeWidth;

  const glm::vec3 worldViewNormal = helper::worldDirection(view.camera(), Directions::View::Back);
  const glm::vec4 worldViewPlane = math::makePlane(worldViewNormal, worldCrosshairs);

  // Render landmarks for each image
  for (const auto& imgSegPair : I) {
    if (!imgSegPair.first) {
      // Non-existent image
      continue;
    }

    const auto imgUid = *(imgSegPair.first);
    const Image* img = appData.image(imgUid);

    if (!img) {
      spdlog::error("Null image {} when rendering landmarks", imgUid);
      continue;
    }

    // Don't render landmarks for invisible image:
    /// @todo Need to properly manage global visibility vs. visibility for just one component
    if (
      !img->settings().globalVisibility() ||
      (1 == img->header().numComponentsPerPixel() && !img->settings().visibility()))
    {
      continue;
    }

    const auto lmGroupUids = appData.imageToLandmarkGroupUids(imgUid);
    if (lmGroupUids.empty()) {
      continue;
    }

    // Slice spacing of the image along the view normal
    const float sliceSpacing = data::sliceScrollDistance(-worldViewNormal, *img);

    for (const auto& lmGroupUid : lmGroupUids) {
      const LandmarkGroup* lmGroup = appData.landmarkGroup(lmGroupUid);
      if (!lmGroup) {
        spdlog::error("Null landmark group for image {}", imgUid);
        continue;
      }

      if (!lmGroup->getVisibility()) {
        continue;
      }

      // Matrix that transforms landmark position from either Voxel or Subject to World space.
      const glm::mat4 world_T_landmark = (lmGroup->getInVoxelSpace()) ? img->transformations().worldDef_T_pixel()
                                                                      : img->transformations().worldDef_T_subject();

      const float minDim = std::min(miewportViewBounds.bounds.width, miewportViewBounds.bounds.height);
      const float pixelsMaxLmSize = glm::clamp(lmGroup->getRadiusFactor() * minDim, sk_minSize, sk_maxSize);

      for (const auto& p : lmGroup->getPoints()) {
        const std::size_t index = p.first;
        const PointRecord<glm::vec3>& point = p.second;

        if (!point.getVisibility()) {
          continue;
        }

        // Put landmark into World space
        const glm::vec4 worldLmPos = world_T_landmark * glm::vec4{point.getPosition(), 1.0f};
        const glm::vec3 worldLmPos3 = glm::vec3{worldLmPos / worldLmPos.w};

        // Landmark must be within a distance of half the image slice spacing along the
        // direction of the view to be rendered in the view
        const float distLmToPlane = std::abs(math::signedDistancePointToPlane(worldLmPos3, worldViewPlane));

        // Maximum distance beyond which the landmark is not rendered:
        const float maxDist = 0.5f * sliceSpacing;
        if (distLmToPlane >= maxDist) {
          continue;
        }

        const glm::vec2 miewportPos = helper::miewport_T_world(
          appData.windowData().viewport(),
          view.camera(),
          view.windowClip_T_viewClip(),
          worldLmPos3);

        const bool inView = miewportViewBounds.bounds.xoffset < miewportPos.x &&
                            miewportViewBounds.bounds.yoffset < miewportPos.y &&
                            miewportPos.x < miewportViewBounds.bounds.xoffset + miewportViewBounds.bounds.width &&
                            miewportPos.y < miewportViewBounds.bounds.yoffset + miewportViewBounds.bounds.height;

        if (!inView) {
          continue;
        }

        // Use the landmark group color if defined
        const bool lmGroupColorOverride = lmGroup->getColorOverride();
        const glm::vec3 lmGroupColor = lmGroup->getColor();
        const float lmGroupOpacity = lmGroup->getOpacity();

        // Non-premult. alpha:
        const glm::vec4 fillColor{(lmGroupColorOverride) ? lmGroupColor : point.getColor(), lmGroupOpacity};

        /// @todo If landmark is selected, then highlight it here:
        const float strokeOpacity = 1.0f - std::pow((lmGroupOpacity - 1.0f), 2.0f);

        const glm::vec4 strokeColor{(lmGroupColorOverride) ? lmGroupColor : point.getColor(), strokeOpacity};

        // Landmark radius depends on distance of the view plane from the landmark center
        const float radius = pixelsMaxLmSize * std::sqrt(std::abs(1.0f - std::pow(distLmToPlane / maxDist, 2.0f)));

        drawCircle(nvg, miewportPos, radius, fillColor, strokeColor, strokeWidth);

        const bool renderIndices = lmGroup->getRenderLandmarkIndices();
        const bool renderNames = lmGroup->getRenderLandmarkNames();

        if (renderIndices || renderNames) {
          const float textOffset = radius + 0.7f;
          const float textSize = 0.9f * pixelsMaxLmSize;

          std::string indexString = (renderIndices) ? std::to_string(index) : "";
          std::string nameString = (renderNames) ? point.getName() : "";

          // Non premult. alpha:
          const auto lmGroupTextColor = lmGroup->getTextColor();
          const glm::vec4 textColor{(lmGroupTextColor) ? *lmGroupTextColor : fillColor, lmGroupOpacity};

          drawText(nvg, miewportPos, indexString, nameString, textColor, textOffset, textSize);
        }
      }
    }
  }

  nvgResetScissor(nvg);

  endNvgFrame(nvg); /*** END FRAME ***/
}

void drawAnnotations(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& I)
{
  /// @todo Should annotation opacity be modulated with image opacity? Landmarks opacity is not.
  /// img->settings().opacity()

  static constexpr std::size_t OUTER_BOUNDARY = 0;

  // Color of selected vertices, edges, and the selection bounding box:
  static const glm::vec4 sk_green{0.0f, 1.0f, 0.0f, 0.75f};

  // Stroke width of selected vertices and edges:
  static constexpr float sk_vertexSelectionStrokeWidth = 2.0f;

  // Stroke width of selection bounding box:
  static constexpr float sk_bboxSelectionStrokeWidth = 1.0f;

  // Radius of selection bounding box corners
  static constexpr float sk_rectCornerRadius = 4.0f;

  // Radius of polygon vertices
  static constexpr float sk_vertexRadius = 3.0f;

  // Radius of polygon vertex selection circle
  static constexpr float sk_vertexSelectionRadius = sk_vertexRadius + 1.0f;

  // Convert vertex coordinates from local annotation plane space to Miewport space:
  auto convertAnnotationPlaneVertexToMiewport =
    [&appData, &view](const Image& image, const Annotation& annot, const glm::vec2& annotPlaneVertex) -> glm::vec2 {
    const glm::vec3 subjectPos = annot.unprojectFromAnnotationPlaneToSubjectPoint(annotPlaneVertex);
    const glm::vec4 worldPos = image.transformations().worldDef_T_subject() * glm::vec4{subjectPos, 1.0f};

    return helper::miewport_T_world(
      appData.windowData().viewport(),
      view.camera(),
      view.windowClip_T_viewClip(),
      glm::vec3{worldPos / worldPos.w});
  };

  startNvgFrame(nvg, appData.windowData().viewport()); /*** START FRAME ***/

  // Other line cap options: NVG_BUTT, NVG_SQUARE
  nvgLineCap(nvg, NVG_ROUND);

  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  const glm::vec3 worldViewNormal = helper::worldDirection(view.camera(), Directions::View::Back);

  // Render annotations for each image
  for (const auto& imgSegPair : I) {
    if (!imgSegPair.first) {
      continue; // Non-existent image
    }

    const auto imgUid = *(imgSegPair.first);
    const Image* img = appData.image(imgUid);
    if (!img) {
      spdlog::error("Null image {} when rendering annotations", imgUid);
      continue;
    }

    // Don't render annotations for invisible image:

    /// @todo Need to properly manage global visibility vs. visibility for just one component
    /// @todo Need to account for global visibility when editing annotations! Should not be able
    /// to modify annotations of invisible image!

    if (
      !img->settings().globalVisibility() ||
      (1 == img->header().numComponentsPerPixel() && !img->settings().visibility()))
    {
      continue;
    }

    // Annotation plane equation in image Subject space:
    const auto [subjectPlaneEquation, subjectPlanePoint] =
      math::computeSubjectPlaneEquation(img->transformations().subject_T_worldDef(), worldViewNormal, worldCrosshairs);

    // Slice spacing of the image along the view normal is the plane distance threshold
    // for annotation searching:
    const float sliceSpacing = 0.5f * data::sliceScrollDistance(-worldViewNormal, *img);

    const auto annotUids = data::findAnnotationsForImage(appData, imgUid, subjectPlaneEquation, sliceSpacing);

    for (const auto& annotUid : annotUids) {
      const Annotation* annot = appData.annotation(annotUid);
      if (!annot) {
        continue;
      }

      const bool visible = (img->settings().visibility() && annot->isVisible());
      if (!visible) {
        continue;
      }

      // Annotation vertices in 2D annotation plane coordinates:
      if (0 == annot->numBoundaries()) {
        continue;
      }

      const std::vector<glm::vec2>& annotPlaneVertices = annot->getBoundaryVertices(OUTER_BOUNDARY);
      if (annotPlaneVertices.empty()) {
        continue;
      }

      // Track the minimum and maximum vertex positions for drawing the bounding box
      glm::vec2 miewportMinPos{std::numeric_limits<float>::max()};
      glm::vec2 miewportMaxPos{std::numeric_limits<float>::lowest()};

      // Set the annotation outer boundary:
      if (annot->isSmoothed()) {
        nvgLineJoin(nvg, NVG_ROUND);

        std::vector<glm::vec2> miewportPoints;

        nvgBeginPath(nvg);

        bool isFirst = true;

        for (const auto& command : annot->getBezierCommands()) {
          const glm::vec2 c1 = convertAnnotationPlaneVertexToMiewport(*img, *annot, std::get<0>(command));
          const glm::vec2 c2 = convertAnnotationPlaneVertexToMiewport(*img, *annot, std::get<1>(command));
          const glm::vec2 p = convertAnnotationPlaneVertexToMiewport(*img, *annot, std::get<2>(command));

          miewportMinPos = glm::min(miewportMinPos, c1);
          miewportMinPos = glm::min(miewportMinPos, c2);
          miewportMinPos = glm::min(miewportMinPos, p);

          miewportMaxPos = glm::max(miewportMaxPos, c1);
          miewportMaxPos = glm::max(miewportMaxPos, c2);
          miewportMaxPos = glm::max(miewportMaxPos, p);

          if (isFirst) {
            // Move pen to the first point:
            nvgMoveTo(nvg, p.x, p.y);
            isFirst = false;
          }
          else {
            nvgBezierTo(nvg, c1.x, c1.y, c2.x, c2.y, p.x, p.y);
          }
        }

        // Note: unlike for non-smoothed boundaries, the Bezier commands already account
        // for closed polygons. There is no need to draw a vertex back to the beggining.
      }
      else {
        nvgLineJoin(nvg, NVG_MITER);

        nvgBeginPath(nvg);

        bool isFirst = true;

        for (const glm::vec2& vertex : annotPlaneVertices) {
          const glm::vec2 miewportPos = convertAnnotationPlaneVertexToMiewport(*img, *annot, vertex);
          miewportMinPos = glm::min(miewportMinPos, miewportPos);
          miewportMaxPos = glm::max(miewportMaxPos, miewportPos);

          if (isFirst) {
            // Move pen to the first point:
            nvgMoveTo(nvg, miewportPos.x, miewportPos.y);
            isFirst = false;
          }
          else {
            nvgLineTo(nvg, miewportPos.x, miewportPos.y);
          }
        }

        // If the annotation is a closed, then create a line back to the first vertex:
        if (annot->isClosed()) {
          //                    const glm::vec2 miewportPos =
          //                    convertAnnotationPlaneVertexToMiewport(
          //                                *img, *annot, annotPlaneVertices.front() );

          //                    nvgLineTo( nvg, miewportPos.x, miewportPos.y );

          nvgClosePath(nvg);
        }
      }

      // Draw the boundary line:
      const glm::vec4& lineColor = annot->getLineColor();
      nvgStrokeColor(nvg, nvgRGBAf(lineColor.r, lineColor.g, lineColor.b, annot->getOpacity() * lineColor.a));
      nvgStrokeWidth(nvg, annot->getLineThickness());
      nvgStroke(nvg);

      // Only fill the annotation if it is closed:
      if (annot->isClosed() && annot->isFilled()) {
        const glm::vec4& fillColor = annot->getFillColor();
        nvgFillColor(nvg, nvgRGBAf(fillColor.r, fillColor.g, fillColor.b, annot->getOpacity() * fillColor.a));
        nvgFill(nvg);
      }

      // Draw the annotation outer boundary vertices:
      if (!appData.renderData().m_globalAnnotationParams.hidePolygonVertices && annot->getVertexVisibility()) {
        for (const glm::vec2& vertex : annotPlaneVertices) {
          const glm::vec2 miewportPos = convertAnnotationPlaneVertexToMiewport(*img, *annot, vertex);
          const float radius = std::max(sk_vertexRadius, annot->getLineThickness());
          const glm::vec4& vertColor = annot->getVertexColor();

          nvgFillColor(nvg, nvgRGBAf(vertColor.r, vertColor.g, vertColor.b, annot->getOpacity() * vertColor.a));

          nvgBeginPath(nvg);
          nvgCircle(nvg, miewportPos.x, miewportPos.y, radius);
          nvgClosePath(nvg);
          nvgFill(nvg);
        }
      }

      // If the annotation opacity equals zero, then do not show selected vertices,
      // edges, or the selection bounding box.
      const bool showSelections = (annot->getOpacity() > 0.0f);

      // Highlight vertices with circles:
      if (showSelections && state::annot::isInStateWhereVertexHighlightsAreVisible()) {
        for (const auto& highlightedVertex : annot->highlightedVertices()) {
          const std::size_t boundary = highlightedVertex.first;
          const std::size_t vertexIndex = highlightedVertex.second;

          const auto selectedVertexCoords = annot->polygon().getBoundaryVertex(boundary, vertexIndex);

          if ((OUTER_BOUNDARY == boundary) && selectedVertexCoords) {
            const glm::vec2 miewportPos = convertAnnotationPlaneVertexToMiewport(*img, *annot, *selectedVertexCoords);
            const float radius = std::max(sk_vertexSelectionRadius, annot->getLineThickness());

            nvgStrokeWidth(nvg, sk_vertexSelectionStrokeWidth);
            nvgStrokeColor(nvg, nvgRGBAf(sk_green.r, sk_green.g, sk_green.b, sk_green.a));

            nvgBeginPath(nvg);
            nvgCircle(nvg, miewportPos.x, miewportPos.y, radius);
            nvgClosePath(nvg);
            nvgStroke(nvg);
          }
        }
      }

      // Draw the annotation outer boundary bounding box:
      if (showSelections && state::annot::isInStateWhereAnnotationHighlightsAreVisible() && annot->isHighlighted()) {
        nvgStrokeWidth(nvg, sk_bboxSelectionStrokeWidth);
        nvgStrokeColor(nvg, nvgRGBAf(sk_green.r, sk_green.g, sk_green.b, sk_green.a));

        nvgBeginPath(nvg);
        nvgRoundedRect(
          nvg,
          miewportMinPos.x,
          miewportMinPos.y,
          miewportMaxPos.x - miewportMinPos.x,
          miewportMaxPos.y - miewportMinPos.y,
          sk_rectCornerRadius);
        nvgClosePath(nvg);
        nvgStroke(nvg);
      }
    }
  }

  nvgResetScissor(nvg);

  endNvgFrame(nvg); /*** END FRAME ***/
}

void drawVectorFieldArrows(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const std::list<uuids::uuid>& imageUids)
{
  static constexpr float k_referenceArrowLengthPx = 18.0f;
  static constexpr float k_maxArrowLengthViewportFraction = 0.22f;
  static constexpr float k_maxArrowLengthPx = 240.0f;

  const Viewport& windowVP = appData.windowData().viewport();
  const glm::vec2 viewMin{miewportViewBounds.bounds.xoffset, miewportViewBounds.bounds.yoffset};
  const glm::vec2 viewSize{miewportViewBounds.bounds.width, miewportViewBounds.bounds.height};
  const glm::vec3 worldViewNormal = helper::worldDirection(view.camera(), Directions::View::Back);
  const RenderData& renderData = appData.renderData();
  const float aspectRatio = view.camera().aspectRatio();
  const float numCheckers = static_cast<float>(renderData.m_numCheckerboardSquares);
  const glm::vec4 clipCrosshairs4 = helper::clip_T_world(view.camera()) * glm::vec4{worldCrosshairs, 1.0f};
  const glm::vec2 clipCrosshairs{clipCrosshairs4 / clipCrosshairs4.w};

  nvgLineCap(nvg, NVG_ROUND);
  nvgLineJoin(nvg, NVG_ROUND);
  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  bool isFixedImage = true;
  for (const auto& imageUid : imageUids) {
    const Image* image = appData.image(imageUid);
    if (!image || !image->settings().vectorArrowOverlayVisible()) {
      isFixedImage = false;
      continue;
    }
    if (image->header().numComponentsPerPixel() != 3u || !image->settings().globalVisibility()) {
      isFixedImage = false;
      continue;
    }

    const ImageSettings& settings = image->settings();
    const float spacingPx =
      std::clamp(vectorArrowSpacingPixels(settings, windowVP, view, *image, worldCrosshairs), 4.0f, 512.0f);
    const float lineThickness = settings.vectorArrowOverlayLineThickness();
    const float arrowOpacity = settings.vectorArrowOverlayOpacity();
    const float scaleFactor = settings.vectorArrowOverlayScaleFactor();
    const bool scaleByMagnitude = settings.vectorArrowOverlayScaleByMagnitude();
    const bool useDirectionColor = settings.vectorArrowOverlayUseDirectionColor();
    const glm::vec3 fixedColor = settings.vectorArrowOverlayColor();
    const uint32_t activeTimePoint = image->timeAxis().clamp(settings.activeTimePoint());

    const glm::mat4 subject_T_world = image->transformations().subject_T_worldDef();
    const glm::mat4 world_T_subject = image->transformations().worldDef_T_subject();
    const glm::mat4 pixel_T_subject = image->transformations().pixel_T_subject();
    const glm::mat4 subject_T_pixel = image->transformations().subject_T_pixel();
    const float maxArrowLengthPx = std::clamp(
      k_maxArrowLengthViewportFraction * std::min(viewSize.x, viewSize.y),
      k_referenceArrowLengthPx,
      k_maxArrowLengthPx);

    const auto drawSample = [&](const glm::vec2& samplePos, const glm::vec3& subjectPos, const glm::vec3& pixelPos) {
      const glm::vec2 viewClipPos = viewClip_T_miewport(windowVP, view, samplePos);
      const glm::vec2 checkerCoord = checkerCoordForViewClip(viewClipPos, numCheckers, aspectRatio);
      if (!doRenderComparisonSample(
            view.renderMode(),
            viewClipPos,
            checkerCoord,
            clipCrosshairs,
            renderData.m_quadrants,
            isFixedImage,
            aspectRatio,
            renderData.m_flashlightRadius,
            renderData.m_flashlightOverlays))
      {
        return;
      }

      const auto xValue = image->valueLinear<double>(0, pixelPos.x, pixelPos.y, pixelPos.z, activeTimePoint);
      const auto yValue = image->valueLinear<double>(1, pixelPos.x, pixelPos.y, pixelPos.z, activeTimePoint);
      const auto zValue = image->valueLinear<double>(2, pixelPos.x, pixelPos.y, pixelPos.z, activeTimePoint);
      if (!xValue || !yValue || !zValue) {
        return;
      }

      const glm::vec3 subjectVector{
        static_cast<float>(*xValue),
        static_cast<float>(*yValue),
        static_cast<float>(*zValue)};
      if (glm::length(subjectVector) <= std::numeric_limits<float>::epsilon()) {
        return;
      }

      const glm::vec3 vectorForEndpoint =
        scaleByMagnitude ? scaleFactor * subjectVector : scaleFactor * glm::normalize(subjectVector);
      const glm::vec3 worldEnd{world_T_subject * glm::vec4{subjectPos + vectorForEndpoint, 1.0f}};
      const glm::vec2 endPos =
        helper::miewport_T_world(windowVP, view.camera(), view.windowClip_T_viewClip(), worldEnd);
      if (!isFiniteVec2(endPos)) {
        return;
      }

      glm::vec2 screenDelta = endPos - samplePos;
      const float screenLength = glm::length(screenDelta);
      if (screenLength <= 2.0f) {
        return;
      }

      if (!scaleByMagnitude) {
        screenDelta =
          glm::normalize(screenDelta) * std::clamp(k_referenceArrowLengthPx * scaleFactor, 2.0f, maxArrowLengthPx);
      }

      const float finalScreenLength = glm::length(screenDelta);
      if (finalScreenLength > maxArrowLengthPx) {
        screenDelta *= maxArrowLengthPx / finalScreenLength;
      }

      const glm::vec3 arrowColor = useDirectionColor ? glm::abs(glm::normalize(subjectVector)) : fixedColor;
      drawArrow(
        nvg,
        samplePos,
        samplePos + screenDelta,
        nvgRGBAf(arrowColor.r, arrowColor.g, arrowColor.b, 0.86f * arrowOpacity * settings.globalOpacity()),
        lineThickness);
    };

    if (VectorArrowOverlaySpacingMode::Voxels == settings.vectorArrowOverlaySpacingMode()) {
      const glm::uvec3 dims = image->header().pixelDimensions();
      const glm::vec3 subjectCrosshairs{subject_T_world * glm::vec4{worldCrosshairs, 1.0f}};
      const glm::vec3 subjectViewNormal = glm::normalize(glm::mat3(subject_T_world) * worldViewNormal);
      const glm::vec3 pixelPlaneNormal = glm::transpose(glm::mat3(subject_T_pixel)) * subjectViewNormal;
      const glm::vec3 pixelCrosshairs{pixel_T_subject * glm::vec4{subjectCrosshairs, 1.0f}};
      const float planeDistance = glm::dot(pixelPlaneNormal, pixelCrosshairs);

      int sliceAxis = 0;
      if (std::abs(pixelPlaneNormal[1]) > std::abs(pixelPlaneNormal[sliceAxis])) {
        sliceAxis = 1;
      }
      if (std::abs(pixelPlaneNormal[2]) > std::abs(pixelPlaneNormal[sliceAxis])) {
        sliceAxis = 2;
      }
      if (std::abs(pixelPlaneNormal[sliceAxis]) <= std::numeric_limits<float>::epsilon()) {
        continue;
      }

      const int axis0 = (sliceAxis + 1) % 3;
      const int axis1 = (sliceAxis + 2) % 3;
      const float requestedStep = std::max(settings.vectorArrowOverlayVoxelSpacing(), 0.1f);
      const float minStepForScreenSpacing =
        std::max(requestedStep, 4.0f / std::max(screenPixelsPerVoxel(windowVP, view, *image, worldCrosshairs), 0.1f));
      const float voxelStep = std::min(minStepForScreenSpacing, 10.0f);

      for (float b = 0.0f; b < static_cast<float>(dims[axis1]); b += voxelStep) {
        for (float a = 0.0f; a < static_cast<float>(dims[axis0]); a += voxelStep) {
          glm::vec3 pixelPos{0.0f};
          pixelPos[axis0] = a;
          pixelPos[axis1] = b;
          pixelPos[sliceAxis] =
            (planeDistance - pixelPlaneNormal[axis0] * a - pixelPlaneNormal[axis1] * b) / pixelPlaneNormal[sliceAxis];
          if (
            pixelPos.x < -0.5f || pixelPos.y < -0.5f || pixelPos.z < -0.5f ||
            pixelPos.x > static_cast<float>(dims.x) - 0.5f || pixelPos.y > static_cast<float>(dims.y) - 0.5f ||
            pixelPos.z > static_cast<float>(dims.z) - 0.5f)
          {
            continue;
          }

          const glm::vec3 subjectPos{subject_T_pixel * glm::vec4{pixelPos, 1.0f}};
          const glm::vec3 worldPos{world_T_subject * glm::vec4{subjectPos, 1.0f}};
          const glm::vec2 samplePos =
            helper::miewport_T_world(windowVP, view.camera(), view.windowClip_T_viewClip(), worldPos);
          if (!isFiniteVec2(samplePos) || !isInsideRect(samplePos, viewMin, viewSize)) {
            continue;
          }

          drawSample(samplePos, subjectPos, pixelPos);
        }
      }
    }
    else {
      const float startX = viewMin.x + 0.5f * spacingPx;
      const float startY = viewMin.y + 0.5f * spacingPx;
      for (float y = startY; y < viewMin.y + viewSize.y; y += spacingPx) {
        for (float x = startX; x < viewMin.x + viewSize.x; x += spacingPx) {
          const glm::vec2 samplePos{x, y};
          const glm::vec3 worldNear =
            helper::world_T_miewport(windowVP, view.camera(), view.viewClip_T_windowClip(), samplePos);
          const float signedDistance = glm::dot(worldNear - worldCrosshairs, worldViewNormal);
          const glm::vec3 worldOnSlice = worldNear - signedDistance * worldViewNormal;
          const glm::vec3 subjectPos{subject_T_world * glm::vec4{worldOnSlice, 1.0f}};
          const glm::vec3 pixelPos{pixel_T_subject * glm::vec4{subjectPos, 1.0f}};
          drawSample(samplePos, subjectPos, pixelPos);
        }
      }
    }

    isFixedImage = false;
  }

  nvgResetScissor(nvg);
}

void drawCrosshairs(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const View& view,
  const glm::vec4& color,
  const std::array<AnatomicalLabelPosInfo, 2>& labelPosInfo)
{
  // Line segment stipple length in pixels
  static constexpr float sk_stippleLen = 8.0f;

  nvgLineCap(nvg, NVG_BUTT);
  nvgLineJoin(nvg, NVG_MITER);

  const auto& offset = view.offsetSetting();

  // Is the view offset from the crosshairs position?
  const bool viewIsOffset =
    (ViewOffsetMode::RelativeToRefImageScrolls == offset.m_offsetMode && 0 != offset.m_relativeOffsetSteps) ||
    (ViewOffsetMode::RelativeToImageScrolls == offset.m_offsetMode && 0 != offset.m_relativeOffsetSteps) ||
    (ViewOffsetMode::Absolute == offset.m_offsetMode &&
     glm::epsilonNotEqual(offset.m_absoluteOffset, 0.0f, glm::epsilon<float>()));

  if (viewIsOffset) {
    // Offset views get thinner, transparent crosshairs
    nvgStrokeWidth(nvg, 1.0f);
    nvgStrokeColor(nvg, nvgRGBAf(color.r, color.g, color.b, 0.5f * color.a));
  }
  else {
    nvgStrokeWidth(nvg, 2.0f);
    nvgStrokeColor(nvg, nvgRGBAf(color.r, color.g, color.b, color.a));
  }

  // Clip against the view bounds, even though not strictly necessary with how lines are defined
  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  for (const auto& pos : labelPosInfo) {
    if (!pos.miewportXhairPositions) {
      // Only render crosshairs when there are two intersections with the view box:
      continue;
    }

    const auto& hits = *(pos.miewportXhairPositions);

    if (ViewType::Oblique != view.viewType()) {
      // Orthogonal views get solid crosshairs:
      nvgBeginPath(nvg);
      nvgMoveTo(nvg, hits[0].x, hits[0].y);
      nvgLineTo(nvg, hits[1].x, hits[1].y);
      nvgClosePath(nvg);
      nvgStroke(nvg);
    }
    else {
      // Oblique views get stippled crosshairs:
      for (std::size_t line = 0; line < 2; ++line) {
        const auto numLines =
          static_cast<uint32_t>(glm::distance(hits[line], pos.miewportXhairCenterPos) / sk_stippleLen);

        nvgBeginPath(nvg);
        for (uint32_t i = 0; i <= numLines; ++i) {
          const float t = static_cast<float>(i) / static_cast<float>(numLines);
          const glm::vec2 p = pos.miewportXhairCenterPos + t * (hits[line] - pos.miewportXhairCenterPos);

          if (i % 2) {
            nvgLineTo(nvg, p.x, p.y); // when i odd
          }
          else {
            nvgMoveTo(nvg, p.x, p.y); // when i even
          }
        }
        nvgClosePath(nvg);
        nvgStroke(nvg);
      }
    }
  }

  nvgResetScissor(nvg);
}

void drawLightboxOffsetLabel(
  NVGcontext* nvg,
  const FrameBounds& viewportViewBounds,
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

  const glm::vec2 viewportMinCorner(viewportViewBounds.bounds.xoffset, viewportViewBounds.bounds.yoffset);
  const glm::vec2 viewportSize(viewportViewBounds.bounds.width, viewportViewBounds.bounds.height);
  const float fontSizePixels = glm::clamp(0.065f * std::min(viewportSize.x, viewportSize.y), 9.0f, 14.0f);
  const std::string label = entropy::rendering::lightbox::formatOffsetDistanceMm(offsetMm, unitReferenceLengthMm);

  nvgScissor(
    nvg,
    viewportViewBounds.viewport[0],
    viewportViewBounds.viewport[1],
    viewportViewBounds.viewport[2],
    viewportViewBounds.viewport[3]);

  nvgFontSize(nvg, fontSizePixels);
  nvgFontFace(nvg, ROBOTO_LIGHT.c_str());
  nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

  const glm::vec2 labelPos = viewportMinCorner + glm::vec2{sk_padding, sk_padding};

  nvgFontBlur(nvg, sk_shadowBlur);
  nvgFillColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, color.a));
  nvgText(nvg, labelPos.x, labelPos.y, label.c_str(), nullptr);

  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, nvgRGBAf(color.r, color.g, color.b, color.a));
  nvgText(nvg, labelPos.x, labelPos.y, label.c_str(), nullptr);

  nvgResetScissor(nvg);
}

/// @see https://community.vcvrack.com/t/advanced-nanovg-custom-label/6769/21
// void draw (const DrawArgs &args) override {
//   // draw the text
//   float bounds[4];
//   const char* txt = "DEMO";
//   nvgTextAlign(args.vg, NVG_ALIGN_MIDDLE);
//       nvgTextBounds(args.vg, 0, 0, txt, NULL, bounds);
//       nvgBeginPath(args.vg);
//   nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
//       nvgRect(args.vg, bounds[0], bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1]);
//       nvgFill(args.vg);
//   nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
//   nvgText(args.vg, 0, 0, txt, NULL);
// }
