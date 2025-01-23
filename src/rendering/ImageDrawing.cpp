#include "rendering/ImageDrawing.h"
#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/gl/GLShaderProgram.h"

#include "common/DataHelper.h"
#include "image/Image.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "windowing/View.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <glad/glad.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace
{

/**
 * @brief Compute Texture-space direction to sample direction along a camera view axis
 * @param[in] pixel_T_clip Clip-to-Pixel transformation matrix for the view camera and image
 * @param[in] invPixelDims
 * @param[in] axis View axis
 * @return Sampling direction in Texture space
 */
glm::vec3 computeTexSamplingDir(
  const glm::mat4& pixel_T_clip, const glm::vec3& invPixelDims, const Directions::View& axis
)
{
  static const glm::vec4 clipOrigin{0.0f, 0.0f, -1.0f, 1.0};
  const glm::vec4 clipPos = clipOrigin + glm::vec4{Directions::get(axis), 0.0f};

  const glm::vec4 pixelPos = pixel_T_clip * clipPos;
  const glm::vec4 pixelOrigin = pixel_T_clip * clipOrigin;

  const glm::vec3 pixelDir = glm::normalize(pixelPos / pixelPos.w - pixelOrigin / pixelOrigin.w);

  return glm::dot(glm::abs(pixelDir), invPixelDims) * pixelDir;
}

glm::vec3 computeTextureSamplingDirectionForViewPixelOffset(
  const glm::mat4& texture_T_viewClip,
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec2& winPixelDir
)
{
  static const glm::vec2 winPixelOrigin(0.0f, 0.0f);

  const glm::vec4
    winNdcOrigin{camera::windowNdc_T_window(windowViewport, winPixelOrigin), -1.0f, 1.0f};
  const glm::vec4 winNdcPos{camera::windowNdc_T_window(windowViewport, winPixelDir), -1.0f, 1.0f};

  glm::vec4 viewNdcOrigin = viewClip_T_windowClip * winNdcOrigin;
  viewNdcOrigin /= viewNdcOrigin.w;
  glm::vec4 viewNdcPos = viewClip_T_windowClip * winNdcPos;
  viewNdcPos /= viewNdcPos.w;

  glm::vec4 texOrigin = texture_T_viewClip * viewNdcOrigin;
  texOrigin /= texOrigin.w;
  glm::vec4 texPos = texture_T_viewClip * viewNdcPos;
  texPos /= texPos.w;

  return glm::vec3{texPos - texOrigin};
}

glm::vec3 computeTextureSamplingDirectionForImageVoxelOffset(
  const glm::mat4& voxel_T_viewClip,
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec3& invPixelDimensions,
  const glm::vec2& winPixelDir
)
{
  static const glm::vec2 winPixelOrigin(0.0f, 0.0f);

  const glm::vec4
    winNdcOrigin{camera::windowNdc_T_window(windowViewport, winPixelOrigin), -1.0f, 1.0f};
  const glm::vec4 winNdcPos{camera::windowNdc_T_window(windowViewport, winPixelDir), -1.0f, 1.0f};

  glm::vec4 viewNdcOrigin = viewClip_T_windowClip * winNdcOrigin;
  viewNdcOrigin /= viewNdcOrigin.w;
  glm::vec4 viewNdcPos = viewClip_T_windowClip * winNdcPos;
  viewNdcPos /= viewNdcPos.w;

  glm::vec4 voxelOrigin = voxel_T_viewClip * viewNdcOrigin;
  voxelOrigin /= voxelOrigin.w;
  glm::vec4 voxelPos = voxel_T_viewClip * viewNdcPos;
  voxelPos /= voxelPos.w;

  const glm::vec3 voxelDir = glm::normalize(voxelPos - voxelOrigin);
  const glm::vec3 texDir = glm::dot(glm::abs(voxelDir), invPixelDimensions) * voxelDir;

  return texDir;
}

/**
 * @brief Compute half the number of samples and the sample distance (in centimeters) for MIPs
 * @param camera
 * @param image
 * @param doMaxExtentMip
 * @return Pair containing 1) half the number of image samples to compute per slab;
 * 2) the sampling distance in centimeters
 */
std::pair<int, float> computeMipSamplingParams(
  const camera::Camera& camera, const Image& image, float mipSlabThickness_mm, bool doMaxExtentMip
)
{
  const float mmPerSample
    = data::sliceScrollDistance(camera::worldDirection(camera, Directions::View::Front), image);

  int halfNumMipSamples = 0;

  if (!doMaxExtentMip)
  {
    halfNumMipSamples = static_cast<int>(std::floor(0.5f * mipSlabThickness_mm / mmPerSample));
  }
  else
  {
    // To achieve maximum extent, use the number of samples along the image diagonal.
    // That way, the MIP will hit all voxels.
    halfNumMipSamples = static_cast<int>(
      std::ceil(glm::length(glm::vec3{image.header().pixelDimensions()}))
    );
  }

  // Convert sampling distance from mm to cm:
  return std::make_pair(halfNumMipSamples, mmPerSample / 10.0f);
}

} // namespace

