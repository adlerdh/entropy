#include "logic/states/AnnotationStates.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace state
{

void AnnotationOffState::entry()
{
  if (!ms_appData)
  {
    // The AppData pointer has not yet been set
    return;
  }

  ms_hoveredViewUid = std::nullopt;
  ms_selectedViewUid = std::nullopt;
  ms_growingAnnotUid = std::nullopt;

  unhoverAnnotation();
  deselect(true, false);
}

void AnnotationOffState::react(const TurnOnAnnotationModeEvent&)
{
  transit<ViewBeingSelectedState>();
}

/**************** ViewBeingSelectedState *******************/

void ViewBeingSelectedState::entry()
{
  ms_hoveredViewUid = std::nullopt;
  ms_selectedViewUid = std::nullopt;
  ms_growingAnnotUid = std::nullopt;
  unhoverAnnotation();
}

void ViewBeingSelectedState::react(const MousePressEvent& e)
{
  if (!selectView(e.m_currHit))
    return;
  transit<StandbyState>();

  /// @note If this is not call, the UI may not update until the next mouse event following the
  /// \c MousePressEvent that should trigger the UI change
  if (ms_renderUiCallback)
    ms_renderUiCallback();
}

void ViewBeingSelectedState::react(const MouseMoveEvent& e)
{
  hoverView(e.m_currHit);
}

void ViewBeingSelectedState::react(const MouseReleaseEvent&) {}

void ViewBeingSelectedState::react(const TurnOffAnnotationModeEvent&)
{
  transit<AnnotationOffState>();
}

/**************** StandbyState *******************/

void StandbyState::entry()
{
  if (!ms_selectedViewUid)
  {
    spdlog::error("Entered StandbyState without a selected view");
    transit<ViewBeingSelectedState>();
    return;
  }

  ms_growingAnnotUid = std::nullopt;
  unhoverAnnotation();
}

void StandbyState::exit() {}

void StandbyState::react(const MousePressEvent& e)
{
  if (!selectView(e.m_currHit))
    return;

  if (e.buttonState.left)
  {
    if (selectAnnotationAndVertex(e.m_currHit))
    {
      transit<VertexSelectedState>();
    }
    else
    {
      selectAnnotation(e.m_currHit);
    }
  }

  /// @note If this is not call, the UI may not update until the next mouse event following the
  /// \c MousePressEvent that should trigger the UI change
  if (ms_renderUiCallback)
    ms_renderUiCallback();
}

void StandbyState::react(const MouseReleaseEvent& /*e*/) {}

void StandbyState::react(const MouseMoveEvent& e)
{
  hoverView(e.m_currHit);
  hoverAnnotationAndVertex(e.m_currHit);

  if (e.buttonState.left)
  {
    moveSelectedPolygon(e.m_prevHit, e.m_currHit);
  }
}

void StandbyState::react(const TurnOffAnnotationModeEvent&)
{
  transit<AnnotationOffState>();
}

void StandbyState::react(const CreateNewAnnotationEvent&)
{
  transit<CreatingNewAnnotationState>();
}

void StandbyState::react(const RemoveSelectedAnnotationEvent&)
{
  removeSelectedPolygon();
}

void StandbyState::react(const CutSelectedAnnotationEvent&)
{
  cutSelectedAnnotation();
}

void StandbyState::react(const CopySelectedAnnotationEvent&)
{
  copySelectedAnnotation();
}

void StandbyState::react(const PasteAnnotationEvent&)
{
  pasteAnnotation();
}

void StandbyState::react(const HorizontallyFlipSelectedAnnotationEvent&)
{
  flipSelectedAnnotation(FlipDirection::Horizontal);
}

void StandbyState::react(const VerticallyFlipSelectedAnnotationEvent&)
{
  flipSelectedAnnotation(FlipDirection::Vertical);
}

/**************** CreatingNewAnnotationState *******************/

void CreatingNewAnnotationState::entry()
{
  if (!ms_selectedViewUid)
  {
    spdlog::error("Attempting to create a new annotation without a selected view");
    transit<ViewBeingSelectedState>();
    return;
  }

  ms_growingAnnotUid = std::nullopt;
  unhoverAnnotation();
  deselect(true, true);
}

void CreatingNewAnnotationState::exit() {}

void CreatingNewAnnotationState::react(const MousePressEvent& e)
{
  if (e.buttonState.left)
  {
    if (createNewGrowingPolygon(e.m_currHit) && addVertexToGrowingPolygon(e.m_currHit))
    {
      transit<AddingVertexToNewAnnotationState>();
    }
  }

  /// @note If this is not call, the UI may not update until the next mouse event following the
  /// \c MousePressEvent that should trigger the UI change
  if (ms_renderUiCallback)
    ms_renderUiCallback();
}

void CreatingNewAnnotationState::react(const MouseMoveEvent& e)
{
  hoverAnnotationAndVertex(e.m_currHit);
}

void CreatingNewAnnotationState::react(const MouseReleaseEvent& /*e*/) {}

void CreatingNewAnnotationState::react(const TurnOffAnnotationModeEvent&)
{
  transit<AnnotationOffState>();
}

void CreatingNewAnnotationState::react(const CompleteNewAnnotationEvent&)
{
  static constexpr bool sk_doNotClosePolygon = false;
  completeGrowingPoylgon(sk_doNotClosePolygon);
}

void CreatingNewAnnotationState::react(const CancelNewAnnotationEvent&)
{
  removeGrowingPolygon();
}

/**************** AddingVertexToNewAnnotationState *******************/

void AddingVertexToNewAnnotationState::entry()
{
  if (!ms_selectedViewUid)
  {
    spdlog::error("Entered AddingVertexToNewAnnotationState without a selected view");
    transit<ViewBeingSelectedState>();
    return;
  }

  if (!ms_growingAnnotUid)
  {
    spdlog::error("Entered AddingVertexToNewAnnotationState without "
                  "an annotation having been created");
    transit<CreatingNewAnnotationState>();
    return;
  }
}

void AddingVertexToNewAnnotationState::exit() {}

void AddingVertexToNewAnnotationState::react(const MousePressEvent& e)
{
  if (e.buttonState.left)
  {
    addVertexToGrowingPolygon(e.m_currHit);
  }

  /// @note If this is not call, the UI may not update until the next mouse event following the
  /// \c MousePressEvent that should trigger the UI change
  if (ms_renderUiCallback)
    ms_renderUiCallback();
}

void AddingVertexToNewAnnotationState::react(const MouseMoveEvent& e)
{
  hoverAnnotationAndVertex(e.m_currHit);

  if (e.buttonState.left)
  {
    addVertexToGrowingPolygon(e.m_currHit);
  }
}

void AddingVertexToNewAnnotationState::react(const MouseReleaseEvent& /*e*/) {}

void AddingVertexToNewAnnotationState::react(const TurnOffAnnotationModeEvent&)
{
  transit<AnnotationOffState>();
}

void AddingVertexToNewAnnotationState::react(const CompleteNewAnnotationEvent&)
{
  static constexpr bool sk_doNotClosePolygon = false;
  completeGrowingPoylgon(sk_doNotClosePolygon);
}

void AddingVertexToNewAnnotationState::react(const CloseNewAnnotationEvent&)
{
  static constexpr bool sk_closePolygon = true;
  completeGrowingPoylgon(sk_closePolygon);
}

void AddingVertexToNewAnnotationState::react(const UndoVertexEvent&)
{
  undoLastVertexOfGrowingPolygon();
}

void AddingVertexToNewAnnotationState::react(const CancelNewAnnotationEvent&)
{
  removeGrowingPolygon();
}

/**************** VertexSelectedState *******************/

void VertexSelectedState::entry() {}

void VertexSelectedState::exit()
{
  deselect(true, false);
}

void VertexSelectedState::react(const MousePressEvent& e)
{
  if (e.buttonState.left)
  {
    if (!selectAnnotationAndVertex(e.m_currHit))
    {
      // Did not select a vertex, so try selecting an annotation and go to stand-by state
      selectAnnotation(e.m_currHit);
      transit<StandbyState>();
    }
  }

  /// @note If this is not call, the UI may not update until the next mouse event following the
  /// \c MousePressEvent that should trigger the UI change
  if (ms_renderUiCallback)
    ms_renderUiCallback();
}

void VertexSelectedState::react(const MouseReleaseEvent& /*e*/) {}

void VertexSelectedState::react(const MouseMoveEvent& e)
{
  hoverAnnotationAndVertex(e.m_currHit);

  if (e.buttonState.left)
  {
    moveSelectedVertex(e.m_prevHit, e.m_currHit);
  }
}

void VertexSelectedState::react(const TurnOffAnnotationModeEvent&)
{
  transit<AnnotationOffState>();
}

void VertexSelectedState::react(const InsertVertexEvent&)
{
  insertVertex();
}

void VertexSelectedState::react(const RemoveSelectedVertexEvent&)
{
  removeSelectedVertex();
}

void VertexSelectedState::react(const RemoveSelectedAnnotationEvent&)
{
  removeSelectedPolygon();
}

void VertexSelectedState::react(const CutSelectedAnnotationEvent&)
{
  cutSelectedAnnotation();
}

void VertexSelectedState::react(const CopySelectedAnnotationEvent&)
{
  copySelectedAnnotation();
}

void VertexSelectedState::react(const PasteAnnotationEvent&)
{
  pasteAnnotation();
}

} // namespace state
