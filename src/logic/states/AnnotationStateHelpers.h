#ifndef ANNOTATION_STATE_HELPERS_H
#define ANNOTATION_STATE_HELPERS_H

#include <uuid.h>

namespace state
{

/// Are annotation selections/highlights visible?
bool isInStateWhereAnnotationHighlightsAreVisible();

/// Are vertex selections/highlights visible?
bool isInStateWhereVertexHighlightsAreVisible();

/// Can views scroll in the current state?
bool isInStateWhereViewsCanScroll();

/// Can crosshairs move with the mouse in the current state?
bool isInStateWhereCrosshairsCanMove();

/// Can the type of the view in the current state?
bool isInStateWhereViewTypeCanChange(const uuids::uuid& viewUid);

/// Is the toolbar visible in the current state?
bool isInStateWhereToolbarVisible();

/// Are view highlights and selections visible in the current state?
bool isInStateWhereViewSelectionsVisible();

// The following functions are used to check whether Annotation Toolbar buttons
// are visible in the current state

bool showToolbarCreateButton();                   // Create new annotation
bool showToolbarCompleteButton();                 // Complete current annotation
bool showToolbarCloseButton();                    // Close current annotation
bool showToolbarFillButton();                     // Fill current annotation
bool showToolbarCancelButton();                   // Cancel current annotation
bool showToolbarUndoButton();                     // Undo last vertex
bool showToolbarInsertVertexButton();             // Insert vertex
bool showToolbarRemoveSelectedVertexButton();     // Remove selected vertex
bool showToolbarRemoveSelectedAnnotationButton(); // Remove selected annotation
bool showToolbarCutSelectedAnnotationButton();    // Cut selected annotation
bool showToolbarCopySelectedAnnotationButton();   // Copy selected annotation
bool showToolbarPasteSelectedAnnotationButton();  // Paste selected annotation
bool showToolbarFlipAnnotationButtons();          // Flip selected annotation

} // namespace state

#endif // ANNOTATION_STATE_HELPERS_H
