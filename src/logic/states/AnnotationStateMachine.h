#ifndef ANNOTATION_STATE_MACHINE_H
#define ANNOTATION_STATE_MACHINE_H

#include "logic/interaction/ViewHit.h"
#include "logic/interaction/events/ButtonState.h"
#include "logic/states/AnnotationEvents.h"

#include <tinyfsm.hpp>
#include <uuid.h>

#include <functional>
#include <optional>

class AppData;
class Image;

namespace state
{

/**
 * @brief State machine for annotations
 *
 * @note Access the current state with \c current_state_ptr()
 * @note Check if it is in a given state with \c is_in_state<STATE>()
 */
class AnnotationStateMachine : public tinyfsm::Fsm<AnnotationStateMachine>
{
  friend class tinyfsm::Fsm<AnnotationStateMachine>;

public:
  AnnotationStateMachine() = default;
  virtual ~AnnotationStateMachine() = default;

  /**
     * @brief Synchronize the selected and hovered annotation and vertices with
     * the highlight state held in annotations
     */
  static void synchronizeAnnotationHighlights();

  /**
     * @brief Set a non-const pointer to the application data
     * @param[in] appData
     */
  static void setAppData(AppData* appData) { ms_appData = appData; }

  /**
     * @brief Set callbacks used by state machine
     * @param renderUi Function that triggers rendering one frame of the UI
     */
  static void setCallbacks(std::function<void()> renderUi) { ms_renderUiCallback = renderUi; }

  /**
     * @brief Get a non-const pointer to the application data
     */
  static AppData* appData() { return ms_appData; }

  /// @brief Get the hovered (putatively selected) view UID
  static std::optional<uuids::uuid> hoveredViewUid() { return ms_hoveredViewUid; }

  /// @brief Get the selected view UID, in which the user is currently annotating
  static std::optional<uuids::uuid> selectedViewUid() { return ms_selectedViewUid; }

  /// @brief Get the active annotation that is being created and growing
  static std::optional<uuids::uuid> growingAnnotUid() { return ms_growingAnnotUid; }

protected:
  /// Default action when entering a state
  virtual void entry() {}

  /// Default action when exiting a state
  virtual void exit() {}

  /// Default reaction for unhandled events
  void react(const tinyfsm::Event&);

  /// Default reactions for handled events
  virtual void react(const MousePressEvent&) {}
  virtual void react(const MouseReleaseEvent&) {}
  virtual void react(const MouseMoveEvent&) {}

  virtual void react(const TurnOnAnnotationModeEvent&) {}
  virtual void react(const TurnOffAnnotationModeEvent&) {}

  virtual void react(const CreateNewAnnotationEvent&) {}
  virtual void react(const CompleteNewAnnotationEvent&) {}
  virtual void react(const CloseNewAnnotationEvent&) {}
  virtual void react(const CancelNewAnnotationEvent&) {}

  virtual void react(const UndoVertexEvent&) {}
  virtual void react(const InsertVertexEvent&) {}
  virtual void react(const RemoveSelectedVertexEvent&) {}

  virtual void react(const RemoveSelectedAnnotationEvent&) {}
  virtual void react(const CutSelectedAnnotationEvent&) {}
  virtual void react(const CopySelectedAnnotationEvent&) {}
  virtual void react(const PasteAnnotationEvent&) {}

  virtual void react(const HorizontallyFlipSelectedAnnotationEvent&) {}
  virtual void react(const VerticallyFlipSelectedAnnotationEvent&) {}

  /***** Start helper functions used in multiple states *****/

  /**
     * @brief Check if the pointer to AppData is null
     * @return False iff the pointer is null
     */
  static bool checkAppData();

  /**
     * @brief Check if there is an active image that is visible in the selected view.
     * @param[in] hit Mouse hit
     * @return Pointer to the active image; nullptr if the image is null or not visible
     * in the hit view
     */
  static Image* checkActiveImage(const ViewHit& hit);

  /**
     * @brief Check if there is a view selection and whether the mouse hit is in the selected view.
     * @param[in] hit Mouse hit
     * @return True iff a view is selected and the mouse hit is in that view
     */
  bool checkViewSelection(const ViewHit& hit);

  /**
     * @brief Set the hovered view to the view hit by the mouse
     * @param[in] hit Mouse hit
     */
  void hoverView(const ViewHit& hit);

  /**
     * @brief Select the view hit by the mouse
     * @param[in] hit Mouse hit
     * @return True iff a view was selected
     */
  bool selectView(const ViewHit& hit);

  /**
     * @brief Deselect the vertex and/or annotation
     * @param[in] deselectVertex
     * @param[in] deselectAnnotation
     */
  void deselect(bool deselectVertex, bool deselectAnnotation);

  /**
     * @brief If there is a hovered annotation or vertex, then unhover them.
     */
  void unhoverAnnotation();

  /**
     * @brief Start creating a new ("growing") annotation polygon at the mouse position
     * @param[in] hit Mouse hit
     * @return True iff the annotation was created
     */
  bool createNewGrowingPolygon(const ViewHit& hit);

  /**
     * @brief Add a vertex at the mouse hit to the currently growing annotation polygon
     * @param[in] hit
     * @return True iff a vertex was added or the polygon was closed
     */
  bool addVertexToGrowingPolygon(const ViewHit& hit);

