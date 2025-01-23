#include "logic/states/AnnotationStateMachine.h"
#include "logic/states/AnnotationStates.h"

#include "common/DataHelper.h"
#include "common/MathFuncs.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace
{

// Only create/edit points on the outer polygon boundary for now
static constexpr size_t OUTER_BOUNDARY = 0;

static constexpr size_t FIRST_VERTEX_INDEX = 0;

} // namespace

namespace state
{

AppData* ASM::ms_appData = nullptr;
std::function<void()> ASM::ms_renderUiCallback = nullptr;

std::optional<uuids::uuid> ASM::ms_hoveredViewUid = std::nullopt;
std::optional<uuids::uuid> ASM::ms_selectedViewUid = std::nullopt;
std::optional<uuids::uuid> ASM::ms_growingAnnotUid = std::nullopt;

std::optional<size_t> ASM::ms_selectedVertex = std::nullopt;
std::optional<uuids::uuid> ASM::ms_hoveredAnnotUid = std::nullopt;
std::optional<size_t> ASM::ms_hoveredVertex = std::nullopt;

void AnnotationStateMachine::react(const tinyfsm::Event&)
{
  spdlog::warn("Unhandled event sent to AnnotationStateMachine");
}

bool AnnotationStateMachine::checkAppData()
{
  if (!ms_appData)
  {
    spdlog::error("AppData is null");
    return false;
  }
  return true;
}

Image* AnnotationStateMachine::checkActiveImage(const ViewHit& hit)
{
  if (!checkAppData())
    return nullptr;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
  {
    spdlog::info("There is no active image to annotate");
    return nullptr;
  }

  Image* activeImage = ms_appData->image(*activeImageUid);
  if (!activeImage)
  {
    spdlog::error("Active image {} is null", *activeImageUid);
    return nullptr;
  }

  if (!std::count(
        std::begin(hit.view->visibleImages()), std::end(hit.view->visibleImages()), *activeImageUid
      ))
  {
    // The active image is not visible in the view hit by the mouse
    return nullptr;
  }

  return activeImage;
}

bool AnnotationStateMachine::checkViewSelection(const ViewHit& hit)
{
  if (!checkAppData())
    return false;

  if (!ms_selectedViewUid)
  {
    spdlog::error("No selected view in which to annotate");
    transit<ViewBeingSelectedState>();
    return false;
  }

  if (*ms_selectedViewUid != hit.viewUid)
  {
    // Mouse pointer is not in the view selected for annotating
    return false;
  }

  return true;
}

void AnnotationStateMachine::hoverView(const ViewHit& hit)
{
  if (hit.view && (ViewType::ThreeD != hit.view->viewType()))
  {
    ms_hoveredViewUid = hit.viewUid;
  }
  else
  {
    // Do not hover 3D views
    ms_hoveredViewUid = std::nullopt;
  }
}

bool AnnotationStateMachine::selectView(const ViewHit& hit)
{
  if (ms_selectedViewUid && *ms_selectedViewUid != hit.viewUid)
  {
    deselect(true, false);
    unhoverAnnotation();
  }

  if (hit.view && (ViewType::ThreeD != hit.view->viewType()))
  {
    ms_selectedViewUid = hit.viewUid;
    return true;
  }

  // Do not select 3D views
  return false;
}

void AnnotationStateMachine::deselect(bool deselectVertex, bool deselectAnnotation)
{
  if (!checkAppData())
    return;

  if (deselectVertex)
  {
    ms_selectedVertex = std::nullopt;
  }

  if (deselectAnnotation)
  {
    const auto activeImageUid = ms_appData->activeImageUid();
    if (!activeImageUid)
      return;

    if (!ms_appData->assignActiveAnnotationUidToImage(*activeImageUid, std::nullopt))
    {
      spdlog::error("Unable to remove active annotation from image {}", *activeImageUid);
    }
  }

  synchronizeAnnotationHighlights();
}

void AnnotationStateMachine::unhoverAnnotation()
{
  ms_hoveredAnnotUid = std::nullopt;
  ms_hoveredVertex = std::nullopt;
  synchronizeAnnotationHighlights();
}

bool AnnotationStateMachine::createNewGrowingPolygon(const ViewHit& hit)
{
  if (!checkAppData())
    return false;
  if (!checkViewSelection(hit))
    return false;

  // Annotate on the active image
  const Image* activeImage = checkActiveImage(hit);
  if (!activeImage)
    return false;

  const auto activeImageUid = ms_appData->activeImageUid();

  // Compute the plane equation in Subject space. Use the World position after
  // the view offset has been applied, so that the user can annotate in any view
  // of a lightbox layout.
  const auto [subjectPlaneEquation, subjectPlanePoint] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(),
    -hit.worldFrontAxis,
    glm::vec3{hit.worldPos_offsetApplied}
  );

  // Create new annotation for this image:
  std::optional<uuids::uuid> annotUid;

  try
  {
    const std::string name = std::string("Annotation ")
                             + std::to_string(ms_appData->annotationsForImage(*activeImageUid).size()
                             );

    const glm::vec4 color{activeImage->settings().borderColor(), 1.0f};

    annotUid = ms_appData
                 ->addAnnotation(*activeImageUid, Annotation(name, color, subjectPlaneEquation));

    if (!annotUid)
    {
      spdlog::error(
        "Unable to add new annotation (subject plane: {}) for image {}",
        glm::to_string(subjectPlaneEquation),
        *activeImageUid
      );
      return false;
    }

    ms_appData->assignActiveAnnotationUidToImage(*activeImageUid, *annotUid);

    spdlog::debug(
      "Added new annotation {} (subject plane: {}) for image {}",
      *annotUid,
      glm::to_string(subjectPlaneEquation),
      *activeImageUid
    );
  }
  catch (const std::exception& ex)
  {
    spdlog::error(
      "Unable to create new annotation (subject plane: {}) for image {}: {}",
      glm::to_string(subjectPlaneEquation),
      *activeImageUid,
      ex.what()
    );
    return false;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);
  if (!annot)
  {
    spdlog::error("Null annotation {}", *annotUid);
    return false;
  }

