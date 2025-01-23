#include "windowing/GlfwCallbacks.h"

#include "EntropyApp.h"
#include "common/Types.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/interaction/ViewHit.h"
#include "logic/interaction/events/ButtonState.h"
#include "logic/states/AnnotationStateHelpers.h"
#include "logic/states/FsmList.hpp"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <spdlog/spdlog.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <optional>

namespace
{

static ButtonState s_mouseButtonState;
static ModifierState s_modifierState;

// The last cursor position in Window space
static std::optional<ViewHit> s_prevHit;

// The start cursor position in Window space: where the cursor was clicked prior to dragging
static std::optional<ViewHit> s_startHit;

// Should zooms be synchronized for all views?
bool syncZoomsForAllViews(const ModifierState& modState)
{
  return (modState.shift) || ImGui::IsKeyDown(ImGuiKey_LeftShift)
         || ImGui::IsKeyDown(ImGuiKey_RightShift);
}

} // namespace

void errorCallback(int error, const char* description)
{
  spdlog::error("GLFW error #{}: '{}'", error, description);
}

void windowContentScaleCallback(GLFWwindow* window, float contentScaleX, float contentScaleY)
{
  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in window content scale callback");
    return;
  }

  spdlog::debug("*** windowContentScaleCallback: {}x{} ", contentScaleX, contentScaleY);

  app->windowData().setContentScaleRatios(glm::vec2{1.0f, 1.0f});
  app->imgui().setContentScale(app->windowData().getContentScaleRatio());
}

void windowCloseCallback(GLFWwindow* window)
{
  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in window close callback");
    return;
  }

  spdlog::trace("User has requested to close the application");

  // Setting this flag will show the popup on the next render iteration:
  app->guiData().m_showConfirmCloseAppPopup = true;

  // Turn off the closing flag, so that the window does not close until the user confirms 'yes'
  glfwSetWindowShouldClose(window, GLFW_FALSE);
}

void windowPositionCallback(GLFWwindow* window, int screenWindowPosX, int screenWindowPosY)
{
  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in window size callback");
    return;
  }

  // Save the window position. This does not affect rendering at all, so no render is required.
  app->windowData().setWindowPos(screenWindowPosX, screenWindowPosY);
}

void windowSizeCallback(GLFWwindow* window, int windowWidth, int windowHeight)
{
  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in window size callback");
    return;
  }

  spdlog::debug("*** windowSizeCallback: {}x{} ", windowWidth, windowHeight);

  app->resize(windowWidth, windowHeight);
  app->render();

  // The app sometimes crashes on macOS without this call
  glfwSwapBuffers(window);
}

void framebufferSizeCallback(GLFWwindow* window, int fbWidth, int fbHeight)
{
  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in framebuffer size callback");
    return;
  }

  spdlog::debug("*** framebufferSizeCallback: {}x{} ", fbWidth, fbHeight);

  app->windowData().setFramebufferSize(fbWidth, fbHeight);
  app->render();

  glfwSwapBuffers(window);
}

