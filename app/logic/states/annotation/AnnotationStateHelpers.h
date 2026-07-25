#pragma once

#include <uuid.h>

namespace state::annot
{
/**
 * @brief Check whether annotation selections and highlights should be visible
 * @return True when annotation highlights are visible in the current annotation state
 */
bool isInStateWhereAnnotationHighlightsAreVisible();

/**
 * @brief Check whether annotation vertex selections and highlights should be visible
 * @return True when vertex highlights are visible in the current annotation state
 */
bool isInStateWhereVertexHighlightsAreVisible();

/**
 * @brief Check whether views can scroll while the annotation state machine is active
 * @return True when scrolling is allowed in the current annotation state
 */
bool isInStateWhereViewsCanScroll();

/**
 * @brief Check whether mouse interaction can move crosshairs in the current annotation state
 * @return True when crosshairs movement is allowed
 */
bool isInStateWhereCrosshairsCanMove();

/**
 * @brief Check whether a view can change type in the current annotation state
 * @param viewUid UID of the view being changed
 * @return True when the view type may be changed
 */
bool isInStateWhereViewTypeCanChange(const uuids::uuid& viewUid);

/**
 * @brief Check whether the annotation toolbar should be visible
 * @return True when the toolbar should be shown
 */
bool isInStateWhereToolbarVisible();

/**
 * @brief Check whether annotation view selection highlights should be visible
 * @return True when view selection highlights should be shown
 */
bool isInStateWhereViewSelectionsVisible();

/**
 * @brief Check whether the toolbar create button should be visible
 * @return True when a new annotation can be started
 */
bool showToolbarCreateButton();

/**
 * @brief Check whether the toolbar complete button should be visible
 * @return True when the annotation in progress can be completed
 */
bool showToolbarCompleteButton();

/**
 * @brief Check whether the toolbar close button should be visible
 * @return True when the annotation in progress can be closed
 */
bool showToolbarCloseButton();

/**
 * @brief Check whether the toolbar fill button should be visible
 * @return True when the selected annotation can fill a segmentation
 */
bool showToolbarFillButton();

/**
 * @brief Check whether the toolbar cancel button should be visible
 * @return True when annotation creation can be canceled
 */
bool showToolbarCancelButton();

/**
 * @brief Check whether the toolbar undo button should be visible
 * @return True when the last annotation vertex can be removed
 */
bool showToolbarUndoButton();

/**
 * @brief Check whether the toolbar insert-vertex button should be visible
 * @return True when a selected vertex can be followed by a new vertex
 */
bool showToolbarInsertVertexButton();

/**
 * @brief Check whether the toolbar remove-vertex button should be visible
 * @return True when a selected annotation vertex can be removed
 */
bool showToolbarRemoveSelectedVertexButton();

/**
 * @brief Check whether the toolbar remove-annotation button should be visible
 * @return True when a selected annotation can be removed
 */
bool showToolbarRemoveSelectedAnnotationButton();

/**
 * @brief Check whether the toolbar cut button should be visible
 * @return True when a selected annotation can be cut
 */
bool showToolbarCutSelectedAnnotationButton();

/**
 * @brief Check whether the toolbar copy button should be visible
 * @return True when a selected annotation can be copied
 */
bool showToolbarCopySelectedAnnotationButton();

/**
 * @brief Check whether the toolbar paste button should be visible
 * @return True when a copied annotation can be pasted
 */
bool showToolbarPasteSelectedAnnotationButton();

/**
 * @brief Check whether annotation flip buttons should be visible
 * @return True when a selected annotation can be flipped
 */
bool showToolbarFlipAnnotationButtons();
} // namespace state::annot
