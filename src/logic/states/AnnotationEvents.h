#ifndef ANNOTATION_EVENTS_H
#define ANNOTATION_EVENTS_H

#include "logic/interaction/ViewHit.h"
#include "logic/interaction/events/ButtonState.h"

#include <tinyfsm.hpp>

namespace state
{

/// MouseEvent is a base class for mouse press, release, and move events.
struct MouseEvent : public tinyfsm::Event
{
  MouseEvent(
    const ViewHit& prevHit, const ViewHit& currHit, const ButtonState& b, const ModifierState& m
  )
    : m_prevHit(prevHit)
    , m_currHit(currHit)
    , buttonState(b)
    , modifierState(m)
  {
  }

  virtual ~MouseEvent() = default;

  const ViewHit m_prevHit;           //!< Previous view hit information
  const ViewHit m_currHit;           //!< Current view hit information
  const ButtonState buttonState;     //!< Mouse button state
  const ModifierState modifierState; //!< Keyboard modifier state
};

/// Mouse pointer pressed
struct MousePressEvent : public MouseEvent
{
  MousePressEvent(const ViewHit& currHit, const ButtonState& b, const ModifierState& m)
    : MouseEvent(currHit, currHit, b, m)
  {
  }

  ~MousePressEvent() override = default;
};

/// Mouse pointer released
struct MouseReleaseEvent : public MouseEvent
{
  MouseReleaseEvent(const ViewHit& currHit, const ButtonState& b, const ModifierState& m)
    : MouseEvent(currHit, currHit, b, m)
  {
  }

  ~MouseReleaseEvent() override = default;
};

/// Mouse pointer moved
struct MouseMoveEvent : public MouseEvent
{
  MouseMoveEvent(
    const ViewHit& prevHit, const ViewHit& currHit, const ButtonState& b, const ModifierState& m
  )
    : MouseEvent(prevHit, currHit, b, m)
  {
  }

  ~MouseMoveEvent() override = default;
};

/// User has turned on annotation mode: they want to create or edit annotations
struct TurnOnAnnotationModeEvent : public tinyfsm::Event
{
};

/// User has turned off annotation mode: they want to stop annotating
struct TurnOffAnnotationModeEvent : public tinyfsm::Event
{
};

/// User wants to create a new annotation
struct CreateNewAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to complete the new annotation that is currently in progress
struct CompleteNewAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to close the new annotation that is currently in progress
struct CloseNewAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to undo the last annotation vertex that was created for the current
/// annotation in progress
struct UndoVertexEvent : public tinyfsm::Event
{
};

/// User wants to cancel creating the new annotation that is currently in progress
struct CancelNewAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to insert a new vertex following the currently selected annotation vertex
struct InsertVertexEvent : public tinyfsm::Event
{
};

/// User wants to remove the currently selected annotation vertex
struct RemoveSelectedVertexEvent : public tinyfsm::Event
{
};

/// User wants to remove the currently selected annotation
struct RemoveSelectedAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to cut (copy + remove) the selected annotation
struct CutSelectedAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to copy the selected annotation to the clipboard
struct CopySelectedAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to paste the selected annotation from the clipboard
struct PasteAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to horizontally flip the selected annotation
struct HorizontallyFlipSelectedAnnotationEvent : public tinyfsm::Event
{
};

/// User wants to horizontally flip the selected annotation
struct VerticallyFlipSelectedAnnotationEvent : public tinyfsm::Event
{
};

/// Defines the direction (in the view) in which to flip the annotation polygon
enum class FlipDirection
{
  Horizontal,
  Vertical
};

} // namespace state

#endif // ANNOTATION_EVENTS_H
