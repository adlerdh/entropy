#include "image/ImageHeader.h"
#include "image/ImageTransformations.h"
#include "image/ImageUtility.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>

namespace
{

ImageIoInfo makeIoInfo(ComponentType componentType, uint32_t numComponents, glm::uvec3 dims)
{
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = "header-transform.nrrd";
  info.m_fileInfo.m_fileTypeString = "Nrrd";
  info.m_componentInfo.m_componentType = componentType;
  info.m_componentInfo.m_componentTypeString = componentTypeString(componentType);
  info.m_componentInfo.m_componentSizeInBytes = componentType == ComponentType::UInt16 ? 2 : 4;

  info.m_pixelInfo.m_pixelType = numComponents == 1 ? PixelType::Scalar : PixelType::Vector;
  info.m_pixelInfo.m_pixelTypeString = numComponents == 1 ? "scalar" : "vector";
  info.m_pixelInfo.m_numComponents = numComponents;
  info.m_pixelInfo.m_pixelStrideInBytes = info.m_componentInfo.m_componentSizeInBytes * numComponents;

  info.m_sizeInfo.m_imageSizeInPixels = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
  info.m_sizeInfo.m_imageSizeInComponents = info.m_sizeInfo.m_imageSizeInPixels * numComponents;
  info.m_sizeInfo.m_imageSizeInBytes =
    info.m_sizeInfo.m_imageSizeInComponents * info.m_componentInfo.m_componentSizeInBytes;

  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {dims.x, dims.y, dims.z};
  info.m_spaceInfo.m_origin = {-1.0, 2.0, 3.0};
  info.m_spaceInfo.m_spacing = {0.5, 1.5, 2.5};
  info.m_spaceInfo.m_directions = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  return info;
}

ImageIoInfo make2dIoInfo()
{
  ImageIoInfo info = makeIoInfo(ComponentType::UInt16, 1, glm::uvec3(3, 2, 1));
  info.m_sizeInfo.m_imageSizeInPixels = 6;
  info.m_sizeInfo.m_imageSizeInComponents = 6;
  info.m_sizeInfo.m_imageSizeInBytes = 12;
  info.m_spaceInfo.m_numDimensions = 2;
  info.m_spaceInfo.m_dimensions = {3, 2};
  info.m_spaceInfo.m_origin = {4.0, 5.0};
  info.m_spaceInfo.m_spacing = {0.75, 1.25};
  info.m_spaceInfo.m_directions = {{0.0, 1.0}, {-1.0, 0.0}};
  return info;
}

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
}

void checkMat4Near(const glm::mat4& actual, const glm::mat4& expected)
{
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      CHECK(actual[c][r] == Catch::Approx(expected[c][r]).margin(1.0e-5));
    }
  }
}

} // namespace

TEST_CASE("ImageHeader expands 2D image metadata into Entropy's 3D model", "[image][header]")
{
  const ImageIoInfo info = make2dIoInfo();
  ImageHeader header(info, info, false);

  CHECK(header.existsOnDisk());
  CHECK(header.fileName() == "header-transform.nrrd");
  CHECK(header.pixelDimensions() == glm::uvec3(3, 2, 1));
  checkVec3(header.origin(), glm::vec3(4.0f, 5.0f, 0.0f));
  checkVec3(header.spacing(), glm::vec3(0.75f, 1.25f, 1.0f));
  CHECK(header.numPixels() == 6);
  CHECK(header.numComponentsPerPixel() == 1);
  CHECK_FALSE(header.interleavedComponents());

  checkVec3(header.pixelBBoxCorners().front(), glm::vec3(-0.5f));
  CHECK(header.subjectBBoxCorners().size() == 8);
  CHECK(header.subjectBBoxSize().x > 0.0f);
  CHECK(header.subjectBBoxSize().y > 0.0f);
}