  // Mark this annotation as the one being created:
  ms_growingAnnotUid = annotUid;

  // Select the annotation
  const std::optional<size_t> noSelectedVertex;
  setSelectedAnnotationAndVertex(*annotUid, noSelectedVertex);

  return true;
}

bool AnnotationStateMachine::addVertexToGrowingPolygon(const ViewHit& hit)
{
  if (!checkAppData())
    return false;
  if (!checkViewSelection(hit))
    return false;

  if (!ms_growingAnnotUid)
  {
    spdlog::error("There is no new annotation for which to add a vertex");
    transit<AnnotationOffState>();
    return false;
  }

  // Annotate on the active image
  const Image* activeImage = checkActiveImage(hit);
  if (!activeImage)
    return false;

  const auto [subjectPlaneEquation, subjectPlanePoint] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(),
    -hit.worldFrontAxis,
    glm::vec3{hit.worldPos_offsetApplied}
  );

  Annotation* growingAnnot = ms_appData->annotation(*ms_growingAnnotUid);
  if (!growingAnnot)
  {
    spdlog::warn("Growing annotation {} is no longer valid", *ms_growingAnnotUid);
    transit<StandbyState>();
    return false;
  }

  const auto hitVertices = findHitVertices(hit);

  if (growingAnnot->numBoundaries() > 0)
  {
    // If the growing annotation has a boundary polygon, then perform checks
    // to see if we should
    // 1) ignore the current vertex due to being too close to the last vertex
    // 2) close the polygon due to the current vertex being close to the first vertex

    const bool hasMoreThanTwoVertices
      = (growingAnnot->getBoundaryVertices(OUTER_BOUNDARY).size() >= 2);

    // Index of the vertex that is going to be added:
    const size_t currentVertexIndex = growingAnnot->getBoundaryVertices(OUTER_BOUNDARY).size();

    // Loop over vertices near the mouse hit
    for (const auto& vertex : hitVertices)
    {
      if (*ms_growingAnnotUid == vertex.first && currentVertexIndex == (vertex.second + 1))
      {
        // The current vertex to add is close to the last vertex, so do not add it.
        return false;
      }

      if ( *ms_growingAnnotUid == vertex.first &&
                 FIRST_VERTEX_INDEX == vertex.second &&
                 ! growingAnnot->isClosed() &&
                 hasMoreThanTwoVertices )
      {
        // The point is near the annotation's first vertex and the annotation
        // has more than two vertices, so close the polygon and do not add a new vertex.
        growingAnnot->setClosed(true);
        growingAnnot->setFilled(true);

        // Highlight the first vertex
        setSelectedAnnotationAndVertex(*ms_growingAnnotUid, FIRST_VERTEX_INDEX);

        transit<StandbyState>();
        return true;
      }
    }
  }

  // Check if the point is near another vertex of an annotation. Use this existing
  // vertex position for the new one, so that we can create sealed annotations.

  for (const auto& vertex : hitVertices)
  {
    Annotation* otherAnnot = ms_appData->annotation(vertex.first);

    if (!otherAnnot)
    {
      spdlog::error("Null annotation {}", vertex.first);
      continue;
    }

    if (const auto planePoint2d = otherAnnot->polygon().getBoundaryVertex(OUTER_BOUNDARY, vertex.second))
    {
      // Add the existing point
      growingAnnot->addPlanePointToBoundary(OUTER_BOUNDARY, *planePoint2d);

      // Highlight the added vertex:
      const size_t addedVertexIndex = growingAnnot->getBoundaryVertices(OUTER_BOUNDARY).size() - 1;
      setSelectedAnnotationAndVertex(*ms_growingAnnotUid, addedVertexIndex);

      return true;
    }
  }

  // The prior checks fell through, so add the new point to the polygon:
  const auto projectedPoint = growingAnnot
                                ->addSubjectPointToBoundary(OUTER_BOUNDARY, subjectPlanePoint);

  if (!projectedPoint)
  {
    spdlog::error(
      "Unable to add point {} to annotation {}",
      glm::to_string(hit.worldPos_offsetApplied),
      *ms_growingAnnotUid
    );
    return false;
  }

  // Highlight the added vertex:
  const size_t addedVertexIndex = growingAnnot->getBoundaryVertices(OUTER_BOUNDARY).size() - 1;
  setSelectedAnnotationAndVertex(*ms_growingAnnotUid, addedVertexIndex);

  return true;
}

void AnnotationStateMachine::completeGrowingPoylgon(bool closePolgon)
{
  if (!checkAppData())
    return;

  if (!ms_growingAnnotUid)
  {
    // No growing annotation to complete
    return;
  }

  if (closePolgon)
  {
    Annotation* growingAnnot = ms_appData->annotation(*ms_growingAnnotUid);
    if (!growingAnnot)
    {
      spdlog::warn("Growing annotation {} is no longer valid", *ms_growingAnnotUid);
      transit<StandbyState>();
      return;
    }

    const bool hasThreeOrMoreVertices
      = (growingAnnot->getBoundaryVertices(OUTER_BOUNDARY).size() >= 3);

    if (hasThreeOrMoreVertices)
    {
      growingAnnot->setClosed(true);
      growingAnnot->setFilled(true);
    }
  }

  spdlog::info("Finished creating annotation {}", *ms_growingAnnotUid);

  // Done with growing this annotation:
  ms_growingAnnotUid = std::nullopt;

  transit<StandbyState>();
}