void drawImageQuad(
  GLShaderProgram& program,
  const camera::ViewRenderMode& renderMode,
  RenderData::Quad& quad,
  const View& view,
  const Viewport& windowViewport,
  const glm::vec3& worldCrosshairs,
  float flashlightRadius,
  bool flashlightOverlays,
  float mipSlabThickness_mm,
  bool doMaxExtentMip,
  float xrayIntensityWindow,
  float xrayIntensityLevel,
  const std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >& I,
  const std::function<const Image*(const std::optional<uuids::uuid>& imageUid)> getImage,
  bool showEdges,
  const SegmentationOutlineStyle& segOutlineStyle,
  float segInteriorOpacity,
  const SegmentationInterpolation& segInterpolation,
  float segInterpCutoff
)
{
  static const glm::vec4 sk_clipO{0.0f, 0.0f, -1.0f, 1.0};
  static const glm::vec4 sk_clipX{1.0f, 0.0f, -1.0f, 1.0};
  static const glm::vec4 sk_clipY{0.0f, 1.0f, -1.0f, 1.0};

  if (I.empty())
  {
    spdlog::error("No images provided when rendering plane");
    return;
  }

  const Image* image0 = getImage(I[0].first);

  if (!image0)
  {
    spdlog::error("Null image when rendering textured quad");
    return;
  }

  const glm::mat4 world_T_viewClip = camera::world_T_clip(view.camera());

  // Direction to sample direction along the camera view's Z axis for image 0:
  glm::vec3 texSamplingDirZ(0.0f);

  // Half the number of samples for MIPs (for image 0):
  int halfNumMipSamples = 0;

  // Distance (mm) per sample for computing MIPs (for image 0):
  float mipSamplingDistance_cm = 0.0f;

  // Only compute these if doing a MIP:
  if (camera::IntensityProjectionMode::None != view.intensityProjectionMode())
  {
    const glm::mat4 pixel_T_clip = image0->transformations().pixel_T_worldDef() * world_T_viewClip;

    texSamplingDirZ = computeTexSamplingDir(
      pixel_T_clip, image0->transformations().invPixelDimensions(), Directions::View::Back
    );

    const auto s
      = computeMipSamplingParams(view.camera(), *image0, mipSlabThickness_mm, doMaxExtentMip);
    halfNumMipSamples = s.first;
    mipSamplingDistance_cm = s.second;
  }

  std::vector<glm::vec3> voxelSamplingDirs{glm::vec3{0.0f}, glm::vec3{0.0f}};
  std::vector<glm::vec3> texSamplingDirsForSegOutline{glm::vec3{0.0f}, glm::vec3{0.0f}};
  std::vector<glm::vec3> texSamplingDirsForSmoothSeg{glm::vec3{0.0f}, glm::vec3{0.0f}};
  std::vector<glm::vec3> texSamplingDirsForEdges{glm::vec3{0.0f}, glm::vec3{0.0f}};

  {
    const auto posInfo = math::computeAnatomicalLabelsForView(
      view.camera().camera_T_world(), image0->transformations().worldDef_T_subject()
    );

    const glm::mat4 voxel_T_viewClip = image0->transformations().pixel_T_worldDef()
                                       * world_T_viewClip;

    for (int i = 0; i < 2; ++i)
    {
      voxelSamplingDirs[i] = computeTextureSamplingDirectionForImageVoxelOffset(
        voxel_T_viewClip,
        windowViewport,
        view.viewClip_T_windowClip(),
        image0->transformations().invPixelDimensions(),
        posInfo[i].viewClipDir
      );

      if (SegmentationOutlineStyle::ImageVoxel == segOutlineStyle)
      {
        texSamplingDirsForSegOutline = voxelSamplingDirs;
      }

      // For edges and smooth segmentation sampling,
      // use sampling directions based on image voxels:
      texSamplingDirsForEdges = voxelSamplingDirs;
      texSamplingDirsForSmoothSeg = voxelSamplingDirs;
    }
  }

  if (SegmentationOutlineStyle::ViewPixel == segOutlineStyle)
  {
    const auto posInfo = math::computeAnatomicalLabelsForView(
      view.camera().camera_T_world(), image0->transformations().worldDef_T_subject()
    );

    const glm::mat4 texture_T_viewClip = image0->transformations().texture_T_worldDef()
                                         * world_T_viewClip;

    for (int i = 0; i < 2; ++i)
    {
      texSamplingDirsForSegOutline[i] = computeTextureSamplingDirectionForViewPixelOffset(
        texture_T_viewClip, windowViewport, view.viewClip_T_windowClip(), posInfo[i].viewClipDir
      );
    }
  }

  // Set the view transformation uniforms that are common to all image plane rendering programs:
  program.setUniform("u_view_T_clip", view.windowClip_T_viewClip());
  program.setUniform("u_world_T_clip", world_T_viewClip);
  program.setUniform("u_clipDepth", view.clipPlaneDepth());

  // Segmentation outlines:
  program.setUniform("u_texSamplingDirsForSegOutline", texSamplingDirsForSegOutline);
  program.setUniform(
    "u_segInteriorOpacity",
    (SegmentationOutlineStyle::Disabled == segOutlineStyle) ? 1.0f : segInteriorOpacity
  );

  if (camera::ViewRenderMode::Image == renderMode || camera::ViewRenderMode::Checkerboard == renderMode || camera::ViewRenderMode::Quadrants == renderMode || camera::ViewRenderMode::Flashlight == renderMode)
  {
    program.setUniform("u_aspectRatio", view.camera().aspectRatio());
    program.setUniform("u_flashlightRadius", flashlightRadius);
    program.setUniform("u_flashlightOverlays", flashlightOverlays);

    const glm::vec4 clipXhairs = camera::clip_T_world(view.camera())
                                 * glm::vec4{worldCrosshairs, 1.0f};

    program.setUniform("u_clipCrosshairs", glm::vec2{clipXhairs / clipXhairs.w});

    if (showEdges)
    {
      program.setUniform("u_texSamplingDirsForEdges", texSamplingDirsForEdges);
    }
    else
    {
      if (SegmentationInterpolation::Linear == segInterpolation)
      {
        // Segmentation interpolation:
        // For now, only used in Image.fs. Add this to all shaders.
        program.setUniform("u_texSamplingDirsForSmoothSeg", texSamplingDirsForSmoothSeg);
        program.setUniform("u_segInterpCutoff", segInterpCutoff);
      }

      // Only render with intensity projection when edges are not visible:
      program.setUniform("u_halfNumMipSamples", halfNumMipSamples);
      program.setUniform("u_texSamplingDirZ", texSamplingDirZ);

      if (camera::IntensityProjectionMode::Xray != view.intensityProjectionMode())
      {
        program.setUniform("u_mipMode", underlyingType_asInt32(view.intensityProjectionMode()));
      }
      else
      {
        // Convert window/level to slope/intercept:
        const float window = std::max(xrayIntensityWindow, 1.0e-3f);

        const glm::vec2 slopeIntercept{1.0f / window, 0.5f - xrayIntensityLevel / window};

        program.setUniform("slopeInterceptWindowLevel", slopeIntercept);
        program.setUniform("mipSamplingDistance_cm", mipSamplingDistance_cm);
      }
    }
  }
  else if (camera::ViewRenderMode::Difference == renderMode)
  {
    program.setUniform("u_mipMode", underlyingType_asInt32(view.intensityProjectionMode()));
    program.setUniform("u_halfNumMipSamples", halfNumMipSamples);
    program.setUniform("u_texSamplingDirZ", texSamplingDirZ);
  }
  else if (camera::ViewRenderMode::CrossCorrelation == renderMode)
  {
    if (2 != I.size())
    {
      spdlog::error("Not enough images provided when rendering plane with cross-correlation metric");
      return;
    }

    const Image* img0 = getImage(I[0].first);
    const Image* img1 = getImage(I[1].first);

    if (!img0 || !img1)
    {
      spdlog::error("Null image when rendering plane with edges");
      return;
    }

    const glm::mat4 img0Pixel_T_clip = img0->transformations().pixel_T_worldDef()
                                       * world_T_viewClip;

    const glm::vec4 ppO = img0Pixel_T_clip * sk_clipO;
    const glm::vec4 ppX = img0Pixel_T_clip * sk_clipX;
    const glm::vec4 ppY = img0Pixel_T_clip * sk_clipY;

    const glm::vec3 pixelDirX = glm::normalize(ppX / ppX.w - ppO / ppO.w);
    const glm::vec3 pixelDirY = glm::normalize(ppY / ppY.w - ppO / ppO.w);

    const glm::vec3 img0_invDims = img0->transformations().invPixelDimensions();

    const glm::vec3 tex0SamplingDirX = glm::dot(glm::abs(pixelDirX), img0_invDims) * pixelDirX;
    const glm::vec3 tex0SamplingDirY = glm::dot(glm::abs(pixelDirY), img0_invDims) * pixelDirY;

    program.setUniform("u_tex0SamplingDirX", tex0SamplingDirX);
    program.setUniform("u_tex0SamplingDirY", tex0SamplingDirY);
  }

  quad.m_vao.bind();
  {
    quad.m_vao.drawElements(quad.m_vaoParams);
  }
  quad.m_vao.release();
}

void drawRaycastQuad(
  GLShaderProgram& program,
  RenderData::Quad& quad,
  const View& view,
  const std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >& I,
  const std::function<const Image*(const std::optional<uuids::uuid>& imageUid)> getImage
)
{
  if (I.empty())
  {
    spdlog::error("No images provided when raycasting");
    return;
  }

  const Image* image0 = getImage(I[0].first);

  if (!image0)
  {
    spdlog::error("Null image when raycasting");
    return;
  }

  // Set the view transformation uniforms that are common to all raycast rendering programs:
  program.setUniform("u_view_T_clip", view.windowClip_T_viewClip());
  program.setUniform("u_world_T_clip", camera::world_T_clip(view.camera()));
  program.setUniform("clip_T_world", camera::clip_T_world(view.camera()));

  /// @todo This must match the camera eye position
  program.setUniform("u_clipDepth", view.clipPlaneDepth());

  /// @todo the near distance must equal 0.5 voxel spacing and far distance must be beyond volume...
  /// look at how ED does it
  //    glm::perspective( view.fov, windowAspect, imageData.nearDistance, imageData.farDistance );

  quad.m_vao.bind();
  {
    quad.m_vao.drawElements(quad.m_vaoParams);
  }
  quad.m_vao.release();
}