void cursorPosCallback(GLFWwindow* window, double mindowCursorPosX, double mindowCursorPosY)
{
  static constexpr bool inPlane = true;
  static constexpr bool outOfPlane = false;

  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in cursor position callback");
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();

  if (io.WantCaptureMouse)
  {
    // Poll events, so that the UI is responsive:
    app->glfw().setEventProcessingMode(EventProcessingMode::Poll);

    // Since ImGui has captured the event, do not send it to the app:
    return;
  }
  else if (!app->appData().state().animating())
  {
    // Mouse is not captured by the UI and the app is not animating,
    // so wait for events to save processing power:
    app->glfw().setEventProcessingMode(EventProcessingMode::Wait);
  }

  // Since modifier state is not passed to this callback
  const bool shiftDown = ImGui::IsKeyDown(ImGuiKey_LeftShift)
                         || ImGui::IsKeyDown(ImGuiKey_RightShift);

  const glm::vec2 windowCurrentPos = camera::window_T_mindow(
    static_cast<float>(app->windowData().getWindowSize().y), {mindowCursorPosX, mindowCursorPosY}
  );

  if (!s_startHit)
  {
    s_startHit = getViewHit(app->appData(), windowCurrentPos);
  }

  View* startView = s_startHit->view;
  if (!startView)
    return;

  if (!s_prevHit)
  {
    s_prevHit = getViewHit(app->appData(), windowCurrentPos);
  }

  // Compute current hit based on the transformation of the starting view.
  // This preserves continuity between previous and current coordinates.
  // It also allows the hit to be valid outside of a view.
  const auto currHit_withOverride = getViewHit(app->appData(), windowCurrentPos, s_startHit->viewUid);

  // Compute current hit without any override provided. This prevents the
  // hit from being valid if the cursor hits outside of a view.
  const auto currHit_invalidOutsideView = getViewHit(app->appData(), windowCurrentPos);

  // Send event to annotation state machine
  if (currHit_invalidOutsideView)
  {
    send_event(state::MouseMoveEvent(
      *s_prevHit, *currHit_invalidOutsideView, s_mouseButtonState, s_modifierState
    ));
  }

  CallbackHandler& H = app->callbackHandler();

  switch (app->appData().state().mouseMode())
  {
  case MouseMode::Pointer:
  {
    if (s_mouseButtonState.left)
    {
      if (!currHit_invalidOutsideView)
        break;
      H.doCrosshairsMove(*currHit_invalidOutsideView);
    }
    else if (s_mouseButtonState.right)
    {
      if (!currHit_withOverride)
        break;
      H.doCameraZoomDrag(
        *s_startHit,
        *s_prevHit,
        *currHit_withOverride,
        ZoomBehavior::ToCrosshairs,
        syncZoomsForAllViews(s_modifierState)
      );
    }
    else if (s_mouseButtonState.middle)
    {
      if (!currHit_withOverride)
        break;
      H.doCameraTranslate2d(*s_startHit, *s_prevHit, *currHit_withOverride);
    }
    break;
  }
  case MouseMode::Segment:
  {
    if (!currHit_invalidOutsideView)
      break;

    if (s_mouseButtonState.left || s_mouseButtonState.right)
    {
      if (app->appData().settings().crosshairsMoveWithBrush())
      {
        H.doCrosshairsMove(*currHit_invalidOutsideView);
      }

      const bool swapFgAndBg = (s_mouseButtonState.right);
      H.doSegment(*currHit_invalidOutsideView, swapFgAndBg);
    }

    break;
  }
  case MouseMode::Annotate:
  {
    if (!currHit_invalidOutsideView)
      break;

    if (s_mouseButtonState.left)
    {
      if (app->appData().settings().crosshairsMoveWhileAnnotating() && state::isInStateWhereCrosshairsCanMove())
      {
        H.doCrosshairsMove(*currHit_invalidOutsideView);
      }
    }
    else if (s_mouseButtonState.right)
    {
      if (!currHit_withOverride)
        break;
      H.doCameraZoomDrag(
        *s_startHit,
        *s_prevHit,
        *currHit_withOverride,
        ZoomBehavior::ToCrosshairs,
        syncZoomsForAllViews(s_modifierState)
      );
    }
    else if (s_mouseButtonState.middle)
    {
      if (!currHit_withOverride)
        break;
      H.doCameraTranslate2d(*s_startHit, *s_prevHit, *currHit_withOverride);
    }
    break;
  }
  case MouseMode::WindowLevel:
  {
    if (!currHit_withOverride)
      break;

    const bool fineAdjustment = shiftDown;

    if (s_mouseButtonState.left)
    {
      H.doWindowLevel(*s_startHit, *s_prevHit, *currHit_withOverride, fineAdjustment);
    }
    else if (s_mouseButtonState.right)
    {
      H.doOpacity(*s_prevHit, *currHit_withOverride);
    }
    break;
  }
  case MouseMode::CameraZoom:
  {
    if (!currHit_withOverride)
      break;

    if (s_mouseButtonState.left)
    {
      H.doCameraZoomDrag(
        *s_startHit,
        *s_prevHit,
        *currHit_withOverride,
        ZoomBehavior::ToCrosshairs,
        syncZoomsForAllViews(s_modifierState)
      );
    }
    else if (s_mouseButtonState.right)
    {
      H.doCameraZoomDrag(
        *s_startHit,
        *s_prevHit,
        *currHit_withOverride,
        ZoomBehavior::ToStartPosition,
        syncZoomsForAllViews(s_modifierState)
      );
    }
    else if (s_mouseButtonState.middle)
    {
      H.doCameraTranslate2d(*s_startHit, *s_prevHit, *currHit_withOverride);
    }
    break;
  }
  case MouseMode::CameraTranslate:
  {
    if (!currHit_withOverride)
      break;

    if (s_mouseButtonState.left)
    {
      H.doCameraTranslate2d(*s_startHit, *s_prevHit, *currHit_withOverride);
    }
    else if (s_mouseButtonState.right)
    {
      // do 3D translate
    }
    break;
  }
  case MouseMode::CameraRotate:
  {
    if (!currHit_withOverride)
      break;

    switch (startView->viewType())
    {
    case ViewType::Oblique:
    {
      if (s_mouseButtonState.left)
      {
        H.doCameraRotate2d(*s_startHit, *s_prevHit, *currHit_withOverride, RotationOrigin::ViewCenter);
      }
      else if (s_mouseButtonState.right)
      {
        // Depending on which mouse button key modifier is held, a different axis constraint is
        // applied to the 3D camera rotation.

        if (s_modifierState.shift)
        {
          H.doCameraRotate3d(
            *s_startHit,
            *s_prevHit,
            *currHit_withOverride,
            RotationOrigin::Crosshairs,
            AxisConstraint::X
          );
        }
        else if (s_modifierState.control)
        {
          H.doCameraRotate3d(
            *s_startHit,
            *s_prevHit,
            *currHit_withOverride,
            RotationOrigin::Crosshairs,
            AxisConstraint::Y
          );
        }
        else if (s_modifierState.alt)
        {
          H.doCameraRotate2d(
            *s_startHit, *s_prevHit, *currHit_withOverride, RotationOrigin::Crosshairs
          );
        }
        else
        {
          H.doCameraRotate3d(
            *s_startHit,
            *s_prevHit,
            *currHit_withOverride,
            RotationOrigin::Crosshairs,
            AxisConstraint::None
          );
        }
      }
      break;
    }
    case ViewType::ThreeD:
    {
      if (s_mouseButtonState.left)
      {
        if (s_modifierState.alt)
        {
          H.doCameraRotate2d(
            *s_startHit, *s_prevHit, *currHit_withOverride, RotationOrigin::Crosshairs
          );
        }
        else
        {
          H.doCameraRotate3d(
            *s_startHit,
            *s_prevHit,
            *currHit_withOverride,
            RotationOrigin::Crosshairs,
            AxisConstraint::None
          );
        }
      }
      else if (s_mouseButtonState.right)
      {
        if (s_modifierState.alt)
        {
          H.doCameraRotate2d(
            *s_startHit, *s_prevHit, *currHit_withOverride, RotationOrigin::CameraEye
          );
        }
        else
        {
          H.doCameraRotate3d(
            *s_startHit,
            *s_prevHit,
            *currHit_withOverride,
            RotationOrigin::CameraEye,
            AxisConstraint::None
          );
        }
      }
      break;
    }
    default:
      break;
    }

    break;
  }
  case MouseMode::ImageTranslate:
  {
    if (!currHit_withOverride)
      break;

    if (s_mouseButtonState.left)
    {
      H.doImageTranslate(*s_startHit, *s_prevHit, *currHit_withOverride, inPlane);
    }
    else if (s_mouseButtonState.right)
    {
      H.doImageTranslate(*s_startHit, *s_prevHit, *currHit_withOverride, outOfPlane);
    }
    break;
  }
  case MouseMode::ImageRotate:
  {
    if (!currHit_withOverride)
      break;

    if (s_mouseButtonState.left)
    {
      H.doImageRotate(*s_startHit, *s_prevHit, *currHit_withOverride, inPlane);
    }
    else if (s_mouseButtonState.right)
    {
      H.doImageRotate(*s_startHit, *s_prevHit, *currHit_withOverride, outOfPlane);
    }
    break;
  }
  case MouseMode::ImageScale:
  {
    if (!currHit_withOverride)
      break;

    if (s_mouseButtonState.left)
    {
      const bool constrainIsotropic = s_modifierState.shift;
      H.doImageScale(*s_startHit, *s_prevHit, *currHit_withOverride, constrainIsotropic);
    }
    break;
  }
  }

  s_prevHit = currHit_withOverride;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in mouse button callback");
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return; // ImGui has captured event

  // Update button state
  s_mouseButtonState.updateFromGlfwEvent(button, action);
  s_modifierState.updateFromGlfwEvent(mods);

  // Reset start and previous hits
  s_startHit = std::nullopt;
  s_prevHit = std::nullopt;

  double mindowCursorPosX, mindowCursorPosY;
  glfwGetCursorPos(window, &mindowCursorPosX, &mindowCursorPosY);

  const glm::vec2 windowCursorPos = camera::window_T_mindow(
    static_cast<float>(app->windowData().getWindowSize().y), {mindowCursorPosX, mindowCursorPosY}
  );

  // Get a hit that will be invalid (null) if the cursor is not in any view:
  const auto hit_invalidOutsideView = getViewHit(app->appData(), windowCursorPos);
  if (!hit_invalidOutsideView)
    return;

  // Send event to the annotation state machine
  switch (action)
  {
  case GLFW_PRESS:
  {
    send_event(state::MousePressEvent(*hit_invalidOutsideView, s_mouseButtonState, s_modifierState));
    break;
  }
  case GLFW_RELEASE:
  {
    app->appData().windowData().setActiveViewUid(std::nullopt);

    send_event(state::MouseReleaseEvent(*hit_invalidOutsideView, s_mouseButtonState, s_modifierState)
    );
    break;
  }
  default:
    break;
  }

  // Trigger the cursor position callback
  cursorPosCallback(window, mindowCursorPosX, mindowCursorPosY);
}

