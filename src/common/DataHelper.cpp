#include "common/DataHelper.h"

#include "image/Image.h"
#include "image/ImageUtility.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>

#include <algorithm>
#include <limits>

namespace
{

// The default voxel scale is 1.0 units
static constexpr float sk_defaultRefSpaceVoxelScale = 1.0f;
static constexpr float sk_defaultSliceScrollDistance = sk_defaultRefSpaceVoxelScale;
static constexpr float sk_defaultSliceMoveDistance = sk_defaultRefSpaceVoxelScale;

} // namespace

namespace data
{

std::vector<uuids::uuid> selectImages(
  const AppData& data, const ImageSelection& selection, const View* view
)
{
  std::vector<uuids::uuid> imageUids;

  switch (selection)
  {
  case ImageSelection::ReferenceImage:
  {
    if (const auto refUid = data.refImageUid())
    {
      imageUids.push_back(*refUid);
    }
    break;
  }
  case ImageSelection::ActiveImage:
  {
    if (const auto actUid = data.activeImageUid())
    {
      imageUids.push_back(*actUid);
    }
    break;
  }
  case ImageSelection::ReferenceAndActiveImages:
  {
    if (const auto refUid = data.refImageUid())
    {
      imageUids.push_back(*refUid);
    }

    if (const auto actUid = data.activeImageUid())
    {
      imageUids.push_back(*actUid);
    }

    break;
  }
  case ImageSelection::AllLoadedImages:
  {
    for (const auto& imageUid : data.imageUidsOrdered())
    {
      imageUids.push_back(imageUid);
    }
    break;
  }
  case ImageSelection::VisibleImagesInView:
  {
    if (view)
    {
      for (const auto& viewUid : view->visibleImages())
      {
        imageUids.push_back(viewUid);
      }
    }
    break;
  }
  case ImageSelection::FixedImageInView:
  {
    if (view && !view->metricImages().empty())
    {
      // The first image is the fixed one:
      imageUids.push_back(view->metricImages().front());
    }
    break;
  }
  case ImageSelection::MovingImageInView:
  {
    if (view)
    {
      int index = 0;
      for (const auto& viewUid : view->metricImages())
      {
        if (1 == index)
        {
          // The second image!
          imageUids.push_back(viewUid);
          break;
        }
        ++index;
      }
    }
    break;
  }
  case ImageSelection::FixedAndMovingImagesInView:
  {
    if (view)
    {
      int index = 0;
      for (const auto& viewUid : view->metricImages())
      {
        if (0 == index || 1 == index)
        {
          // The first and second images!
          imageUids.push_back(viewUid);
        }
        else
        {
          break;
        }
        ++index;
      }
    }
    break;
  }
  }

  return imageUids;
}

float sliceScrollDistance(
  const AppData& data,
  const glm::vec3& worldCameraFrontDir,
  const ImageSelection& imageSelection,
  const View* view
)
{
  if (0 == data.numImages())
  {
    return sk_defaultSliceScrollDistance;
  }

  float distance = std::numeric_limits<float>::max();
  std::size_t numImagesUsed = 0;

  for (const auto& imageUid : selectImages(data, imageSelection, view))
  {
    const Image* image = data.image(imageUid);
    if (!image)
      continue;

    // Scroll in image Pixel space along the camera's front direction:
    const glm::mat3& pixel_T_world = image->transformations().pixel_T_worldDef();
    const glm::vec3 pixelDir = glm::normalize(pixel_T_world * worldCameraFrontDir);
    const glm::vec3 pixelDirSq = pixelDir * pixelDir;

    const glm::vec3 pixelDirSqInv{
      glm::epsilonEqual(pixelDirSq.x, 0.0f, glm::epsilon<float>())
        ? std::numeric_limits<float>::max()
        : 1.0f / pixelDirSq.x,
      glm::epsilonEqual(pixelDirSq.y, 0.0f, glm::epsilon<float>())
        ? std::numeric_limits<float>::max()
        : 1.0f / pixelDirSq.y,
      glm::epsilonEqual(pixelDirSq.z, 0.0f, glm::epsilon<float>())
        ? std::numeric_limits<float>::max()
        : 1.0f / pixelDirSq.z
    };

    const float d = std::min(
      std::min(
        image->header().spacing().x
          * std::sqrt(1.0f + (pixelDirSq.y + pixelDirSq.z) * pixelDirSqInv.x),
        image->header().spacing().y
          * std::sqrt(1.0f + (pixelDirSq.z + pixelDirSq.x) * pixelDirSqInv.y)
      ),
      image->header().spacing().z * std::sqrt(1.0f + (pixelDirSq.x + pixelDirSq.y) * pixelDirSqInv.z)
    );

    distance = std::min(distance, d);
    ++numImagesUsed;
  }

  if (0 == numImagesUsed)
  {
    return sk_defaultSliceScrollDistance;
  }

  return distance;
}

float sliceScrollDistance(const glm::vec3& worldCameraFrontDir, const Image& image)
{
  // Scroll in image Pixel space along the camera's front direction:
  const glm::mat3& pixel_T_world = image.transformations().pixel_T_worldDef();

  glm::vec3 pixelDir = glm::abs(glm::normalize(pixel_T_world * worldCameraFrontDir));
  pixelDir /= (pixelDir.x + pixelDir.y + pixelDir.z);

  // Scroll distance is proportional to spacing of image along the view direction:
  return std::abs(glm::dot(glm::vec3{image.header().spacing()}, pixelDir));
}

float computeViewOffsetDistance(
  const AppData& appData, const ViewOffsetSetting& offsetSetting, const glm::vec3& worldCameraFront
)
{
  switch (offsetSetting.m_offsetMode)
  {
  case ViewOffsetMode::RelativeToRefImageScrolls:
  {
    if (const Image* refImg = appData.refImage())
    {
      return static_cast<float>(offsetSetting.m_relativeOffsetSteps)
             * data::sliceScrollDistance(worldCameraFront, *refImg);
    }

    return 0.0f; // Invalid reference image, so do not offset
  }
  case ViewOffsetMode::RelativeToImageScrolls:
  {
    const Image* image = (offsetSetting.m_offsetImage)
                           ? appData.image(*(offsetSetting.m_offsetImage))
                           : nullptr;

    if (image)
    {
      return static_cast<float>(offsetSetting.m_relativeOffsetSteps)
             * data::sliceScrollDistance(worldCameraFront, *image);
    }

    return 0.0f; // Invalid image, so do not offset
  }
  case ViewOffsetMode::Absolute:
  {
    return offsetSetting.m_absoluteOffset;
  }
  case ViewOffsetMode::None:
  {
    return 0.0f;
  }
  }

  return 0.0f;
}

glm::vec2 sliceMoveDistance(
  const AppData& data,
  const glm::vec3& worldCameraRightDir,
  const glm::vec3& worldCameraUpDir,
  const ImageSelection& imageSelection,
  const View* view
)
{
  if (0 == data.numImages())
  {
    return {sk_defaultSliceMoveDistance, sk_defaultSliceMoveDistance};
  }

  glm::vec2 distances{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};

  size_t numImagesUsed = 0;

  for (const auto& imageUid : selectImages(data, imageSelection, view))
  {
    const Image* image = data.image(imageUid);
    if (!image)
      continue;

    // Scroll in image Pixel space along the camera's front direction:
    const glm::mat3& pixel_T_world = image->transformations().pixel_T_worldDef();
    const glm::vec3 pixelRightDir = glm::abs(glm::normalize(pixel_T_world * worldCameraRightDir));
    const glm::vec3 pixelUpDir = glm::abs(glm::normalize(pixel_T_world * worldCameraUpDir));

    // Move distance is proportional to spacing of image along the view directions
    float distRight = std::abs(glm::dot(glm::vec3{image->header().spacing()}, pixelRightDir));
    float distUp = std::abs(glm::dot(glm::vec3{image->header().spacing()}, pixelUpDir));
    distances = glm::min(distances, glm::vec2{distRight, distUp});

    ++numImagesUsed;
  }

  if (0 == numImagesUsed)
  {
    return {sk_defaultSliceMoveDistance, sk_defaultSliceMoveDistance};
  }

  return distances;
}

AABB<float> computeWorldAABBoxEnclosingImages(
  const AppData& appData, const ImageSelection& imageSelection
)
{
  static const AABB<float> sk_defaultAABB{{-1, -1, -1}, {1, 1, 1}};

  switch (imageSelection)
  {
  case ImageSelection::VisibleImagesInView:
  case ImageSelection::FixedImageInView:
  case ImageSelection::MovingImageInView:
  case ImageSelection::FixedAndMovingImagesInView:
  {
    // These image selection modes are dependent on a specific view.
    // Since we want an AABB that applies to all views, just return the default AABB:
    return sk_defaultAABB;
  }

  case ImageSelection::ReferenceImage:
  case ImageSelection::ActiveImage:
  case ImageSelection::ReferenceAndActiveImages:
  case ImageSelection::AllLoadedImages:
  {
    break;
  }
  }

  std::vector<glm::vec3> corners;
  bool anyImagesUsed = false;

  for (const auto& imageUid : selectImages(appData, imageSelection, nullptr))
  {
    const auto* img = appData.image(imageUid);
    if (!img)
      continue;

    anyImagesUsed = true;

    const auto worldMinMaxCorners = computeWorldMinMaxCornersOfImage(*img);
    corners.push_back(worldMinMaxCorners.first);
    corners.push_back(worldMinMaxCorners.second);
  }

  if (!anyImagesUsed)
  {
    return sk_defaultAABB;
  }

  return math::computeAABBox<float>(corners);
}

std::optional<uuids::uuid> createLabelColorTableForSegmentation(
  AppData& appData, const uuids::uuid& segUid
)
{
  static constexpr int64_t sk_minNumLabels = static_cast<int64_t>(256);

  Image* seg = appData.seg(segUid);

  if (!seg)
  {
    spdlog::error("Cannot create label color table for invalid segmentation {}", segUid);
    return std::nullopt;
  }

  // What's the largest label value in this segmentation?
  const int64_t maxLabelInSeg = static_cast<int64_t>(seg->settings().componentStatistics().m_maximum
  );
  spdlog::debug("Maximum label value in segmentation {} is {}", segUid, maxLabelInSeg);

  // What's the largest value supported by the segmentation image component type?
  const int64_t maxLabelForComp = static_cast<int64_t>(
    componentRange(seg->header().memoryComponentType()).second
  );

  // Add one to account for label 0:
  const int64_t maxNumLabelsInSeg = maxLabelInSeg + 1;
  const int64_t maxNumLabelsForComp = maxLabelForComp + 1;

  spdlog::debug(
    "Maximum label value supported by the component type ({}) of segmentation {} is {}",
    seg->header().memoryComponentTypeAsString(),
    segUid,
    maxLabelForComp
  );

  // Allocate a table with at least 256 labels, but no more than the upper bound.
  const int64_t numLabels = std::min(
    std::max(maxNumLabelsInSeg, sk_minNumLabels),
    static_cast<int64_t>(ParcellationLabelTable::labelCountUpperBound())
  );

  if (maxNumLabelsInSeg > numLabels)
  {
    spdlog::warn(
      "A color table is being allocated with {} labels, which is fewer than "
      "the number required to represent the maximum label ({}) in segmentation {}",
      numLabels,
      maxNumLabelsInSeg,
      segUid
    );
  }

  if (maxNumLabelsForComp > numLabels)
  {
    spdlog::info(
      "A color table is being allocated with {} labels, which is fewer than "
      "the number of labels ({}) that can be represented by the pixel component type "
      "({}) of segmentation {}",
      numLabels,
      maxNumLabelsForComp,
      seg->header().memoryComponentTypeAsString(),
      segUid
    );
  }

  const size_t newTableIndex = appData.addLabelColorTable(numLabels, maxNumLabelsForComp);
  seg->settings().setLabelTableIndex(newTableIndex);

  spdlog::info(
    "Create new label color table (index {}) with {} labels for segmentation {}",
    newTableIndex,
    numLabels,
    segUid
  );

  return appData.labelTableUid(newTableIndex);
}

std::optional<glm::ivec3> getImageVoxelCoordsAtCrosshairs(const AppData& appData, size_t imageIndex)
{
  const auto imageUid = appData.imageUid(imageIndex);
  const Image* image = imageUid ? appData.image(*imageUid) : nullptr;

  if (!image)
    return std::nullopt;

  const glm::vec4 pixelPos = image->transformations().pixel_T_worldDef()
                             * glm::vec4{appData.state().worldCrosshairs().worldOrigin(), 1};

  const glm::ivec3 roundedPixelPos = glm::ivec3{glm::round(pixelPos / pixelPos.w)};

  if ( glm::any( glm::lessThan( roundedPixelPos, glm::ivec3{ 0, 0, 0 } ) ) ||
         glm::any( glm::greaterThanEqual( roundedPixelPos, glm::ivec3{ image->header().pixelDimensions() } ) ) )
  {
    return std::nullopt;
  }

  return roundedPixelPos;
}

std::optional<glm::vec3> getImageVoxelCoordsContinuousAtCrosshairs(
  const AppData& appData, std::size_t imageIndex
)
{
  const auto imageUid = appData.imageUid(imageIndex);
  const Image* image = imageUid ? appData.image(*imageUid) : nullptr;

  if (!image)
    return std::nullopt;

  const glm::vec4 pixelPos = image->transformations().pixel_T_worldDef()
                             * glm::vec4{appData.state().worldCrosshairs().worldOrigin(), 1};

  const glm::vec3 pixelPosXYZ = pixelPos / pixelPos.w;

  const glm::vec3 HALF_VOXEL{0.5f};

  if ( glm::any( glm::lessThan( pixelPosXYZ, -HALF_VOXEL ) ) ||
        glm::any( glm::greaterThanEqual( pixelPosXYZ, glm::vec3{ image->header().pixelDimensions() } - HALF_VOXEL ) ) )
  {
    return std::nullopt;
  }

  return pixelPosXYZ;
}

std::optional<glm::ivec3> getSegVoxelCoordsAtCrosshairs(
  const AppData& appData, const uuids::uuid& segUid, const uuids::uuid& matchingImgUid
)
{
  const Image* seg = appData.seg(segUid);
  if (!seg)
    return std::nullopt;

  const Image* img = appData.image(matchingImgUid);
  if (!img)
    return std::nullopt;

  const ImageTransformations& segTx = seg->transformations();
  const ImageTransformations& imgTx = img->transformations();

  // Concatenate the segmentation's pixel_T_subject with the image's manual affine tx
  // (subject_T_worldDef):
  const glm::mat4 segPixel_T_worldDef = segTx.pixel_T_subject() * imgTx.subject_T_worldDef();

  const glm::vec4 pixelPos = segPixel_T_worldDef
                             * glm::vec4{appData.state().worldCrosshairs().worldOrigin(), 1};

  const glm::ivec3 roundedPixelPos = glm::ivec3{glm::round(pixelPos / pixelPos.w)};

  if ( glm::any( glm::lessThan( roundedPixelPos, glm::ivec3{ 0, 0, 0 } ) ) ||
         glm::any( glm::greaterThanEqual( roundedPixelPos, glm::ivec3{ seg->header().pixelDimensions() } ) ) )
  {
    return std::nullopt;
  }

  return roundedPixelPos;
}

std::vector<uuids::uuid> findAnnotationsForImage(
  const AppData& appData,
  const uuids::uuid& imageUid,
  const glm::vec4& querySubjectPlaneEquation,
  float planeDistanceThresh
)
{
  // Angle threshold (in degrees) for checking whether two vectors are parallel
  static constexpr float sk_parallelThreshold_degrees = 0.1f;

  std::vector<uuids::uuid> annotUids;

  for (const auto& annotUid : appData.annotationsForImage(imageUid))
  {
    const Annotation* annot = appData.annotation(annotUid);
    if (!annot)
      continue;

    const glm::vec4 testSubjectPlaneEquation = annot->getSubjectPlaneEquation();

    // Compare absolute values of dot product between normal vectors and
    // distances between plane offsets:
    const glm::vec3 n1 = glm::normalize(glm::vec3{testSubjectPlaneEquation});
    const glm::vec3 n2 = glm::normalize(glm::vec3{querySubjectPlaneEquation});

    const float d1 = testSubjectPlaneEquation[3];
    float d2 = querySubjectPlaneEquation[3];

    if ((glm::dot(n1, n2) < 0.0f))
    {
      d2 = -d2;
    }

    const bool normalMatch = camera::areVectorsParallel(n1, n2, sk_parallelThreshold_degrees);
    const bool offsetMatch = (std::abs(d1 - d2) < planeDistanceThresh);

    if (normalMatch && offsetMatch)
    {
      // The plane of this annotation group matches the query plane:
      annotUids.push_back(annotUid);
    }
  }

  return annotUids;
}

glm::vec3 roundPointToNearestImageVoxelCenter(const Image& image, const glm::vec3& worldPos)
{
  const auto& tx = image.transformations();
  const glm::vec4 refPixelPos = tx.pixel_T_worldDef() * glm::vec4{worldPos, 1.0f};
  const glm::vec4 roundedPixelPos{glm::vec3{glm::round(refPixelPos / refPixelPos.w)}, 1.0f};

  glm::vec4 roundedWorldPos = tx.worldDef_T_pixel() * roundedPixelPos;
  return glm::vec3{roundedWorldPos / roundedWorldPos.w};
}

std::string getAnnotationSubjectPlaneName(const Annotation& annotation)
{
  static const std::unordered_map<Directions::Anatomy, std::string> sk_directionToName{
    {Directions::Anatomy::Left, "sagittal"},
    {Directions::Anatomy::Posterior, "coronal"},
    {Directions::Anatomy::Superior, "axial"}
  };

  static const std::string sk_oblique("oblique");

  static constexpr float sk_parallelThreshold_degrees = 0.1f;

  const glm::vec3 subjectPlaneNormal{annotation.getSubjectPlaneEquation()};

  for (const auto& dir : sk_directionToName)
  {
    if (camera::areVectorsParallel(
          Directions::get(dir.first), subjectPlaneNormal, sk_parallelThreshold_degrees
        ))
    {
      return dir.second;
    }
  }

  return sk_oblique;
}

std::optional<uuids::uuid> getSelectedAnnotation(const AppData& appData)
{
  const auto activeImageUid = appData.activeImageUid();
  if (!activeImageUid)
    return std::nullopt;

  return appData.imageToActiveAnnotationUid(*activeImageUid);
}

glm::vec3 snapWorldPointToImageVoxels(
  const AppData& appData, const glm::vec3& worldPos, const std::optional<CrosshairsSnapping>& force
)
{
  const CrosshairsSnapping snapping = force ? *force : appData.renderData().m_snapCrosshairs;

  switch (snapping)
  {
  case CrosshairsSnapping::ReferenceImage:
  {
    if (const Image* refImg = appData.refImage())
    {
      return glm::vec4{data::roundPointToNearestImageVoxelCenter(*refImg, worldPos), 1.0f};
    }
    break;
  }

  case CrosshairsSnapping::ActiveImage:
  {
    if (const Image* activeImg = appData.activeImage())
    {
      return glm::vec4{data::roundPointToNearestImageVoxelCenter(*activeImg, worldPos), 1.0f};
    }
    break;
  }

  default:
  case CrosshairsSnapping::Disabled:
  {
    return worldPos;
  }
  }

  return worldPos;
}

std::size_t computeNumImageSlicesAlongWorldDirection(const Image& image, const glm::vec3& worldDir)
{
  const glm::mat3 pixel_T_world = glm::mat3{image.transformations().pixel_T_worldDef()};
  const glm::vec3 imgSpacing = image.header().spacing();
  const glm::vec3 worldBboxSize = image.header().subjectBBoxSize();

  glm::vec3 pixelDir = glm::abs(glm::normalize(glm::vec3{pixel_T_world * worldDir}));
  pixelDir /= (pixelDir.x + pixelDir.y + pixelDir.z);

  const float spacing = glm::dot(imgSpacing, pixelDir);
  const std::size_t numSlices = static_cast<std::size_t>(std::ceil(worldBboxSize.z / spacing));

  return numSlices;
}

} // namespace data