void AnnotationStateMachine::undoLastVertexOfGrowingPolygon()
{
  if (!checkAppData())
    return;

  if (!ms_growingAnnotUid)
  {
    // No growing annotation to undo
    return;
  }

  Annotation* growingAnnot = ms_appData->annotation(*ms_growingAnnotUid);
  if (!growingAnnot)
  {
    spdlog::warn("Growing annotation {} is no longer valid", *ms_growingAnnotUid);
    transit<StandbyState>();
    return;
  }

  if (growingAnnot->numBoundaries() > 0)
  {
    const size_t numVertices = growingAnnot->getBoundaryVertices(OUTER_BOUNDARY).size();

    if (numVertices >= 2)
    {
      // There are at least two vertices, so remove the last one
      growingAnnot->polygon().removeVertexFromBoundary(OUTER_BOUNDARY, numVertices - 1);

      // Highlight the last vertex
      const size_t newSelectedVertex = numVertices - 2;
      setSelectedAnnotationAndVertex(*ms_growingAnnotUid, newSelectedVertex);
    }
    else
    {
      // There are 0 or 1 vertices, so remove the whole annotation
      removeGrowingPolygon();
    }
  }
}

void AnnotationStateMachine::insertVertex()
{
  if (!checkAppData())
    return;

  if (!ms_selectedVertex)
  {
    spdlog::warn("There is no selected vertex after which to insert a new vertex");
    return;
  }

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  const auto annotUid = ms_appData->imageToActiveAnnotationUid(*activeImageUid);
  if (!annotUid)
  {
    transit<StandbyState>();
    return;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);
  if (!annot)
  {
    spdlog::warn("Annotation {} is not valid", *annotUid);
    transit<StandbyState>();
    return;
  }

  if (0 == annot->numBoundaries())
  {
    spdlog::warn("Annotation {} has no boundaries", *annotUid);
    transit<StandbyState>();
    return;
  }

  const size_t N = annot->getBoundaryVertices(OUTER_BOUNDARY).size();

  if (*ms_selectedVertex >= N)
  {
    spdlog::warn("Invalid vertex {} for annotation {}", *ms_selectedVertex, *annotUid);
    transit<StandbyState>();
    return;
  }

  if (0 == N)
  {
    spdlog::warn("Boundary {} of annotation {} has no vertices", OUTER_BOUNDARY, *annotUid);
    transit<StandbyState>();
    return;
  }

  const std::optional<glm::vec2> selectedVertex
    = annot->polygon().getBoundaryVertex(OUTER_BOUNDARY, *ms_selectedVertex);

  if (!selectedVertex)
  {
    spdlog::warn("Invalid vertex {} for annotation {}", *ms_selectedVertex, *annotUid);
    transit<StandbyState>();
    return;
  }

  if (N - 1 == *ms_selectedVertex)
  {
    // Selected the last vertex
    glm::vec2 newVertex;

    if (1 == N)
    {
      // Selected the last and only vertex. Add a new vertex with a small offset.
      newVertex = *selectedVertex + glm::vec2{5.0f, 5.0f};
    }
    else if (!annot->isClosed())
    {
      // Selected the last vertex and the annotation is not closed. There is a previous vertex,
      // so insert a new vertex after the last vertex that continues the line from the previous vertex.
      const size_t prevVertexIndex = *ms_selectedVertex - 1;

      const std::optional<glm::vec2> prevVertex
        = annot->polygon().getBoundaryVertex(OUTER_BOUNDARY, prevVertexIndex);

      if (!prevVertex)
      {
        spdlog::warn("Invalid vertex {} for annotation {}", prevVertexIndex, *annotUid);
        transit<StandbyState>();
        return;
      }

      newVertex = *selectedVertex + (*selectedVertex - *prevVertex);
    }
    else if (annot->isClosed())
    {
      // Selected the last vertex and the annotation is closed. Insert a new vertex in between the
      // last vertex and the first vertex.
      static constexpr size_t firstVertexIndex = 0;

      const std::optional<glm::vec2> firstVetex
        = annot->polygon().getBoundaryVertex(OUTER_BOUNDARY, firstVertexIndex);

      if (!firstVetex)
      {
        spdlog::warn("Invalid vertex {} for annotation {}", firstVertexIndex, *annotUid);
        transit<StandbyState>();
        return;
      }

      newVertex = 0.5f * (*selectedVertex + *firstVetex);
    }

    annot->addPlanePointToBoundary(OUTER_BOUNDARY, newVertex);

    const size_t newSelectedVertex = *ms_selectedVertex + 1;
    setSelectedAnnotationAndVertex(*annotUid, newSelectedVertex);
  }
  else if (N - 1 > *ms_selectedVertex)
  {
    // Selected a vertex that is not the last one. Insert a new vertex at the midpoint
    // between the selected vertex and the next vertex.

    const size_t nextVertexIndex = *ms_selectedVertex + 1;

    const std::optional<glm::vec2> nextVertex
      = annot->polygon().getBoundaryVertex(OUTER_BOUNDARY, nextVertexIndex);

    if (!nextVertex)
    {
      spdlog::warn("Invalid vertex {} for annotation {}", nextVertexIndex, *annotUid);
      transit<StandbyState>();
      return;
    }

    const glm::vec2 newVertex = 0.5f * (*selectedVertex + *nextVertex);

    if (annot->insertPlanePointIntoBoundary(OUTER_BOUNDARY, nextVertexIndex, newVertex))
    {
      const size_t newSelectedVertex = *ms_selectedVertex + 1;
      setSelectedAnnotationAndVertex(*annotUid, newSelectedVertex);
    }
    else
    {
      spdlog::error("Unable to insert vertex into annotation {}", *annotUid);
    }
  }
}

