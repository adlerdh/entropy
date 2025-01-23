#ifndef ANNOTATION_STATES_H
#define ANNOTATION_STATES_H

#include "logic/states/AnnotationStateMachine.h"

namespace state
{

/// @todo Create AnnotatingState: { Nothing, PickingPoint, MovingPoint, SelectingPoint }

/**
 * @brief State where annotating is turned off.
 */
class AnnotationOffState : public AnnotationStateMachine
{
  void entry() override;

  void react(const TurnOnAnnotationModeEvent&) override;
};

/**
 * @brief State where the user has turned annotating on,
 * but no view has yet been selected in which to annotate.
 */
class ViewBeingSelectedState : public AnnotationStateMachine
{
  void entry() override;

  void react(const MousePressEvent&) override;
  void react(const MouseMoveEvent&) override;
  void react(const MouseReleaseEvent&) override;

  void react(const TurnOffAnnotationModeEvent&) override;
};

/**
 * @brief State where the user has turned annotating on
 * and has also selected a view in which to perform annotation.
 * They are ready to either edit existing annotations or to
 * create a new annotation.
 */
class StandbyState : public AnnotationStateMachine
{
  void entry() override;
  void exit() override;

  void react(const MousePressEvent&) override;
  void react(const MouseMoveEvent&) override;
  void react(const MouseReleaseEvent&) override;

  void react(const TurnOffAnnotationModeEvent&) override;
  void react(const CreateNewAnnotationEvent&) override;
  void react(const RemoveSelectedAnnotationEvent&) override;
  void react(const CutSelectedAnnotationEvent&) override;
  void react(const CopySelectedAnnotationEvent&) override;
  void react(const PasteAnnotationEvent&) override;
  void react(const HorizontallyFlipSelectedAnnotationEvent&) override;
  void react(const VerticallyFlipSelectedAnnotationEvent&) override;
};

/**
 * @brief State where the user has decided to create a new annotation.
 */
class CreatingNewAnnotationState : public AnnotationStateMachine
{
  void entry() override;
  void exit() override;

  void react(const MousePressEvent&) override;
  void react(const MouseMoveEvent&) override;
  void react(const MouseReleaseEvent&) override;

  void react(const TurnOffAnnotationModeEvent&) override;
  void react(const CompleteNewAnnotationEvent&) override;
  void react(const CancelNewAnnotationEvent&) override;
};

/**
 * @brief State where the user is adding vertices to the new annotation.
 */
class AddingVertexToNewAnnotationState : public AnnotationStateMachine
{
  void entry() override;
  void exit() override;

  void react(const MousePressEvent&) override;
  void react(const MouseMoveEvent&) override;
  void react(const MouseReleaseEvent&) override;

  void react(const TurnOffAnnotationModeEvent&) override;
  void react(const CompleteNewAnnotationEvent&) override;
  void react(const CloseNewAnnotationEvent&) override;
  void react(const UndoVertexEvent&) override;
  void react(const CancelNewAnnotationEvent&) override;
};

/**
 * @brief State where the user has selected a vertex
 */
class VertexSelectedState : public AnnotationStateMachine
{
  void entry() override;
  void exit() override;

  void react(const MousePressEvent&) override;
  void react(const MouseReleaseEvent&) override;
  void react(const MouseMoveEvent&) override;

  void react(const TurnOffAnnotationModeEvent&) override;
  void react(const InsertVertexEvent&) override;
  void react(const RemoveSelectedVertexEvent&) override;
  void react(const RemoveSelectedAnnotationEvent&) override;
  void react(const CutSelectedAnnotationEvent&) override;
  void react(const CopySelectedAnnotationEvent&) override;
  void react(const PasteAnnotationEvent&) override;
};

} // namespace state

#endif // ANNOTATION_STATES_H
