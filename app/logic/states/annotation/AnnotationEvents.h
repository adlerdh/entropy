#pragma once

#include "logic/interaction/ViewHit.h"
#include "logic/interaction/events/ButtonState.h"

#include <tinyfsm.hpp>

namespace state::annot
{
/**
 * @brief Base event for annotation mouse press, release, and move transitions
 */
struct MouseEvent : public tinyfsm::Event
{
  /**
   * @brief Construct an annotation mouse event from previous and current hit information
   * @param prevHit Previous hit location
   * @param currHit Current hit location
   * @param b Mouse button state at the event
   * @param m Keyboard modifier state at the event
   * @throw Propagates exceptions from copying event payloads
   */
  MouseEvent(const ViewHit& prevHit, const ViewHit& currHit, const ButtonState& b, const ModifierState& m)
    : m_prevHit(prevHit), m_currHit(currHit), buttonState(b), modifierState(m)
  {
  }

  /**
   * @brief Destroy the event payload
   */
  virtual ~MouseEvent() = default;

  /** @brief Previous view hit information */
  const ViewHit m_prevHit;
  /** @brief Current view hit information */
  const ViewHit m_currHit;
  /** @brief Mouse button state */
  const ButtonState buttonState;
  /** @brief Keyboard modifier state */
  const ModifierState modifierState;
};

/**
 * @brief Event emitted when the mouse pointer is pressed in annotation mode
 */
struct MousePressEvent : public MouseEvent
{
  /**
   * @brief Construct a mouse press event
   * @param currHit Current hit location
   * @param b Mouse button state at the press
   * @param m Keyboard modifier state at the press
   * @throw Propagates exceptions from copying event payloads
   */
  MousePressEvent(const ViewHit& currHit, const ButtonState& b, const ModifierState& m)
    : MouseEvent(currHit, currHit, b, m)
  {
  }

  /**
   * @brief Destroy the event payload
   */
  ~MousePressEvent() override = default;
};

/**
 * @brief Event emitted when the mouse pointer is released in annotation mode
 */
struct MouseReleaseEvent : public MouseEvent
{
  /**
   * @brief Construct a mouse release event
   * @param currHit Current hit location
   * @param b Mouse button state at the release
   * @param m Keyboard modifier state at the release
   * @throw Propagates exceptions from copying event payloads
   */
  MouseReleaseEvent(const ViewHit& currHit, const ButtonState& b, const ModifierState& m)
    : MouseEvent(currHit, currHit, b, m)
  {
  }

  /**
   * @brief Destroy the event payload
   */
  ~MouseReleaseEvent() override = default;
};

/**
 * @brief Event emitted when the mouse pointer moves in annotation mode
 */
struct MouseMoveEvent : public MouseEvent
{
  /**
   * @brief Construct a mouse move event
   * @param prevHit Previous hit location
   * @param currHit Current hit location
   * @param b Mouse button state during movement
   * @param m Keyboard modifier state during movement
   * @throw Propagates exceptions from copying event payloads
   */
  MouseMoveEvent(const ViewHit& prevHit, const ViewHit& currHit, const ButtonState& b, const ModifierState& m)
    : MouseEvent(prevHit, currHit, b, m)
  {
  }

  /**
   * @brief Destroy the event payload
   */
  ~MouseMoveEvent() override = default;
};

/**
 * @brief Event emitted when annotation mode is enabled
 */
struct TurnOnAnnotationModeEvent : public tinyfsm::Event
{
};

/**
 * @brief Event emitted when annotation mode is disabled
 */
struct TurnOffAnnotationModeEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting creation of a new annotation
 */
struct CreateNewAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting completion of the annotation currently in progress
 */
struct CompleteNewAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting closure of the annotation currently in progress
 */
struct CloseNewAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting removal of the most recent vertex from the annotation in progress
 */
struct UndoVertexEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting cancellation of the annotation currently in progress
 */
struct CancelNewAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting insertion of a vertex after the selected annotation vertex
 */
struct InsertVertexEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting removal of the selected annotation vertex
 */
struct RemoveSelectedVertexEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting removal of the selected annotation
 */
struct RemoveSelectedAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting copy and removal of the selected annotation
 */
struct CutSelectedAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting copy of the selected annotation
 */
struct CopySelectedAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting paste of the copied annotation
 */
struct PasteAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting horizontal flip of the selected annotation
 */
struct HorizontallyFlipSelectedAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Event requesting vertical flip of the selected annotation
 */
struct VerticallyFlipSelectedAnnotationEvent : public tinyfsm::Event
{
};

/**
 * @brief Direction in view coordinates used when flipping an annotation polygon
 */
enum class FlipDirection
{
  /** @brief Flip left/right in the view plane */
  Horizontal,
  /** @brief Flip up/down in the view plane */
  Vertical
};
} // namespace state::annot