void AnnotationStateMachine::removeSelectedVertex()
{
  if (!checkAppData())
    return;

  if (!ms_selectedVertex)
  {
    spdlog::warn("There is no selected vertex to remove");
    return;
  }

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  const auto annotUid = ms_appData->imageToActiveAnnotationUid(*activeImageUid);
  if (!annotUid)
  {
    transit<StandbyState>();
    return;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);
  if (!annot)
  {
    spdlog::warn("Annotation {} is not valid", *annotUid);
    transit<StandbyState>();
    return;
  }

  if (0 == annot->numBoundaries())
  {
    spdlog::warn("Annotation {} has no boundaries", *annotUid);
    transit<StandbyState>();
    return;
  }

  const size_t numVertices = annot->getBoundaryVertices(OUTER_BOUNDARY).size();

  bool removeAnnotation = false;

  if (numVertices >= 2)
  {
    // There are at least two vertices, so remove the last one
    if (annot->polygon().removeVertexFromBoundary(OUTER_BOUNDARY, *ms_selectedVertex))
    {
      const size_t newNumVertices = numVertices - 1;

      size_t nextVertexToSelect = 0ul;

      if (*ms_selectedVertex >= 1)
      {
        nextVertexToSelect = *ms_selectedVertex - 1ul;
      }
      else if (0 == *ms_selectedVertex && annot->isClosed())
      {
        // Wrap around the closed polygon
        nextVertexToSelect = numVertices - 2ul;
      }
      else if (0 == *ms_selectedVertex && *ms_selectedVertex <= newNumVertices - 1)
      {
        nextVertexToSelect = *ms_selectedVertex;
      }

      setSelectedAnnotationAndVertex(*annotUid, nextVertexToSelect);
    }
  }
  else if (1 == numVertices && 0 == *ms_selectedVertex)
  {
    // The selected vertex is the only one left, so remove the annotation
    removeAnnotation = true;
  }
  else if (0 == numVertices)
  {
    // This should not happen
    spdlog::warn("The polygon has no vertices, so removing it.");
    removeAnnotation = true;
  }

  if (removeAnnotation)
  {
    if (ms_appData->removeAnnotation(*annotUid))
    {
      spdlog::info("Removed annotation {}", *annotUid);
      transit<StandbyState>();
      deselect(true, true);
    }
    else
    {
      spdlog::error("Unable to remove annotation {}", *annotUid);
    }
  }
}

void AnnotationStateMachine::moveSelectedVertex(const ViewHit& prevHit, const ViewHit& currHit)
{
  if (!checkAppData())
    return;
  if (!checkActiveImage(currHit))
    return;
  if (!checkViewSelection(currHit))
    return;

  if (!currHit.view)
    return;

  // Don't move the vertex unless the mouse has moved
  if (glm::all(glm::epsilonEqual(
        prevHit.worldPos_offsetApplied, currHit.worldPos_offsetApplied, glm::epsilon<float>()
      )))
  {
    return;
  }

  if (!ms_selectedVertex)
  {
    spdlog::warn("There is no selected vertex to move");
    return;
  }

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  const auto annotUid = ms_appData->imageToActiveAnnotationUid(*activeImageUid);
  if (!annotUid)
  {
    transit<StandbyState>();
    return;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);

  if (!annot)
  {
    spdlog::warn("Annotation {} is not valid", *annotUid);
    transit<StandbyState>();
    return;
  }

  if (!annot->isVisible())
  {
    // Don't edit invisible annotation
    return;
  }

  if (0 == annot->numBoundaries())
  {
    spdlog::warn("Annotation {} has no boundaries", *annotUid);
    transit<StandbyState>();
    return;
  }

  const std::optional<glm::vec2> existingVertex
    = annot->polygon().getBoundaryVertex(OUTER_BOUNDARY, *ms_selectedVertex);

  if (!existingVertex)
  {
    spdlog::warn("Invalid vertex {} to move for annotation {}", *ms_selectedVertex, *annotUid);
    transit<StandbyState>();
    return;
  }

  const Image* activeImage = checkActiveImage(currHit);
  if (!activeImage)
    return;

  const auto [subjectPlaneEquation, subjectPlanePoint] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(),
    -currHit.worldFrontAxis,
    glm::vec3{currHit.worldPos_offsetApplied}
  );

  const auto hitVertices = findHitVertices(currHit);

  const size_t N = annot->getBoundaryVertices(OUTER_BOUNDARY).size();
  const bool hasMoreThanTwoVertices = (N >= 2);

  // Check whether we should close the polygon due to this move
  if (annot->numBoundaries() > 0 && !annot->isClosed() && hasMoreThanTwoVertices)
  {
    for (const auto& hitVertex : hitVertices)
    {
      if (*annotUid == hitVertex.first && hitVertex.second == *ms_selectedVertex)
      {
        // Ignore a hit to the selected/moving vertex itself
        continue;
      }

      const bool firstHitLast = (FIRST_VERTEX_INDEX == *ms_selectedVertex)
                                && (N - 1 == hitVertex.second);
      const bool lastHitFirst = (N - 1 == *ms_selectedVertex)
                                && (FIRST_VERTEX_INDEX == hitVertex.second);

      if (firstHitLast || lastHitFirst)
      {
        annot->setClosed(true);
        annot->setFilled(true);
        return;
      }
    }
  }

  // Check if the point is near another vertex of an annotation. Use this existing
  // vertex position for the new one, so that we can create sealed annotations.
  for (const auto& hitVertex : hitVertices)
  {
    if (*annotUid == hitVertex.first && hitVertex.second == *ms_selectedVertex)
    {
      // Ignore a hit to the selected/moving vertex itself
      continue;
    }

    Annotation* otherAnnot = ms_appData->annotation(hitVertex.first);
    if (!otherAnnot)
    {
      spdlog::error("Null annotation {}", hitVertex.first);
      continue;
    }

    if (const auto planePoint2d = otherAnnot->polygon().getBoundaryVertex(OUTER_BOUNDARY, hitVertex.second))
    {
      // Move to the existing point
      annot->polygon().setBoundaryVertex(OUTER_BOUNDARY, *ms_selectedVertex, *planePoint2d);
      return;
    }
  }

  // The prior checks fell through, so move vertex to the new point after projection
  // to the subject image plane:
  const glm::vec2 annotPlanePoint = annot->projectSubjectPointToAnnotationPlane(subjectPlanePoint);

  if (!annot->polygon().setBoundaryVertex(OUTER_BOUNDARY, *ms_selectedVertex, annotPlanePoint))
  {
    spdlog::error(
      "Unable to move point {} of annotation {}",
      glm::to_string(currHit.worldPos_offsetApplied),
      *annotUid
    );
  }
}

