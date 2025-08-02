#pragma once

#include "common/CoordinateFrame.h"
#include "common/Types.h"

#include "logic/app/CrosshairsState.h"
#include "logic/annotation/Annotation.h"
#include "logic/interaction/events/ButtonState.h"
//#include "logic/ipc/IPCHandler.h"

#include <glm/vec3.hpp>
#include <uuid.h>

#include <atomic>
#include <optional>

/**
 * @brief Collection of application state that changes through its execution.
 */
class AppState
{
public:
  AppState() = default;
  ~AppState() = default;

  void setWorldCrosshairsPos(const glm::vec3& worldPos);
  void setWorldCrosshairs(CoordinateFrame worldCrosshairs);

  const CoordinateFrame& worldCrosshairs() const;
  const CrosshairsState& crosshairsState() const;

  /// Saves current crosshairs position to "old"
  void saveOldCrosshairs();

  /// Set UID of view using the old crosshairs.
  /// std::nullopt to clear it
  void setViewUsingOldCrosshairs(const std::optional<uuids::uuid>& viewUid);

  void setWorldRotationCenter(const std::optional<glm::vec3>& worldRotationCenter);

  /**
     * @brief Get the rotation center in World space. If no rotation has been explicitly set,
     * then it defaults to the crosshairs origin position.
     * @return Rotation center (World space)
     */
  glm::vec3 worldRotationCenter() const;

  void setMouseMode(MouseMode mode);
  MouseMode mouseMode() const;

  void setButtonState(ButtonState);
  ButtonState buttonState() const;

  void setRecenteringMode(ImageSelection);
  ImageSelection recenteringMode() const;

  void setAnimating(bool set);
  bool animating() const;

  void setCopiedAnnotation(const Annotation& annot);
  void clearCopiedAnnotation();
  const std::optional<Annotation>& getCopiedAnnotation() const;

  void setQuitApp(bool quit);
  bool quitApp() const;

private:
  // void broadcastCrosshairsPosition();
  // IPCHandler m_ipcHandler;

  MouseMode m_mouseMode{MouseMode::Pointer}; //!< Current mouse interaction mode
  ButtonState m_buttonState; //!< Global mouse button and keyboard modifier state

  /// Image selection to use when recentering views and crosshairs
  ImageSelection m_recenteringMode{ImageSelection::AllLoadedImages};

  bool m_animating{false}; //!< Is the application currently animating something?

  CrosshairsState m_crosshairsState;

  /// Rotation center position, defined in World space
  std::optional<glm::vec3> m_worldRotationCenter{std::nullopt};

  /// Annotation copied to the clipboard
  std::optional<Annotation> m_copiedAnnotation{std::nullopt};

  std::atomic<bool> m_quitApp{false}; //!< Flag to quit the application
};