void scrollCallback(GLFWwindow* window, double scrollOffsetX, double scrollOffsetY)
{
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return; // ImGui has captured event

  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in scroll callback");
    return;
  }

  const bool shiftDown = ImGui::IsKeyDown(ImGuiKey_LeftShift)
                         || ImGui::IsKeyDown(ImGuiKey_RightShift);

  double mindowCursorPosX, mindowCursorPosY;
  glfwGetCursorPos(window, &mindowCursorPosX, &mindowCursorPosY);
  cursorPosCallback(window, mindowCursorPosX, mindowCursorPosY);

  const glm::vec2 windowCursorPos = camera::window_T_mindow(
    static_cast<float>(app->windowData().getWindowSize().y), {mindowCursorPosX, mindowCursorPosY}
  );

  const auto hit_invalidOutsideView = getViewHit(app->appData(), windowCursorPos);
  if (!hit_invalidOutsideView)
    return;

  CallbackHandler& H = app->callbackHandler();

  switch (app->appData().state().mouseMode())
  {
  case MouseMode::Pointer:
  case MouseMode::Segment:
  case MouseMode::CameraTranslate:
  case MouseMode::CameraRotate:
  case MouseMode::ImageRotate:
  case MouseMode::ImageTranslate:
  case MouseMode::ImageScale:
  case MouseMode::WindowLevel:
  {
    const bool fineScroll = shiftDown; // ( s_modifierState.shift );
    H.doCrosshairsScroll(*hit_invalidOutsideView, {scrollOffsetX, scrollOffsetY}, fineScroll);
    break;
  }
  case MouseMode::CameraZoom:
  {
    H.doCameraZoomScroll(
      *hit_invalidOutsideView,
      {scrollOffsetX, scrollOffsetY},
      ZoomBehavior::ToCrosshairs,
      syncZoomsForAllViews(s_modifierState)
    );
    break;
  }
  case MouseMode::Annotate:
  {
    if (state::isInStateWhereViewsCanScroll())
    {
      const bool fineScroll = shiftDown; // ( s_modifierState.shift );
      H.doCrosshairsScroll(*hit_invalidOutsideView, {scrollOffsetX, scrollOffsetY}, fineScroll);
    }

    break;
  }
  }
}

void keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int mods)
{
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureKeyboard)
    return; // ImGui has captured event

  s_modifierState.updateFromGlfwEvent(mods);

  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));

  if (!app)
  {
    spdlog::warn("App is null in key callback");
    return;
  }

  // Do actions on GLFW_PRESS and GLFW_REPEAT only
  if (GLFW_RELEASE == action)
    return;

  double mindowCursorPosX, mindowCursorPosY;
  glfwGetCursorPos(window, &mindowCursorPosX, &mindowCursorPosY);

  const glm::vec2 windowCursorPos = camera::window_T_mindow(
    static_cast<float>(app->windowData().getWindowSize().y), {mindowCursorPosX, mindowCursorPosY}
  );

  const auto hit_invalidOutsideView = getViewHit(app->appData(), windowCursorPos);

  CallbackHandler& H = app->callbackHandler();

  switch (key)
  {
  case GLFW_KEY_Q:
  {
    if (s_modifierState.control)
    {
      glfwSetWindowShouldClose(window, true);
    }
    break;
  }

  case GLFW_KEY_V:
    H.setMouseMode(MouseMode::Pointer);
    break;
  case GLFW_KEY_B:
    H.setMouseMode(MouseMode::Segment);
    break;
  case GLFW_KEY_L:
    H.setMouseMode(MouseMode::WindowLevel);
    break;

  case GLFW_KEY_R:
    H.setMouseMode(MouseMode::ImageRotate);
    break;
  case GLFW_KEY_T:
    H.setMouseMode(MouseMode::ImageTranslate);
    break;
    //    case GLFW_KEY_Y: H.setMouseMode( MouseMode::ImageScale ); break;

  case GLFW_KEY_Z:
    H.setMouseMode(MouseMode::CameraZoom);
    break;
  case GLFW_KEY_X:
    H.setMouseMode(MouseMode::CameraTranslate);
    break;

  case GLFW_KEY_A:
    H.decreaseSegOpacity();
    break;
  case GLFW_KEY_S:
    H.toggleSegVisibility();
    break;
  case GLFW_KEY_D:
    H.increaseSegOpacity();
    break;

  case GLFW_KEY_W:
    H.toggleImageVisibility();
    break;
  case GLFW_KEY_E:
    H.toggleImageEdges();
    break;
  case GLFW_KEY_O:
    H.cycleOverlayAndUiVisibility();
    break;

  case GLFW_KEY_C:
  {
    // Shift does a "hard" reset of the crosshairs, oblique orientations, and zoom
    const bool hardReset = (s_modifierState.shift);
    const bool recenterCrosshairs = hardReset;
    const bool resetObliqueOrientation = hardReset;
    const bool recenterOnCurrentCrosshairsPosition = true;

    std::optional<bool> resetZoom = std::nullopt;

    if (hardReset)
    {
      resetZoom = true;
    }

    H.recenterViews(
      app->appData().state().recenteringMode(),
      recenterCrosshairs,
      recenterOnCurrentCrosshairsPosition,
      resetObliqueOrientation,
      resetZoom
    );
    break;
  }

  case GLFW_KEY_F4:
    H.toggleFullScreenMode();
    break;
  case GLFW_KEY_ESCAPE:
    H.toggleFullScreenMode(true);
    break;

  case GLFW_KEY_PAGE_DOWN:
  {
    if (s_modifierState.shift)
    {
      H.cycleImageComponent(-1);
    }
    else
    {
      if (!hit_invalidOutsideView)
        break;
      H.scrollViewSlice(*hit_invalidOutsideView, -1);
    }
    break;
  }
  case GLFW_KEY_PAGE_UP:
  {
    if (s_modifierState.shift)
    {
      H.cycleImageComponent(1);
    }
    else
    {
      if (!hit_invalidOutsideView)
        break;
      H.scrollViewSlice(*hit_invalidOutsideView, 1);
    }
    break;
  }
  case GLFW_KEY_LEFT:
  {
    if (!hit_invalidOutsideView)
      break;
    H.moveCrosshairsOnViewSlice(*hit_invalidOutsideView, -1, 0);
    break;
  }
  case GLFW_KEY_RIGHT:
  {
    if (!hit_invalidOutsideView)
      break;
    H.moveCrosshairsOnViewSlice(*hit_invalidOutsideView, 1, 0);
    break;
  }
  case GLFW_KEY_UP:
  {
    if (!hit_invalidOutsideView)
      break;
    H.moveCrosshairsOnViewSlice(*hit_invalidOutsideView, 0, 1);
    break;
  }
  case GLFW_KEY_DOWN:
  {
    if (!hit_invalidOutsideView)
      break;
    H.moveCrosshairsOnViewSlice(*hit_invalidOutsideView, 0, -1);
    break;
  }

  case GLFW_KEY_LEFT_BRACKET:
  {
    if (s_modifierState.shift)
    {
      H.cycleActiveImage(-1);
    }
    else
    {
      H.cyclePrevLayout();
    }
    break;
  }
  case GLFW_KEY_RIGHT_BRACKET:
  {
    if (s_modifierState.shift)
    {
      H.cycleActiveImage(1);
    }
    else
    {
      H.cycleNextLayout();
    }
    break;
  }

  case GLFW_KEY_COMMA:
  {
    if (s_modifierState.shift)
    {
      H.cycleBackgroundSegLabel(-1);
    }
    else
    {
      H.cycleForegroundSegLabel(-1);
    }
    break;
  }
  case GLFW_KEY_PERIOD:
  {
    if (s_modifierState.shift)
    {
      H.cycleBackgroundSegLabel(1);
    }
    else
    {
      H.cycleForegroundSegLabel(1);
    }
    break;
  }

  case GLFW_KEY_KP_ADD:
  case GLFW_KEY_EQUAL:
  {
    H.cycleBrushSize(1);
    break;
  }

  case GLFW_KEY_KP_SUBTRACT:
  case GLFW_KEY_MINUS:
  {
    H.cycleBrushSize(-1);
    break;
  }

    /*
        case GLFW_KEY_ENTER:
        {
            const auto mouseMode = app->appData().state().mouseMode();

            if ( MouseMode::Pointer == mouseMode ||
                 MouseMode::ImageRotate == mouseMode ||
                 MouseMode::ImageScale == mouseMode )
            {
                app->appData().state().setWorldRotationCenter(
                            app->appData().state().worldCrosshairs().worldOrigin() );
            }
            break;
        }

        case GLFW_KEY_SPACE:
        {
            const auto mouseMode = app->appData().state().mouseMode();

            if ( MouseMode::Pointer == mouseMode ||
                 MouseMode::ImageRotate == mouseMode ||
                 MouseMode::ImageScale == mouseMode )
            {
                app->appData().state().setWorldRotationCenter( std::nullopt );
            }
            break;
        }
        */

    /*
        case GLFW_KEY_C:
        {
            if ( GLFW_MOD_CONTROL & mods )
            {
                glfwSetClipboardString( nullptr, "A string with words in it" );
            }
            break;
        }

        case GLFW_KEY_V:
        {
            if ( GLFW_MOD_CONTROL & mods )
            {
                const char* text = glfwGetClipboardString( nullptr );
            }
            break;
        }
        */
  }
}

void dropCallback(GLFWwindow* window, int count, const char** paths)
{
  if (0 == count || !paths)
    return;

  auto app = reinterpret_cast<EntropyApp*>(glfwGetWindowUserPointer(window));
  if (!app)
  {
    spdlog::warn("App is null in drop callback");
    return;
  }

  for (int i = 0; i < count; ++i)
  {
    if (paths[i])
    {
      spdlog::info("Dropped file {}: {}", i, paths[i]);

      serialize::Image serializedImage;
      serializedImage.m_imageFileName = paths[i];

      const bool isReference = false;
      app->loadSerializedImage(serializedImage, isReference);
    }
  }
}