void AnnotationStateMachine::removeSelectedPolygon()
{
  if (!checkAppData())
    return;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  const auto annotUid = ms_appData->imageToActiveAnnotationUid(*activeImageUid);
  if (!annotUid)
  {
    return;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);
  if (!annot)
  {
    spdlog::warn("Annotation {} is not valid", *annotUid);
    transit<StandbyState>();
    return;
  }

  if (ms_appData->removeAnnotation(*annotUid))
  {
    spdlog::info("Removed annotation {}", *annotUid);
    transit<StandbyState>();
    deselect(true, true);
  }
  else
  {
    spdlog::error("Unable to remove annotation {}", *annotUid);
  }
}

void AnnotationStateMachine::moveSelectedPolygon(const ViewHit& prevHit, const ViewHit& currHit)
{
  if (!checkAppData())
    return;
  if (!checkActiveImage(currHit))
    return;
  if (!checkViewSelection(currHit))
    return;

  if (!currHit.view)
    return;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  const auto annotUid = ms_appData->imageToActiveAnnotationUid(*activeImageUid);
  if (!annotUid)
  {
    transit<StandbyState>();
    return;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);

  if (!annot)
  {
    spdlog::warn("Annotation {} is not valid", *annotUid);
    transit<StandbyState>();
    return;
  }

  if (!annot->isVisible())
  {
    // Don't move invisible annotation
    return;
  }

  if (0 == annot->numBoundaries())
  {
    spdlog::warn("Annotation {} has no boundaries", *annotUid);
    transit<StandbyState>();
    return;
  }

  const Image* activeImage = checkActiveImage(currHit);
  if (!activeImage)
    return;

  const auto [subjectPlaneEquationPrev, subjectPlanePointPrev] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(),
    -prevHit.worldFrontAxis,
    glm::vec3{prevHit.worldPos_offsetApplied}
  );

  const auto [subjectPlaneEquationCurr, subjectPlanePointCurr] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(),
    -currHit.worldFrontAxis,
    glm::vec3{currHit.worldPos_offsetApplied}
  );

  const glm::vec2 annotPlanePointPrev = annot->projectSubjectPointToAnnotationPlane(
    subjectPlanePointPrev
  );
  const glm::vec2 annotPlanePointCurr = annot->projectSubjectPointToAnnotationPlane(
    subjectPlanePointCurr
  );

  const glm::vec2 delta = annotPlanePointCurr - annotPlanePointPrev;

  const std::vector<glm::vec2>& vertices = annot->getBoundaryVertices(OUTER_BOUNDARY);

  for (size_t i = 0; i < vertices.size(); ++i)
  {
    if (!annot->polygon().setBoundaryVertex(OUTER_BOUNDARY, i, vertices[i] + delta))
    {
      spdlog::error("Unable to move annotation {}", *annotUid);
    }
  }
}

void AnnotationStateMachine::removeGrowingPolygon()
{
  if (!checkAppData())
    return;

  if (!ms_growingAnnotUid)
  {
    transit<StandbyState>();
    return;
  }

  if (!ms_appData->removeAnnotation(*ms_growingAnnotUid))
  {
    spdlog::error("Unable to remove annotation {}", *ms_growingAnnotUid);
  }

  ms_growingAnnotUid = std::nullopt;
  deselect(true, true);
  transit<StandbyState>();
}

void AnnotationStateMachine::cutSelectedAnnotation()
{
  // cut = copy + remove
  copySelectedAnnotation();
  removeSelectedPolygon();
}

void AnnotationStateMachine::copySelectedAnnotation()
{
  if (!checkAppData())
    return;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  const auto annotUid = ms_appData->imageToActiveAnnotationUid(*activeImageUid);
  if (!annotUid)
  {
    return;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);
  if (!annot)
  {
    spdlog::warn("Annotation {} is not valid", *annotUid);
    transit<StandbyState>();
    return;
  }

  ms_appData->state().setCopiedAnnotation(*annot);
}

