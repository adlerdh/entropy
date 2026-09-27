#include "image/Image.h"
#include "logic/annotation/Annotation.h"
#include "logic/annotation/AnnotPolygon.tpp"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/segmentation/AnnotationSegmentation.h"
#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

namespace
{
Image makeSegmentation(bool rotated)
{
  constexpr std::size_t side = 64;
  ImageIoInfo info;
  info.m_componentInfo.m_componentType = ComponentType::UInt16;
  info.m_componentInfo.m_componentSizeInBytes = 2;
  info.m_pixelInfo.m_pixelType = PixelType::Scalar;
  info.m_pixelInfo.m_numComponents = 1;
  info.m_pixelInfo.m_pixelStrideInBytes = 2;
  info.m_sizeInfo.m_imageSizeInPixels = side * side;
  info.m_sizeInfo.m_imageSizeInComponents = side * side;
  info.m_sizeInfo.m_imageSizeInBytes = side * side * sizeof(uint16_t);
  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {64, 64, 1};
  info.m_spaceInfo.m_spacing = {1, 1, 1};
  info.m_spaceInfo.m_origin = {-32, -32, 0};

  const double c = std::sqrt(0.5);
  info.m_spaceInfo.m_directions = rotated ? std::vector<std::vector<double>>{{c, c, 0}, {-c, c, 0}, {0, 0, 1}}
                                          : std::vector<std::vector<double>>{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

  if (rotated) info.m_spaceInfo.m_origin = {0, -32 * std::sqrt(2.0), 0};
  ImageHeader header(info, info, false);
  std::vector<uint16_t> labels(side * side, 0);

  return Image::fromCopiedData(
    header,
    "seg",
    Image::ImageRepresentation::Segmentation,
    Image::MultiComponentBufferType::SeparateImages,
    {labels.data()});
}
} // namespace

TEST_CASE(
  "Annotation rasterization covers rotated bounds and preserves holes",
  "[annotation][segmentation][regression]")
{
  bool rotated = false;
  SECTION("axis aligned") {}
  SECTION("rotated grid")
  {
    rotated = true;
  }

  auto seg = makeSegmentation(rotated);
  Annotation annotation("square with hole", glm::vec4{1}, {0, 0, 1, 0});
  annotation.polygon().setAllVertices(
    {{{-10, -10}, {10, -10}, {10, 10}, {-10, 10}}, {{-3, -3}, {-3, 3}, {3, 3}, {3, -3}}});
  annotation.setClosed(true);
  fillSegmentationWithPolygon(seg, &annotation, 1, 0, true, [](const auto&, const auto&, const auto&, const auto*) {});

  for (int j = 0; j < 64; ++j) {
    for (int i = 0; i < 64; ++i) {
      const glm::vec3 subject = seg.transformations().subject_T_pixel() * glm::vec4{i, j, 0, 1};
      const auto p = annotation.projectSubjectPointToAnnotationPlane(subject);
      const auto value = seg.value<int>(0, i, j, 0);
      REQUIRE(value);

      if (std::abs(p.x) < 2 && std::abs(p.y) < 2) CHECK(*value == 0);
      if (std::abs(p.x) < 9 && std::abs(p.y) < 9 && (std::abs(p.x) > 4 || std::abs(p.y) > 4)) CHECK(*value == 1);
    }
  }
}

TEST_CASE("Voxel landmarks export in physical subject coordinates", "[landmarks][regression]")
{
  LandmarkGroup group;
  PointRecord<glm::vec3> point({1, 2, 3});
  group.addPoint(7, point);
  const glm::mat4 transform =
    glm::translate(glm::mat4{1}, glm::vec3{10, 20, 30}) * glm::scale(glm::mat4{1}, glm::vec3{2, 3, 4});
  CHECK(group.pointsInSubjectSpace(transform).at(7).getPosition() == glm::vec3(1, 2, 3));
  group.setInVoxelSpace(true);
  CHECK(group.pointsInSubjectSpace(transform).at(7).getPosition() == glm::vec3(12, 26, 42));
  CHECK(group.getPoints().at(7).getPosition() == glm::vec3(1, 2, 3));
}

TEST_CASE("Annotations outside the segmentation have no voxel footprint", "[annotation][segmentation][regression]")
{
  auto seg = makeSegmentation(false);
  Annotation annotation("outside", glm::vec4{1}, {0, 0, 1, 0});
  annotation.polygon().setAllVertices({{{1e12f, 1e12f}, {2e12f, 1e12f}, {2e12f, 2e12f}, {1e12f, 2e12f}}});
  annotation.setClosed(true);
  bool updated = false;
  fillSegmentationWithPolygon(seg, &annotation, 1, 0, true, [&](const auto&, const auto&, const auto&, const auto*) {
    updated = true;
  });
  CHECK_FALSE(updated);
}
