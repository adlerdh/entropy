#include "logic/app/State.h"
//#include "logic/ipc/IPCMessage.h"
#include "logic/states/FsmList.hpp"

AppState::AppState()
  : m_mouseMode(MouseMode::Pointer)
  , m_buttonState()
  , m_recenteringMode(ImageSelection::AllLoadedImages)
  , m_animating(false)
  , m_worldCrosshairs()
  , m_worldRotationCenter(std::nullopt)
  , m_copiedAnnotation(std::nullopt)
  , m_quitApp(false)
//m_ipcHandler()
{
}

void AppState::setWorldRotationCenter(const std::optional<glm::vec3>& worldRotationCenter)
{
  m_worldRotationCenter = *worldRotationCenter;
}

glm::vec3 AppState::worldRotationCenter() const
{
  if (m_worldRotationCenter)
  {
    return *m_worldRotationCenter;
  }

  return m_worldCrosshairs.worldOrigin();
}

void AppState::setWorldCrosshairsPos(const glm::vec3& worldCrosshairsPos)
{
  m_worldCrosshairs.setWorldOrigin(worldCrosshairsPos);
  //    broadcastCrosshairsPosition();
}

const CoordinateFrame& AppState::worldCrosshairs() const
{
  return m_worldCrosshairs;
}

void AppState::setMouseMode(MouseMode mode)
{
  const MouseMode oldMode = m_mouseMode;

  m_mouseMode = mode;

  if (MouseMode::Annotate == oldMode && MouseMode::Annotate != mode)
  {
    send_event(state::TurnOffAnnotationModeEvent());
  }
  else if (MouseMode::Annotate != oldMode && MouseMode::Annotate == mode)
  {
    send_event(state::TurnOnAnnotationModeEvent());
  }
}

MouseMode AppState::mouseMode() const
{
  return m_mouseMode;
}

void AppState::setButtonState(ButtonState state)
{
  m_buttonState = state;
}
ButtonState AppState::buttonState() const
{
  return m_buttonState;
}

void AppState::setRecenteringMode(ImageSelection mode)
{
  m_recenteringMode = mode;
}
ImageSelection AppState::recenteringMode() const
{
  return m_recenteringMode;
}

void AppState::setAnimating(bool set)
{
  m_animating = set;
}
bool AppState::animating() const
{
  return m_animating;
}

void AppState::setCopiedAnnotation(const Annotation& annot)
{
  m_copiedAnnotation = annot;
}
void AppState::clearCopiedAnnotation()
{
  m_copiedAnnotation = std::nullopt;
}
const std::optional<Annotation>& AppState::getCopiedAnnotation() const
{
  return m_copiedAnnotation;
}

void AppState::setQuitApp(bool quit)
{
  m_quitApp = quit;
}
bool AppState::quitApp() const
{
  return m_quitApp;
}

/*
void AppState::broadcastCrosshairsPosition()
{
    static const glm::vec3 ZERO( 0.0f );
    static const glm::vec3 ONE( 1.0f );

    // We need a reference image to broadcast the crosshairs
    const auto imgUid = refImageUid();
    if ( ! imgUid ) return;

    const Image* refImg = image( *imgUid );
    if ( ! refImg ) return;

    const auto& refTx = refImg->transformations();

    // Convert World to reference Subject and texture positions:
    glm::vec4 refSubjectPos = refTx.subject_T_worldDef() * glm::vec4{ worldCrosshairs().worldOrigin(), 1.0f };
    glm::vec4 refTexturePos = refTx.texture_T_subject() * refSubjectPos;

    refSubjectPos /= refSubjectPos.w;
    refTexturePos /= refTexturePos.w;

    if ( glm::any( glm::lessThan( glm::vec3{refTexturePos}, ZERO ) ) ||
         glm::any( glm::greaterThan( glm::vec3{refTexturePos}, ONE ) ) )
    {
        return;
    }

    // Read the contents of shared memory into the local message object
    IPCMessage message;
    m_ipcHandler.Read( static_cast<void*>( &message ) );

    // Convert LPS to RAS
    message.cursor[0] = -refSubjectPos[0];
    message.cursor[1] = -refSubjectPos[1];
    message.cursor[2] = refSubjectPos[2];

    if ( ! m_ipcHandler.Broadcast( static_cast<void*>( &message ) ) )
    {
        spdlog::debug( "Failed to broadcast IPC to ITK-SNAP" );
    }
}
*/
