#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <uuid.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace interaction
{

/// Presentation state shared by all manual-transformation guides.
struct TransformationGuidePresentation
{
  bool dragging = false; //!< True while the interaction is still active
  float opacity = 0.0f;  //!< Fade opacity in the closed interval [0, 1]
};

/// Immutable translation data consumed by transformation-guide renderers.
struct TranslationGuide
{
  glm::vec3 startWorld{0.0f};        //!< World-space pointer position at the start of the gesture
  glm::vec3 displacementWorld{0.0f}; //!< Exact accumulated translation applied to the image
  TransformationGuidePresentation presentation;
};

/// Immutable rotation data consumed by transformation-guide renderers.
struct RotationGuide
{
  glm::vec3 centerWorld{0.0f};                     //!< Fixed world-space center of rotation
  glm::vec3 referenceWorld{0.0f};                  //!< Initial pointer position defining the protractor radius
  glm::quat rotationWorld{1.0f, 0.0f, 0.0f, 0.0f}; //!< Exact accumulated world-space rotation
  TransformationGuidePresentation presentation;
};

/// Immutable scale data consumed by transformation-guide renderers.
struct ScaleGuide
{
  glm::vec3 centerWorld{0.0f};                    //!< Fixed world-space center of scaling
  glm::vec3 pointerStartWorld{0.0f};              //!< World-space pointer position at the start of the gesture
  glm::vec3 pointerCurrentWorld{0.0f};            //!< Latest world-space pointer position
  glm::vec3 initialScale{1.0f};                   //!< Absolute scale at gesture start
  glm::vec3 currentScale{1.0f};                   //!< Current absolute scale
  std::array<glm::vec3, 8> initialWorldCorners{}; //!< Image bounds before this scale gesture
  std::array<glm::vec3, 8> currentWorldCorners{}; //!< Image bounds after the latest scale update
  TransformationGuidePresentation presentation;
};

using TransformationGuide = std::variant<TranslationGuide, RotationGuide, ScaleGuide>;

/// Principal-axis representation of a world-space rotation.
struct RotationAxisAngle
{
  glm::vec3 axisWorld{0.0f, 0.0f, 1.0f}; //!< Unit world-space rotation axis
  float angleRadians = 0.0f;             //!< Shortest equivalent rotation angle in radians
};

/// Transient state for the currently active or recently completed manual transformation.
class TransformationGuideState
{
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  static constexpr std::chrono::milliseconds FadeDuration{500};

  /// Begin a translation at the world-space pointer position where the drag started.
  void beginTranslation(const uuids::uuid& sourceUid, const glm::vec3& startWorld);

  /// Accumulate an exact world-space translation that was applied to the image.
  void appendTranslation(const glm::vec3& appliedWorldDelta);

  /// Begin a rotation about a fixed center using the initial pointer as its radial reference.
  void beginRotation(const uuids::uuid& sourceUid, const glm::vec3& centerWorld, const glm::vec3& referenceWorld);

  /// Accumulate an exact incremental world-space rotation that was applied to the image.
  void appendRotation(const glm::quat& appliedWorldRotation);

  /// Begin scaling and capture the image bounds before the first scale update.
  void beginScale(
    const uuids::uuid& sourceUid,
    const glm::vec3& centerWorld,
    const glm::vec3& pointerStartWorld,
    const glm::vec3& initialScale,
    const std::array<glm::vec3, 8>& initialWorldCorners);

  /// Record the absolute scale and image bounds after the latest applied update.
  void updateScale(
    const glm::vec3& pointerCurrentWorld,
    const glm::vec3& currentScale,
    const std::array<glm::vec3, 8>& currentWorldCorners);

  /// Complete the active gesture and begin its short visual fade.
  void finish(TimePoint now = Clock::now());

  /// Clear active and fading guide data immediately.
  void clear();

  /// Return renderable guide data, or no value once the fade has elapsed.
  std::optional<TransformationGuide> guide(TimePoint now = Clock::now()) const;

  /// Return the view in which the current or fading transformation gesture began.
  const std::optional<uuids::uuid>& sourceViewUid() const;

  /// Return whether a guide of the requested type is currently being dragged.
  template<typename Guide>
  bool isDragging() const
  {
    return m_dragging && m_guide && std::holds_alternative<Guide>(*m_guide);
  }

private:
  std::optional<TransformationGuide> m_guide;
  std::optional<uuids::uuid> m_sourceViewUid;
  bool m_dragging = false;
  std::optional<TimePoint> m_finishedAt;
};

/// Return whether a presentation-ready guide represents an active drag.
bool guideIsDragging(const TransformationGuide& guide);

/// Return the three world-space endpoints of the translation's x, y, and z components.
std::array<glm::vec3, 3> translationComponentEndpoints(const TranslationGuide& guide);

/// Convert a rotation guide to a stable shortest-path world-space axis and angle.
RotationAxisAngle rotationAxisAngle(const RotationGuide& guide);

/// Sample the rotation protractor arc in world space, including both endpoints.
std::vector<glm::vec3> rotationArcWorldPoints(const RotationGuide& guide, std::size_t segmentCount);

/// Return scale factors relative to the start of the current gesture.
glm::vec3 relativeScaleFactors(const ScaleGuide& guide);

/// Compute the ordered polygon where a transformed image box intersects a world-space slice plane.
/// The corners must use binary-axis order: 000, 100, 010, 110, 001, 101, 011, 111.
std::vector<glm::vec3> boxPlaneIntersectionOutline(
  const std::array<glm::vec3, 8>& worldCorners,
  const glm::vec3& planeOriginWorld,
  const glm::vec3& planeNormalWorld);

} // namespace interaction
