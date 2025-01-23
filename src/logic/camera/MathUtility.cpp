#include "logic/camera/MathUtility.h"
#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "common/Exception.hpp"
#include "common/MathFuncs.h"
#include "common/Viewport.h"

#include <glm/gtc/matrix_inverse.hpp>

#define EPSILON 0.000001

#define CROSS(dest, v1, v2) \
  { \
    dest[0] = v1[1] * v2[2] - v1[2] * v2[1]; \
    dest[1] = v1[2] * v2[0] - v1[0] * v2[2]; \
    dest[2] = v1[0] * v2[1] - v1[1] * v2[0]; \
  }

#define DOT(v1, v2) (v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2])

#define SUB(dest, v1, v2) \
  { \
    dest[0] = v1[0] - v2[0]; \
    dest[1] = v1[1] - v2[1]; \
    dest[2] = v1[2] - v2[2]; \
  }

namespace math
{

std::pair<glm::vec3, glm::vec3> buildOrthonormalBasis_branchless(const glm::vec3& n)
{
  float sign = std::copysign(1.0f, n.z);
  const float a = -1.0f / (sign + n.z);
  const float b = n.x * n.y * a;

  return {{1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x}, {b, sign + n.y * n.y * a, -n.y}};
}

std::pair<glm::vec3, glm::vec3> buildOrthonormalBasis(const glm::vec3& n)
{
  if (n.z < 0.0f)
  {
    const float a = 1.0f / (1.0f - n.z);
    const float b = n.x * n.y * a;

    return {{1.0f - n.x * n.x * a, -b, n.x}, {b, n.y * n.y * a - 1.0f, -n.y}};
  }
  else
  {
    const float a = 1.0f / (1.0f + n.z);
    const float b = -n.x * n.y * a;

    return {{1.0f - n.x * n.x * a, b, -n.x}, {b, 1.0f - n.y * n.y * a, -n.y}};
  }
}

glm::vec3 convertVecToRGB(const glm::vec3& v)
{
  const glm::vec3 c = glm::abs(v);
  return c / glm::compMax(c);
}

glm::u8vec3 convertVecToRGB_uint8(const glm::vec3& v)
{
  return glm::u8vec3{255.0f * convertVecToRGB(v)};
}

std::vector<uint32_t> sortCounterclockwise(const std::vector<glm::vec2>& points)
{
  if (points.empty())
  {
    return std::vector<uint32_t>{};
  }
  else if (1 == points.size())
  {
    return std::vector<uint32_t>{0};
  }

  glm::vec2 center{0.0f, 0.0f};

  for (const auto& p : points)
  {
    center += p;
  }
  center = center / static_cast<float>(points.size());

  const glm::vec2 a = points[0] - center;

  std::vector<float> angles;

  for (uint32_t i = 0; i < points.size(); ++i)
  {
    const glm::vec2 b = points[i] - center;
    const float dot = a.x * b.x + a.y * b.y;
    const float det = a.x * b.y - b.x * a.y;
    const float angle = std::atan2(det, dot);
    angles.emplace_back(angle);
  }

  std::vector<uint32_t> indices(points.size());
  std::iota(std::begin(indices), std::end(indices), 0);

  std::sort(
    std::begin(indices),
    std::end(indices),
    [&angles](const uint32_t& i, const uint32_t& j) { return (angles[i] <= angles[j]); }
  );

  return indices;
}

std::vector<glm::vec2> project3dPointsToPlane(const std::vector<glm::vec3>& A)
{
  const glm::vec3 normal = glm::cross(A[1] - A[0], A[2] - A[0]);

  const glm::mat4 M = glm::lookAt(A[0] - normal, A[0], A[1] - A[0]);

  std::vector<glm::vec2> B;

  for (auto& a : A)
  {
    B.emplace_back(glm::vec2{M * glm::vec4{a, 1.0f}});
  }

  return B;
}

glm::vec3 projectPointToPlane(const glm::vec3& point, const glm::vec4& planeEquation)
{
  // Plane normal is (A, B, C):
  const glm::vec3 planeNormal{planeEquation};
  const float L = glm::length(planeNormal);

  if (L < glm::epsilon<float>())
  {
    throw_debug("Cannot project point to plane: plane normal has zero length")
  }

  // Signed distance of point to plane (positive if on same side of plane as normal vector):
  const float distancePointToPlane = glm::dot(planeEquation, glm::vec4{point, 1.0f}) / L;

  // Point projected to plane:
  return (point - distancePointToPlane * planeNormal);
}

glm::vec2 projectPointToPlaneLocal2dCoords(
  const glm::vec3& point,
  const glm::vec4& planeEquation,
  const glm::vec3& planeOrigin,
  const std::pair<glm::vec3, glm::vec3>& planeAxes
)
{
  const glm::vec3 pointProjectedToPlane = projectPointToPlane(point, planeEquation);

  // Express projected point in 2D plane coordinates:
  return glm::vec2{
    glm::dot(pointProjectedToPlane - planeOrigin, glm::normalize(planeAxes.first)),
    glm::dot(pointProjectedToPlane - planeOrigin, glm::normalize(planeAxes.second))
  };
}

void applyLayeringOffsetsToModelPositions(
  const camera::Camera& camera,
  const glm::mat4& model_T_world,
  uint32_t layer,
  std::vector<glm::vec3>& modelPositions
)
{
  if (modelPositions.empty())
  {
    return;
  }

  // Matrix for transforming vectors from Camera to Model space:
  const glm::mat3 model_T_camera_invTrans = glm::inverseTranspose(
    glm::mat3{model_T_world * camera.world_T_camera()}
  );

  // The view's Back direction transformed to Model space:
  const glm::vec3 modelTowardsViewer = glm::normalize(
    model_T_camera_invTrans * Directions::get(Directions::View::Back)
  );

  // Compute offset in World units based on first position (this choice is arbitrary)
  const float worldDepth = camera::computeSmallestWorldDepthOffset(camera, modelPositions.front());

  // Proportionally offset higher layers by more distance
  const float offsetMag = static_cast<float>(layer) * worldDepth;
  const glm::vec3 modelOffset = offsetMag * modelTowardsViewer;

  for (auto& p : modelPositions)
  {
    p += modelOffset;
  }
}

glm::mat3 computeSubjectAxesInCamera(
  const glm::mat3& camera_T_world_rotation, const glm::mat3& world_T_subject_rotation
)
{
  return glm::inverseTranspose(camera_T_world_rotation * world_T_subject_rotation);
}

std::pair<glm::vec4, glm::vec3> computeSubjectPlaneEquation(
  const glm::mat4 subject_T_world,
  const glm::vec3& worldPlaneNormal,
  const glm::vec3& worldPlanePoint
)
{
  const glm::mat4 subject_T_world_IT = glm::inverseTranspose(subject_T_world);
  const glm::vec3 subjectPlaneNormal{subject_T_world_IT * glm::vec4{worldPlaneNormal, 0.0f}};

  glm::vec4 subjectPlanePoint = subject_T_world * glm::vec4{worldPlanePoint, 1.0f};
  subjectPlanePoint /= subjectPlanePoint.w;

  return {
    math::makePlane(subjectPlaneNormal, glm::vec3{subjectPlanePoint}), glm::vec3{subjectPlanePoint}
  };
}

std::array<AnatomicalLabelPosInfo, 2> computeAnatomicalLabelsForView(
  const glm::mat4& camera_T_world, const glm::mat4& world_T_subject
)
{
  // Shortcuts for the three orthogonal anatomical directions
  static constexpr int L = 0;
  static constexpr int P = 1;
  static constexpr int S = 2;

  // Visibility and directions of the labels L, P, S in View Clip/NDC space:
  std::array<AnatomicalLabelPosInfo, 2> labels;

  // The reference subject's left, posterior, and superior directions in Camera space.
  // Columns 0, 1, and 2 of the matrix correspond to Left, Posterior, and Superior, respectively.
  const glm::mat3 axes
    = math::computeSubjectAxesInCamera(glm::mat3{camera_T_world}, glm::mat3{world_T_subject});

  const glm::mat3 axesAbs{glm::abs(axes[0]), glm::abs(axes[1]), glm::abs(axes[2])};
  const glm::mat3 axesSgn{glm::sign(axes[0]), glm::sign(axes[1]), glm::sign(axes[2])};

  // Render the two sets of labels that are closest to the view plane:
  if (axesAbs[L].z > axesAbs[P].z && axesAbs[L].z > axesAbs[S].z)
  {
    labels[0] = AnatomicalLabelPosInfo{P};
    labels[1] = AnatomicalLabelPosInfo{S};
  }
  else if (axesAbs[P].z > axesAbs[L].z && axesAbs[P].z > axesAbs[S].z)
  {
    labels[0] = AnatomicalLabelPosInfo{L};
    labels[1] = AnatomicalLabelPosInfo{S};
  }
  else if (axesAbs[S].z > axesAbs[L].z && axesAbs[S].z > axesAbs[P].z)
  {
    labels[0] = AnatomicalLabelPosInfo{L};
    labels[1] = AnatomicalLabelPosInfo{P};
  }

  // Render the translation vectors for the L (0), P (1), and S (2) labels:
  for (auto& label : labels)
  {
    const int i = label.labelIndex;

    label.viewClipDir = (axesAbs[i].x > 0.0f && axesAbs[i].y / axesAbs[i].x <= 1.0f)
                          ? glm::vec2{axesSgn[i].x, axesSgn[i].y * axesAbs[i].y / axesAbs[i].x}
                          : glm::vec2{axesSgn[i].x * axesAbs[i].x / axesAbs[i].y, axesSgn[i].y};
  }

  return labels;
}

std::array<AnatomicalLabelPosInfo, 2> computeAnatomicalLabelPosInfo(
  const FrameBounds& miewportViewBounds,
  const Viewport& windowVP,
  const camera::Camera& camera,
  const glm::mat4& world_T_subject,
  const glm::mat4& windowClip_T_viewClip,
  const glm::vec3& worldCrosshairsPos
)
{
  // Compute intersections of the anatomical label ray with the view box:
  static constexpr bool sk_doBothLabelDirs = false;

  // Compute intersections of the crosshair ray with the view box:
  static constexpr bool sk_doBothXhairDirs = true;

  const glm::mat4 miewport_T_viewClip = camera::miewport_T_viewport(windowVP.height())
                                        * camera::viewport_T_windowClip(windowVP)
                                        * windowClip_T_viewClip;

  const glm::mat3 miewport_T_viewClip_IT = glm::inverseTranspose(glm::mat3{miewport_T_viewClip});

  auto labelPosInfo = computeAnatomicalLabelsForView(camera.camera_T_world(), world_T_subject);

  const float aspectRatio = miewportViewBounds.bounds.width / miewportViewBounds.bounds.height;

  const glm::vec2 aspectRatioScale = (aspectRatio < 1.0f) ? glm::vec2{aspectRatio, 1.0f}
                                                          : glm::vec2{1.0f, 1.0f / aspectRatio};

  const glm::vec2
    miewportMinCorner(miewportViewBounds.bounds.xoffset, miewportViewBounds.bounds.yoffset);
  const glm::vec2 miewportSize(miewportViewBounds.bounds.width, miewportViewBounds.bounds.height);
  const glm::vec2 miewportCenter = miewportMinCorner + 0.5f * miewportSize;

  glm::vec4 viewClipXhairPos = camera::clip_T_world(camera) * glm::vec4{worldCrosshairsPos, 1.0f};
  viewClipXhairPos /= viewClipXhairPos.w;

  glm::vec4 miewportXhairPos = miewport_T_viewClip * viewClipXhairPos;
  miewportXhairPos /= miewportXhairPos.w;

  for (auto& label : labelPosInfo)
  {
    const glm::vec3 viewClipXhairDir{label.viewClipDir.x, label.viewClipDir.y, 0.0f};

    label.miewportXhairCenterPos = glm::vec2{miewportXhairPos};

    glm::vec2 miewportXhairDir{miewport_T_viewClip_IT * viewClipXhairDir};
    miewportXhairDir.x *= aspectRatioScale.x;
    miewportXhairDir.y *= aspectRatioScale.y;
    miewportXhairDir = glm::normalize(miewportXhairDir);

    // Intersections for the positive label (L, P, or S):
    const auto posLabelHits = math::computeRayAABoxIntersections(
      miewportCenter, miewportXhairDir, miewportMinCorner, miewportSize, sk_doBothLabelDirs
    );

    // Intersections for the negative label (R, A, or I):
    const auto negLabelHits = math::computeRayAABoxIntersections(
      miewportCenter, -miewportXhairDir, miewportMinCorner, miewportSize, sk_doBothLabelDirs
    );

    if (1 != posLabelHits.size() || 1 != negLabelHits.size())
    {
      spdlog::warn(
        "Expected two intersections when computing anatomical label positions for view. "
        "Got {} and {} intersections in the positive and negative directions, respectively.",
        posLabelHits.size(),
        negLabelHits.size()
      );
      continue;
    }

    label.miewportLabelPositions = std::array<glm::vec2, 2>{posLabelHits[0], negLabelHits[0]};

    const auto crosshairHits = math::computeRayAABoxIntersections(
      label.miewportXhairCenterPos,
      miewportXhairDir,
      miewportMinCorner,
      miewportSize,
      sk_doBothXhairDirs
    );

    if (2 != crosshairHits.size())
    {
      // Only render crosshairs when there are two intersections with the view box:
      label.miewportXhairPositions = std::nullopt;
    }
    else
    {
      label.miewportXhairPositions = std::array<glm::vec2, 2>{crosshairHits[0], crosshairHits[1]};
    }
  }

  return labelPosInfo;
}

} // namespace math