void AnnotationStateMachine::pasteAnnotation() const
{
  // Angle threshold (in degrees) for checking whether two vectors are parallel
  static constexpr float sk_parallelThreshold_degrees = 0.1f;

  if (!checkAppData())
    return;

  std::optional<Annotation> annot = ms_appData->state().getCopiedAnnotation();

  if (!annot)
  {
    spdlog::debug("There is no polygon in the clipboard to paste.");
    return;
  }

  if (!ms_selectedViewUid)
  {
    spdlog::warn("A view must be selected before pasting the polygon.");
    return;
  }

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
  {
    spdlog::debug("There is no active image on which to paste the polygon.");
    return;
  }

  const Image* activeImage = ms_appData->image(*activeImageUid);
  if (!activeImage)
  {
    spdlog::error("The active image is null.");
    return;
  }

  View* selectedView = ms_appData->windowData().getView(*ms_selectedViewUid);
  if (!selectedView)
  {
    spdlog::error("The selected view is null.");
    return;
  }

  // The equation of the view plane (in image Subject space) is needed in order to
  // paste the annotation on this view. Calculation of the plane equation requires
  // the view normal vector and a point on the view plane (i.e. the offset crosshairs).

  // World-space view normal direction
  const glm::vec3 worldViewBackDir
    = camera::worldDirection(selectedView->camera(), Directions::View::Back);

  // World-space crosshairs position on this view slice (accounting for view offest)
  const glm::vec3 worldXhairsPos
    = selectedView
        ->updateImageSlice(*ms_appData, ms_appData->state().worldCrosshairs().worldOrigin());

  const auto [subjectPlaneEquation, subjectPlanePoint] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(), worldViewBackDir, worldXhairsPos
  );

  if (!camera::areVectorsParallel(
        glm::vec3{subjectPlaneEquation},
        glm::vec3{annot->getSubjectPlaneEquation()},
        sk_parallelThreshold_degrees
      ))
  {
    spdlog::warn("The normal vector of the view plane and the normal vector of the "
                 "pasted annotation polygon do not match. The pasted polygon may be "
                 "rotated with respect to its original orientation.");
  }

  // Set the new Subject plane in the for the annotation
  annot->setSubjectPlane(subjectPlaneEquation);

  // Append "(copy)" to the name of the pasted annotation
  annot->setDisplayName(annot->getDisplayName() + " (copy)");

  // Add the annotation to the active image:
  if (const auto pastedAnnotUid = ms_appData->addAnnotation(*activeImageUid, *annot))
  {
    // Make this the active annotation
    ms_appData->assignActiveAnnotationUidToImage(*activeImageUid, *pastedAnnotUid);
    synchronizeAnnotationHighlights();

    spdlog::info(
      "Pasted new annotation {} from clipboard to image {}", *pastedAnnotUid, *activeImageUid
    );
  }
  else
  {
    spdlog::error("Unable to add pasted annotation to image {}", *activeImageUid);
  }
}

void AnnotationStateMachine::flipSelectedAnnotation(const FlipDirection& flipDirection)
{
  if (!checkAppData())
    return;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  const Image* activeImage = ms_appData->image(*activeImageUid);
  if (!activeImage)
  {
    spdlog::error("The active image is null.");
    return;
  }

  const auto annotUid = ms_appData->imageToActiveAnnotationUid(*activeImageUid);
  if (!annotUid)
  {
    return;
  }

  Annotation* annot = ms_appData->annotation(*annotUid);
  if (!annot)
  {
    spdlog::warn("Annotation {} is not valid", *annotUid);
    transit<StandbyState>();
    return;
  }

  if (!annot->isVisible())
  {
    // Don't flip invisible annotation
    return;
  }

  View* selectedView = ms_appData->windowData().getView(*ms_selectedViewUid);
  if (!selectedView)
  {
    spdlog::error("The selected view is null.");
    return;
  }

  const std::vector<glm::vec2>& vertices = annot->getBoundaryVertices(OUTER_BOUNDARY);

  if (vertices.empty())
  {
    return;
  }

  // Compute annotation centroid (in annotation plane coordinates)
  glm::vec2 polyCentroid{0.0f, 0.0f};

  for (size_t i = 0; i < vertices.size(); ++i)
  {
    polyCentroid += vertices[i];
  }

  polyCentroid = (1.0f / static_cast<float>(vertices.size())) * polyCentroid;

  // Transform polygon vertex in annotation plane to view Clip space:
  auto transformFromAnnotPlaneToViewClip =
    [annot, selectedView, activeImage](const glm::vec2& annotPoint) -> glm::vec3
  {
    const glm::vec3 subjectPoint = annot->unprojectFromAnnotationPlaneToSubjectPoint(annotPoint);

    const glm::vec4 clipPoint = camera::clip_T_world(selectedView->camera())
                                * activeImage->transformations().worldDef_T_subject()
                                * glm::vec4{subjectPoint, 1.0f};

    return glm::vec3{clipPoint / clipPoint.w};
  };

  const glm::vec3 clipPolyCentroid = transformFromAnnotPlaneToViewClip(polyCentroid);

  std::vector<glm::vec2> flippedVertices;

  for (const glm::vec2& annotVertex : vertices)
  {
    const glm::vec3 clipVertex = transformFromAnnotPlaneToViewClip(annotVertex);

    glm::vec4 clipVertexFlipped{0};

    switch (flipDirection)
    {
    case FlipDirection::Horizontal:
    {
      const float delta = clipVertex.x - clipPolyCentroid.x;
      clipVertexFlipped = glm::vec4{clipPolyCentroid.x - delta, clipVertex.y, clipVertex.z, 1.0f};
      break;
    }
    case FlipDirection::Vertical:
    {
      const float delta = clipVertex.y - clipPolyCentroid.y;
      clipVertexFlipped = glm::vec4{clipVertex.x, clipPolyCentroid.y - delta, clipVertex.z, 1.0f};
      break;
    }
    }

    const glm::vec4 subjectVertexFlipped = activeImage->transformations().subject_T_worldDef()
                                           * camera::world_T_clip(selectedView->camera())
                                           * clipVertexFlipped;

    // Project flipped vertex back into annotation plane:
    const glm::vec2 annotPlaneVertexFlipped = annot->projectSubjectPointToAnnotationPlane(
      glm::vec3{subjectVertexFlipped / subjectVertexFlipped.w}
    );

    flippedVertices.emplace_back(annotPlaneVertexFlipped);
  }

  if (!annot->polygon().setBoundaryVertices(OUTER_BOUNDARY, flippedVertices))
  {
    spdlog::error("Unable to flip annotation {}", *annotUid);
  }
}