TEST_CASE("ImageHeaderOverrides preserve original geometry and compute orthogonal directions", "[image][header]")
{
  const ImageHeaderOverrides empty{};
  CHECK_FALSE(empty.m_useIdentityPixelSpacings);
  CHECK_FALSE(empty.m_useZeroPixelOrigin);
  CHECK_FALSE(empty.m_useIdentityPixelDirections);
  CHECK_FALSE(empty.m_snapToClosestOrthogonalPixelDirections);
  CHECK(empty.m_originalDims == glm::uvec3(0));
  checkVec3(empty.m_originalSpacing, glm::vec3(1.0f));
  checkVec3(empty.m_originalOrigin, glm::vec3(0.0f));
  CHECK(empty.m_originalDirs == glm::mat3(1.0f));

  const glm::mat3 originalDirs{glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
  const ImageHeaderOverrides overrides(
    glm::uvec3(3, 4, 5),
    glm::vec3(0.5f, 1.5f, 2.5f),
    glm::vec3(10.0f, 20.0f, 30.0f),
    originalDirs);

  CHECK(overrides.m_originalDims == glm::uvec3(3, 4, 5));
  checkVec3(overrides.m_originalSpacing, glm::vec3(0.5f, 1.5f, 2.5f));
  checkVec3(overrides.m_originalOrigin, glm::vec3(10.0f, 20.0f, 30.0f));
  CHECK(overrides.m_originalDirs == originalDirs);
  CHECK(overrides.m_closestOrthogonalDirs == originalDirs);
}

TEST_CASE("ImageHeader component adjustment and overrides update derived metadata", "[image][header]")
{
  const ImageIoInfo info = makeIoInfo(ComponentType::UInt16, 3, glm::uvec3(2, 3, 4));
  ImageHeader header(info, info, true);

  CHECK(header.interleavedComponents());
  CHECK(header.fileImageSizeInBytes() == 2 * 3 * 24);
  CHECK(header.memoryImageSizeInBytes() == 2 * 3 * 24);

  header.setNumComponentsPerPixel(2);
  CHECK(header.numComponentsPerPixel() == 2);
  CHECK(header.fileImageSizeInBytes() == 2 * 2 * 24);

  header.setNumComponentsPerPixel(0);
  CHECK(header.numComponentsPerPixel() == 2);

  header.adjustComponents(ComponentType::Float32, 1);
  CHECK(header.pixelType() == PixelType::Scalar);
  CHECK(header.fileComponentType() == ComponentType::Float32);
  CHECK(header.memoryComponentType() == ComponentType::Float32);
  CHECK(header.fileImageSizeInBytes() == 4 * 24);

  ImageHeaderOverrides overrides(header.pixelDimensions(), header.spacing(), header.origin(), header.directions());
  overrides.m_useIdentityPixelSpacings = true;
  overrides.m_useZeroPixelOrigin = true;
  overrides.m_useIdentityPixelDirections = true;
  header.setHeaderOverrides(overrides);

  checkVec3(header.spacing(), glm::vec3(1.0f));
  checkVec3(header.origin(), glm::vec3(0.0f));
  CHECK(header.directions() == glm::mat3(1.0f));
}

TEST_CASE(
  "ImageTransformations honor lock, enable flags, direct affine matrices, and header overrides",
  "[image][transform]")
{
  ImageTransformations tx(
    glm::uvec3(4, 2, 1),
    glm::vec3(0.5f, 2.0f, 3.0f),
    glm::vec3(10.0f, 20.0f, 30.0f),
    glm::mat3(1.0f));

  CHECK(tx.is_worldDef_T_affine_locked());
  tx.set_worldDef_T_affine_translation(glm::vec3(1.0f, 2.0f, 3.0f));
  checkVec3(tx.get_worldDef_T_affine_translation(), glm::vec3(0.0f));

  tx.set_worldDef_T_affine_locked(false);
  tx.set_worldDef_T_affine_translation(glm::vec3(1.0f, 2.0f, 3.0f));
  tx.set_worldDef_T_affine_scale(glm::vec3(2.0f, 3.0f, 4.0f));
  CHECK(tx.get_worldDef_T_affine()[3][0] == Catch::Approx(1.0f));
  CHECK(tx.get_worldDef_T_affine()[0][0] == Catch::Approx(2.0f));

  const glm::mat4 manual = tx.get_worldDef_T_affine();
  tx.set_enable_worldDef_T_affine(false);
  CHECK(tx.get_worldDef_T_affine() == glm::mat4(1.0f));
  CHECK(tx.worldDef_T_subject() == glm::mat4(1.0f));
  tx.set_enable_worldDef_T_affine(true);
  checkMat4Near(tx.get_worldDef_T_affine(), manual);

  const glm::mat4 affine = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f));
  tx.set_affine_T_subject(affine);
  checkMat4Near(tx.get_affine_T_subject(), affine);
  checkMat4Near(tx.worldDef_T_subject(), tx.get_worldDef_T_affine() * affine);
  tx.set_enable_affine_T_subject(false);
  CHECK(tx.get_affine_T_subject() == glm::mat4(1.0f));

  tx.reset_worldDef_T_affine();
  CHECK(tx.get_worldDef_T_affine() == glm::mat4(1.0f));

  tx.set_affine_T_subject_fileName("affine.txt");
  REQUIRE(tx.get_affine_T_subject_fileName().has_value());
  CHECK(tx.get_affine_T_subject_fileName()->filename() == "affine.txt");

  ImageHeaderOverrides overrides(
    glm::uvec3(4, 2, 1),
    glm::vec3(0.5f, 2.0f, 3.0f),
    glm::vec3(10.0f, 20.0f, 30.0f),
    glm::mat3(1.0f));
  overrides.m_useIdentityPixelSpacings = true;
  overrides.m_useZeroPixelOrigin = true;
  tx.setHeaderOverrides(overrides);
  checkVec3(glm::vec3(tx.subject_T_pixel() * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)), glm::vec3(1.0f, 1.0f, 0.0f));
}
