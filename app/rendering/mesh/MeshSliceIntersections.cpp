#include "rendering/Rendering.h"

#include "common/DirectionMaps.h"
#include "image/Image.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"
#include "rendering/PrivateMethods.h"
#include "rendering/gl/OpenGLRenderState.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/vector/VectorDrawing.h"
#include "windowing/View.h"
#include "windowing/WindowData.h"

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <nanovg.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstddef>
#include <limits>

namespace
{

bool sameVector(const glm::vec3& first, const glm::vec3& second)
{
  return first.x == second.x && first.y == second.y && first.z == second.z;
}

bool sameMatrix(const glm::mat4& first, const glm::mat4& second)
{
  for (glm::length_t column = 0; column < 4; ++column) {
    for (glm::length_t row = 0; row < 4; ++row) {
      if (first[column][row] != second[column][row]) {
        return false;
      }
    }
  }
  return true;
}

bool finiteInvertible(const glm::mat4& matrix)
{
  for (glm::length_t column = 0; column < 4; ++column) {
    for (glm::length_t row = 0; row < 4; ++row) {
      if (!std::isfinite(matrix[column][row])) {
        return false;
      }
    }
  }
  return std::abs(glm::determinant(glm::mat3{matrix})) > std::numeric_limits<float>::epsilon();
}

glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point)
{
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed} / transformed.w;
}

} // namespace

void Rendering::renderImportedMeshIntersectionsForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  const CurrentImages& imageSegPairs)
{
  const glm::vec3 worldPlaneNormal = helper::worldDirection(view.camera(), Directions::View::Front);
  auto& viewCache = m_importedMeshSliceIntersections[view.uid()];
  bool beganNvgFrame = false;

  for (const ImgSegPair& pair : imageSegPairs) {
    if (!pair.first) {
      continue;
    }
    const uuids::uuid& imageUid = *pair.first;
    for (const uuids::uuid& meshUid : m_appData.imageToImportedMeshUids(imageUid)) {
      const mesh::MeshRecord* imported = m_appData.importedMesh(meshUid);
      if (!imported || !imported->display.visibleIn2d || imported->display.opacity <= 0.0f) {
        continue;
      }

      const auto prepared = prepareImportedMeshGeometry(imageUid, meshUid);
      if (!prepared || !prepared->geometry || !finiteInvertible(prepared->world_T_mesh)) {
        continue;
      }

      const glm::mat4 mesh_T_world = glm::inverse(prepared->world_T_mesh);
      const auto [meshPlaneEquation, meshPlaneOrigin] =
        math::computeSubjectPlaneEquation(mesh_T_world, worldPlaneNormal, worldOffsetXhairs);
      const glm::vec3 meshPlaneNormal{meshPlaneEquation};

      auto& intersectorCache = m_importedMeshPlaneIntersectors[meshUid];
      if (intersectorCache.geometryVersion != prepared->geometryVersion) {
        intersectorCache.geometryVersion = prepared->geometryVersion;
        intersectorCache.intersector.reset();
        auto created = rendering::mesh::MeshPlaneIntersector::create(*prepared->geometry);
        if (!created) {
          spdlog::warn("Cannot create 2D intersection contours for imported mesh {}: {}", meshUid, created.error());
          continue;
        }
        intersectorCache.intersector = std::make_unique<rendering::mesh::MeshPlaneIntersector>(std::move(*created));
      }
      if (!intersectorCache.intersector) {
        continue;
      }

      auto& sliceCache = viewCache[meshUid];
      const bool needsIntersection = sliceCache.geometryVersion != prepared->geometryVersion ||
                                     !sameVector(sliceCache.meshPlaneOrigin, meshPlaneOrigin) ||
                                     !sameVector(sliceCache.meshPlaneNormal, meshPlaneNormal) ||
                                     !sameMatrix(sliceCache.world_T_mesh, prepared->world_T_mesh);
      if (needsIntersection) {
        const auto intersections = intersectorCache.intersector->intersect(meshPlaneOrigin, meshPlaneNormal);
        if (!intersections) {
          spdlog::warn(
            "Cannot intersect imported mesh {} with the current 2D slice: {}",
            meshUid,
            intersections.error());
          continue;
        }
        sliceCache.geometryVersion = prepared->geometryVersion;
        sliceCache.meshPlaneOrigin = meshPlaneOrigin;
        sliceCache.meshPlaneNormal = meshPlaneNormal;
        sliceCache.world_T_mesh = prepared->world_T_mesh;
        sliceCache.worldSegments.clear();
        sliceCache.worldSegments.reserve(intersections->size());
        for (const auto& segment : *intersections) {
          sliceCache.worldSegments.push_back(
            {.first = transformPoint(prepared->world_T_mesh, segment.first),
             .second = transformPoint(prepared->world_T_mesh, segment.second)});
        }
      }

      if (sliceCache.worldSegments.empty()) {
        continue;
      }
      if (!beganNvgFrame) {
        startNvgFrame(m_nvg, m_appData.windowData().viewport());
        nvgScissor(
          m_nvg,
          miewportViewBounds.viewport[0],
          miewportViewBounds.viewport[1],
          miewportViewBounds.viewport[2],
          miewportViewBounds.viewport[3]);
        nvgLineCap(m_nvg, NVG_ROUND);
        nvgLineJoin(m_nvg, NVG_ROUND);
        beganNvgFrame = true;
      }

      nvgStrokeColor(
        m_nvg,
        nvgRGBAf(
          imported->display.baseColor.r,
          imported->display.baseColor.g,
          imported->display.baseColor.b,
          imported->display.opacity));
      nvgStrokeWidth(m_nvg, 1.5f);
      nvgBeginPath(m_nvg);
      for (const auto& segment : sliceCache.worldSegments) {
        const glm::vec2 first = helper::miewport_T_world(
          m_appData.windowData().viewport(),
          view.camera(),
          view.windowClip_T_viewClip(),
          segment.first);
        const glm::vec2 second = helper::miewport_T_world(
          m_appData.windowData().viewport(),
          view.camera(),
          view.windowClip_T_viewClip(),
          segment.second);
        nvgMoveTo(m_nvg, first.x, first.y);
        nvgLineTo(m_nvg, second.x, second.y);
      }
      nvgStroke(m_nvg);
    }
  }

  if (beganNvgFrame) {
    endNvgFrame(m_nvg);
    rendering::restoreOpenGLRenderState();
  }
}