  /**
     * @brief Complete the curently growing annotation polygon
     * @param[in] closePolgon Flag to also close the polygon if it has three
     * or more vertices
     */
  void completeGrowingPoylgon(bool closePolgon);

  /**
     * @brief If the currently growing annotation polygon has two or more vertices,
     * then remove the last vertex that was added to it. If it has one vertex,
     * then remove the annotation.
     */
  void undoLastVertexOfGrowingPolygon();

  /**
     * @brief Inserts a vertex into the active/selected annotation polygon after the
     * currently selected vertex. Moves the selection to the newly inserted vertex.
     */
  void insertVertex();

  /**
     * @brief Removes the selected vertex of the active/selected annotation polygon
     * and moves the selection to the prior vertex, if one exists. If the polygon has no
     * vertices following removal, then the whole annotation is removed.
     */
  void removeSelectedVertex();

  /**
     * @brief Move the selected vertex according to the mouse movement
     * @param[in] prevHit Previous mouse hit
     * @param[in] currHit Current mouse hit
     */
  void moveSelectedVertex(const ViewHit& prevHit, const ViewHit& currHit);

  /**
     * @brief Removes the selected polygon of the active image.
     */
  void removeSelectedPolygon();

  /**
     * @brief Move the selected polygon according to the mouse movement
     * @param[in] prevHit Previous mouse hit
     * @param[in] currHit Current mouse hit
     */
  void moveSelectedPolygon(const ViewHit& prevHit, const ViewHit& currHit);

  /**
     * @brief Remove the currently growing annotation and deselect it.
     */
  void removeGrowingPolygon();

  /**
     * @brief Cut (copy + remove) the active annotation (if it exists) of the active image.
     * If there is no active annotation, then nothing happens.
     */
  void cutSelectedAnnotation();

  /**
     * @brief Copy the active annotation (if it exists) of the active image to the clipboard.
     * If there is no active annotation, then nothing happens.
     */
  void copySelectedAnnotation();

  /**
     * @brief Paste the active annotation (if it exists) to the active image from the clipboard.
     * If there is no copied annotation, then nothing happens.
     */
  void pasteAnnotation() const;

  /**
     * @brief Flip the active annotation (if it exists) of the active image.
     * If there is no active annotation, then nothing happens.
     */
  void flipSelectedAnnotation(const FlipDirection& direction);

  /**
     * @brief Find vertices in annotations of the active image near the mouse hit
     * @param[in] hit Mouse hit
     * @return Pairs of {annotation uid, vertex index} for the found vertices.
     * The closest vertex is returned in the first element of the vector.
     */
  std::vector<std::pair<uuids::uuid, size_t> > findHitVertices(const ViewHit& hit);

  /**
     * @brief Find polygons of the active image under the mouse hit
     * @param[in] hit Mouse hit
     * @return Vector of annotation UIDs, from top-most to bottom-most layer.
     */
  std::vector<uuids::uuid> findHitPolygon(const ViewHit& hit);

  /**
     * @brief Set the selected annotation and (optionally) one of its vertices
     * @param[in] annotUid UID of annotation to select
     * @param[in] vertexIndex Index of vertex to select
     */
  void setSelectedAnnotationAndVertex(
    const uuids::uuid& annotUid, const std::optional<size_t>& vertexIndex
  );

  /**
     * @brief Set/clear the hover state of the vertex near the hit
     * @param[in] hit Mouse hit
     */
  void hoverAnnotationAndVertex(const ViewHit& hit);

  /**
     * @brief Set/clear the selected state of the annotation and vertex near the hit
     * @param[in] hit Mouse hit
     * @return true iff a vertex was selected by the hit
     */
  bool selectAnnotationAndVertex(const ViewHit& hit);

  /**
     * @brief Set/clear the selected state of the annotation under the hit
     * @param[in] hit Mouse hit
     * @return True iff an annotation polygon was selected by the hit
     */
  bool selectAnnotation(const ViewHit& hit);

  /***** End helper functions used in multiple states *****/

  /// Hold a pointer to the application data object
  static AppData* ms_appData;

  /// Function that triggers rendering of one frame of the UI
  static std::function<void()> ms_renderUiCallback;

  /// Hovered (putatively selected) view UID
  static std::optional<uuids::uuid> ms_hoveredViewUid;

  /// Selected view UID, in which the user is currently annotating
  static std::optional<uuids::uuid> ms_selectedViewUid;

  /// The annotation that is currently being created and that can grow
  static std::optional<uuids::uuid> ms_growingAnnotUid;

  /// The index of the selected vertex of the active annotation and that can be edited
  /// @todo Allow selecting more than one vertex of the active annotation
  static std::optional<size_t> ms_selectedVertex;

  /// The annotation that is currently hovered
  static std::optional<uuids::uuid> ms_hoveredAnnotUid;

  /// The index of the hovered vertex of the hovered annotation
  static std::optional<size_t> ms_hoveredVertex;
};

} // namespace state

// Shortcut for referring to the state machine:
using ASM = state::AnnotationStateMachine;

#endif // ANNOTATION_STATE_MACHINE_H
