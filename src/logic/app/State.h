#ifndef APP_STATE_H
#define APP_STATE_H

#include "common/CoordinateFrame.h"
#include "common/Types.h"

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
  AppState();
  ~AppState() = default;

  void setWorldCrosshairsPos(const glm::vec3& worldCrosshairs);
  const CoordinateFrame& worldCrosshairs() const;

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

  MouseMode m_mouseMode;            //!< Current mouse interaction mode
  ButtonState m_buttonState;        //!< Global mouse button and keyboard modifier state
  ImageSelection m_recenteringMode; //!< Image selection to use when recentering views and crosshairs

  bool m_animating; //!< Is the application currently animating something?

  CoordinateFrame m_worldCrosshairs; //!< Crosshairs coordinate frame, defined in World space
  std::optional<glm::vec3> m_worldRotationCenter; //!< Rotation center position, defined in World space

  std::optional<Annotation> m_copiedAnnotation; //!< Annotation copied to the clipboard

  std::atomic<bool> m_quitApp; //!< Flag to quit the application
};

#endif // APP_STATE_H