std::vector<std::pair<uuids::uuid, size_t> > AnnotationStateMachine::findHitVertices(
  const ViewHit& hit
)
{
  // Distance threshold for hitting a vertex (in pixels)
  static constexpr float sk_distThresh_inPixels = 6.0f;

  static const std::vector<std::pair<uuids::uuid, size_t> > sk_empty;

  if (!checkAppData())
    return sk_empty;

  if (!hit.view)
  {
    spdlog::error("Null view");
    return sk_empty;
  }

  const Image* activeImage = checkActiveImage(hit);
  if (!activeImage)
    return sk_empty;

  const auto activeImageUid = ms_appData->activeImageUid();

  // Number of mm per pixel in the x and y directions:
  const glm::vec2 mmPerPixel = camera::worldPixelSize(
    ms_appData->windowData().viewport(), hit.view->camera(), hit.view->viewClip_T_windowClip()
  );

  const auto [subjectPlaneEquation, subjectPlanePoint] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(),
    -hit.worldFrontAxis,
    glm::vec3{hit.worldPos_offsetApplied}
  );

  // Use the image slice scroll distance as the threshold for plane distances
  const float planeDistanceThresh = 0.5f
                                    * data::sliceScrollDistance(hit.worldFrontAxis, *activeImage);

  // Find all annotations for the active image that match this plane
  const auto uidsOfAnnotsOnImageSlice = data::findAnnotationsForImage(
    *ms_appData, *activeImageUid, subjectPlaneEquation, planeDistanceThresh
  );

  float closestDistance_inPixels = std::numeric_limits<float>::max();
  size_t indexOfClosest = 0;

  // Pairs of {annotationUid, vertex index}
  std::vector<std::pair<uuids::uuid, size_t> > annotsAndVertices;

  // Loop over all annotations and determine whether we're atop a vertex:
  for (const auto& annotUid : uidsOfAnnotsOnImageSlice)
  {
    Annotation* annot = ms_appData->annotation(annotUid);
    if (!annot)
    {
      spdlog::error("Null annotation {}", annotUid);
      continue;
    }

    const glm::vec2 hoveredPoint = annot->projectSubjectPointToAnnotationPlane(subjectPlanePoint);

    size_t vertexIndex = 0;

    if (0 == annot->numBoundaries())
      continue;

    for (const glm::vec2& annotPoint : annot->getBoundaryVertices(OUTER_BOUNDARY))
    {
      const glm::vec2 dist_inMM = glm::abs(annotPoint - hoveredPoint);
      const float dist_inPixels = glm::length(dist_inMM / mmPerPixel);

      if (dist_inPixels < sk_distThresh_inPixels)
      {
        annotsAndVertices.emplace_back(std::make_pair(annotUid, vertexIndex));

        if (dist_inPixels < closestDistance_inPixels)
        {
          closestDistance_inPixels = dist_inPixels;
          indexOfClosest = annotsAndVertices.size() - 1;
        }
      }

      ++vertexIndex;
    }
  }

  if (0 != indexOfClosest && indexOfClosest < annotsAndVertices.size())
  {
    // Put the closest vertex in the first position
    std::swap(annotsAndVertices[0], annotsAndVertices[indexOfClosest]);
  }

  return annotsAndVertices;
}

std::vector<uuids::uuid> AnnotationStateMachine::findHitPolygon(const ViewHit& hit)
{
  static const std::vector<uuids::uuid> sk_empty;

  if (!checkAppData())
    return sk_empty;

  if (!hit.view)
  {
    spdlog::error("Null view");
    return sk_empty;
  }

  const Image* activeImage = checkActiveImage(hit);
  if (!activeImage)
    return sk_empty;

  const auto activeImageUid = ms_appData->activeImageUid();

  const auto [subjectPlaneEquation, subjectPlanePoint] = math::computeSubjectPlaneEquation(
    activeImage->transformations().subject_T_worldDef(),
    -hit.worldFrontAxis,
    glm::vec3{hit.worldPos_offsetApplied}
  );

  // Use the image slice scroll distance as the threshold for plane distances
  const float planeDistanceThresh = 0.5f
                                    * data::sliceScrollDistance(hit.worldFrontAxis, *activeImage);

  // Find all annotations for the active image that match this plane
  const auto uidsOfAnnotsOnImageSlice = data::findAnnotationsForImage(
    *ms_appData, *activeImageUid, subjectPlaneEquation, planeDistanceThresh
  );

  std::vector<uuids::uuid> annots;

  // Loop over all annotations and determine whether we're atop one of the polygons:
  for (const auto& annotUid : uidsOfAnnotsOnImageSlice)
  {
    Annotation* annot = ms_appData->annotation(annotUid);
    if (!annot)
    {
      spdlog::error("Null annotation {}", annotUid);
      continue;
    }

    if (0 == annot->numBoundaries())
      continue;

    if (math::pnpoly(
          annot->getBoundaryVertices(OUTER_BOUNDARY),
          annot->projectSubjectPointToAnnotationPlane(subjectPlanePoint)
        ))
    {
      annots.emplace_back(annotUid);
    }
  }

  return annots;
}

