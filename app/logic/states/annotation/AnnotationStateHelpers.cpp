#include "logic/states/annotation/AnnotationStateHelpers.h"
#include "logic/states/annotation/AnnotationStateMachine.h"
#include "logic/states/annotation/AnnotationStates.h"

#include "logic/app/DataHelper.h"

#include "logic/annotation/Annotation.h"
#include "logic/app/Data.h"

namespace state::annot
{
bool isInStateWhereAnnotationHighlightsAreVisible()
{
  return ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>();
}

bool isInStateWhereVertexHighlightsAreVisible()
{
  return isInStateWhereAnnotationHighlightsAreVisible() || ASM::is_in_state<CreatingNewAnnotationState>() ||
         ASM::is_in_state<AddingVertexToNewAnnotationState>();
}

bool isInStateWhereViewsCanScroll()
{
  return ASM::is_in_state<AnnotationOffState>() || ASM::is_in_state<ViewBeingSelectedState>() ||
         ASM::is_in_state<StandbyState>() || ASM::is_in_state<CreatingNewAnnotationState>() ||
         ASM::is_in_state<VertexSelectedState>();
}

/**
 * @todo There are many edge cases to capture here.
 * For now, crosshairs movement is disabled while annotating.
 */
bool isInStateWhereCrosshairsCanMove()
{
  return ASM::is_in_state<AnnotationOffState>();
}

bool isInStateWhereViewTypeCanChange(const uuids::uuid& viewUid)
{
  const bool isSelectedView = (ASM::selectedViewUid() && (*ASM::selectedViewUid() == viewUid));

  if (!isSelectedView) {
    // Views not selected for annotating can change view type
    return true;
  }

  return isInStateWhereViewsCanScroll();
}

bool isInStateWhereToolbarVisible()
{
  return !(ASM::is_in_state<AnnotationOffState>() || ASM::is_in_state<ViewBeingSelectedState>());
}

bool isInStateWhereViewSelectionsVisible()
{
  return !ASM::is_in_state<AnnotationOffState>();
}

bool showToolbarCreateButton()
{
  return ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>();
}

bool showToolbarCompleteButton()
{
  if (!ASM::growingAnnotUid()) {
    return false;
  }

  // Need a valid annotation with at least 1 vertex in order to complete it:
  Annotation* annot = ASM::appData()->annotation(*ASM::growingAnnotUid());

  if (!annot) {
    return false;
  }

  if (0 == annot->polygon().numVertices()) {
    return false;
  }

  if (ASM::is_in_state<CreatingNewAnnotationState>() || ASM::is_in_state<AddingVertexToNewAnnotationState>()) {
    return true;
  }

  return false;
}

bool showToolbarCloseButton()
{
  if (!ASM::growingAnnotUid()) {
    return false;
  }

  // Need a valid annotation with at least 3 vertices in order to close it:
  Annotation* annot = ASM::appData()->annotation(*ASM::growingAnnotUid());
  if (!annot) {
    return false;
  }

  if (annot->polygon().numVertices() < 3) {
    return false;
  }

  if (ASM::is_in_state<CreatingNewAnnotationState>() || ASM::is_in_state<AddingVertexToNewAnnotationState>()) {
    return true;
  }

  return false;
}

bool showToolbarFillButton()
{
  if (ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>()) {
    if (!ASM::appData()) return false;

    // Show the Fill button when a closed, non-smoothed annotation is selected
    const auto selectedAnnotUid = data::getSelectedAnnotation(*ASM::appData());
    if (!selectedAnnotUid) return false;

    if (const Annotation* annot = ASM::appData()->annotation(*selectedAnnotUid)) {
      /// @todo Implement algorithm for filling smoothed polygons.
      return (annot->isClosed() && !annot->isSmoothed());
    }
  }
  return false;
}

bool showToolbarUndoButton()
{
  return showToolbarCompleteButton();
}

bool showToolbarCancelButton()
{
  return ASM::is_in_state<CreatingNewAnnotationState>() || ASM::is_in_state<AddingVertexToNewAnnotationState>();
}

bool showToolbarInsertVertexButton()
{
  return ASM::is_in_state<VertexSelectedState>();
}

bool showToolbarRemoveSelectedVertexButton()
{
  return ASM::is_in_state<VertexSelectedState>();
}

bool showToolbarRemoveSelectedAnnotationButton()
{
  if (ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>()) {
    return (ASM::appData() && data::getSelectedAnnotation(*ASM::appData()));
  }
  return false;
}

bool showToolbarCutSelectedAnnotationButton()
{
  if (ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>()) {
    return (ASM::appData() && data::getSelectedAnnotation(*ASM::appData()));
  }
  return false;
}

bool showToolbarCopySelectedAnnotationButton()
{
  if (ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>()) {
    return (ASM::appData() && data::getSelectedAnnotation(*ASM::appData()));
  }
  return false;
}

bool showToolbarPasteSelectedAnnotationButton()
{
  if (ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>()) {
    if (ASM::appData() && ASM::appData()->state().getCopiedAnnotation()) {
      // Show the Paste button if there is a copied annotation
      return true;
    }
  }
  return false;
}

bool showToolbarFlipAnnotationButtons()
{
  if (ASM::is_in_state<StandbyState>() || ASM::is_in_state<VertexSelectedState>()) {
    return (ASM::appData() && data::getSelectedAnnotation(*ASM::appData()));
  }
  return false;
}
} // namespace state::annot