void AnnotationStateMachine::synchronizeAnnotationHighlights()
{
  if (!checkAppData())
    return;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  for (const auto& imageUid : ms_appData->imageUidsOrdered())
  {
    const auto activeAnnotUid = ms_appData->imageToActiveAnnotationUid(imageUid);

    for (const auto& annotUid : ms_appData->annotationsForImage(imageUid))
    {
      Annotation* annot = ms_appData->annotation(annotUid);
      if (!annot)
      {
        spdlog::error("Null annotation {}", annotUid);
        continue;
      }

      // Remove all highlights
      annot->setHighlighted(false);
      annot->removeVertexHighlights();
      annot->removeEdgeHighlights();

      if (imageUid == *activeImageUid)
      {
        if (activeAnnotUid && (*activeAnnotUid == annotUid))
        {
          // Highlight the active annotation (of the active image)
          annot->setHighlighted(true);

          if (ms_selectedVertex)
          {
            annot->setVertexHighlight({OUTER_BOUNDARY, *ms_selectedVertex}, true);
          }
        }

        if (ms_hoveredAnnotUid && (*ms_hoveredAnnotUid == annotUid))
        {
          // Do not highlight the hovered annotation; only the hovered vertex
          // annot->setHighlighted( true );

          if (ms_hoveredVertex)
          {
            annot->setVertexHighlight({OUTER_BOUNDARY, *ms_hoveredVertex}, true);
          }
        }
      }
    }
  }
}

void AnnotationStateMachine::hoverAnnotationAndVertex(const ViewHit& hit)
{
  if (!checkViewSelection(hit))
    return;

  // Clear current hover
  ms_hoveredAnnotUid = std::nullopt;
  ms_hoveredVertex = std::nullopt;

  const auto hitVertices = findHitVertices(hit);

  if (!hitVertices.empty())
  {
    // Set hover for first vertex/annotation that was hit
    ms_hoveredAnnotUid = hitVertices[0].first;
    ms_hoveredVertex = hitVertices[0].second;
  }

  synchronizeAnnotationHighlights();
}

bool AnnotationStateMachine::selectAnnotationAndVertex(const ViewHit& hit)
{
  if (!checkAppData())
    return false;
  if (!checkViewSelection(hit))
    return false;

  bool selectedVertex = false;

  // Clear current vertex selection
  ms_selectedVertex = std::nullopt;

  // Note: do not clear the active annotation
  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return false;

  // Deselect the annotation
  ms_appData->assignActiveAnnotationUidToImage(*activeImageUid, std::nullopt);

  const auto hitVertices = findHitVertices(hit);

  if (!hitVertices.empty())
  {
    // Select first vertex that was hit
    /// @todo Allow selecting more than one vertex.
    /// Add/subtract from current selection using shift modifier.
    setSelectedAnnotationAndVertex(hitVertices[0].first, hitVertices[0].second);
    selectedVertex = true;
  }

  synchronizeAnnotationHighlights();
  return selectedVertex;
}

bool AnnotationStateMachine::selectAnnotation(const ViewHit& hit)
{
  if (!checkAppData())
    return false;
  if (!checkViewSelection(hit))
    return false;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return false;

  // Deselect the annotation
  ms_appData->assignActiveAnnotationUidToImage(*activeImageUid, std::nullopt);

  bool selectedPolygon = false;

  const auto hitPolygons = findHitPolygon(hit);

  if (!hitPolygons.empty())
  {
    // Select the last (top-most) polygon that was hit:
    setSelectedAnnotationAndVertex(hitPolygons.back(), std::nullopt);
    selectedPolygon = true;
  }

  synchronizeAnnotationHighlights();
  return selectedPolygon;
}

void AnnotationStateMachine::setSelectedAnnotationAndVertex(
  const uuids::uuid& annotUid, const std::optional<size_t>& vertexIndex
)
{
  if (!checkAppData())
    return;

  const auto activeImageUid = ms_appData->activeImageUid();
  if (!activeImageUid)
    return;

  if (ms_appData->assignActiveAnnotationUidToImage(*activeImageUid, annotUid))
  {
    if (vertexIndex)
    {
      const Annotation* annot = ms_appData->annotation(annotUid);

      if (annot && annot->numBoundaries() > 0)
      {
        if (annot->getBoundaryVertices(OUTER_BOUNDARY).size() > *vertexIndex)
        {
          // It's a valid vertex index:
          ms_selectedVertex = *vertexIndex;
          synchronizeAnnotationHighlights();
        }
        else
        {
          spdlog::warn(
            "Cannot select invalid vertex at index {} for annotation {}", *vertexIndex, annotUid
          );
        }
      }
    }
    else
    {
      // Remove the selection
      ms_selectedVertex = std::nullopt;
    }
  }
  else
  {
    spdlog::error("Unable to assign active annotation {} to image {}", annotUid, *activeImageUid);
  }
}

} // namespace state

/// Initial state definition
FSM_INITIAL_STATE(state::AnnotationStateMachine, state::AnnotationOffState)
