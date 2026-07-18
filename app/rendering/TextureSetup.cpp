#include "rendering/TextureSetup.h"
#include "rendering/helpers/TextureSetupHelpers.h"
#include "logic/app/Data.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

namespace
{

namespace texture_setup = rendering::texture_setup;
using TextureUploadLayout = texture_setup::TextureUploadLayout;

texture_setup::TextureLimits queryTextureLimits()
{
  GLint maxTextureSize = 0;
  GLint max3DTextureSize = 0;
  GLint maxArrayTextureLayers = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DTextureSize);
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);

  texture_setup::TextureLimits limits;
  limits.maxTextureSize = maxTextureSize;
  limits.max3DTextureSize = max3DTextureSize;
  limits.maxArrayTextureLayers = maxArrayTextureLayers;
  return limits;
}

bool shouldLogPlanarImageUpload(const uuids::uuid& imageUid)
{
  static std::unordered_set<uuids::uuid> loggedImageUids;
  return loggedImageUids.insert(imageUid).second;
}

bool shouldLogPlanarSegUpload(const uuids::uuid& segUid)
{
  static std::unordered_set<uuids::uuid> loggedSegUids;
  return loggedSegUids.insert(segUid).second;
}

bool imageUsesPlanar2DTexture(const AppData& appData, const uuids::uuid& imageUid)
{
  const auto layoutIt = appData.renderData().m_imageTextureLayouts.find(imageUid);
  return layoutIt != std::end(appData.renderData().m_imageTextureLayouts) &&
         RenderData::TextureDimension::Texture2D == layoutIt->second.dimension;
}

TextureCreationFailure makeTextureSizeFailure(
  TextureCreationFailure::Resource resource,
  const uuids::uuid& uid,
  const std::string& displayName,
  const glm::uvec3& dimensions,
  int max3DTextureSize,
  std::string reason)
{
  return TextureCreationFailure{
    .resource = resource,
    .uid = uid,
    .displayName = displayName,
    .dimensions = {dimensions.x, dimensions.y, dimensions.z},
    .max3DTextureSize = max3DTextureSize,
    .reason = std::move(reason)};
}

TextureCreationFailure makeTextureUploadFailure(
  TextureCreationFailure::Resource resource,
  const uuids::uuid& uid,
  const std::string& displayName,
  const glm::uvec3& dimensions,
  int max3DTextureSize,
  std::string reason)
{
  return TextureCreationFailure{
    .resource = resource,
    .uid = uid,
    .displayName = displayName,
    .dimensions = {dimensions.x, dimensions.y, dimensions.z},
    .max3DTextureSize = max3DTextureSize,
    .reason = std::move(reason)};
}

std::string dimensionsLabel(const std::array<uint32_t, 3>& dimensions)
{
  std::ostringstream ss;
  ss << dimensions[0] << " x " << dimensions[1] << " x " << dimensions[2];
  return ss.str();
}

std::string resourceLabel(TextureCreationFailure::Resource resource)
{
  switch (resource) {
    case TextureCreationFailure::Resource::Image:
      return "image";
    case TextureCreationFailure::Resource::Segmentation:
      return "segmentation";
  }
  return "resource";
}

std::string unloadButtonLabel(TextureCreationFailure::Resource resource)
{
  switch (resource) {
    case TextureCreationFailure::Resource::Image:
      return "Unload Image";
    case TextureCreationFailure::Resource::Segmentation:
      return "Unload Segmentation";
  }
  return "Unload";
}

bool userWantsToUnloadTextureFailure(const TextureCreationFailure& failure)
{
  const std::string resource = resourceLabel(failure.resource);
  const std::string message =
    "The " + resource + " '" + failure.displayName + "' was loaded, but Entropy cannot create a 3D texture for it.";
  const std::string informativeText =
    "Image dimensions are " + dimensionsLabel(failure.dimensions) + ". " + failure.reason +
    "\n\n"
    "You can keep the " +
    resource +
    " loaded for metadata/project inspection, but it will not be renderable until Entropy supports a "
    "true 2D texture rendering path or a downsampled texture fallback.";

  const auto result = native_dialog::showMessageDialog(
    {"Texture Size Exceeds OpenGL Limit",
     message,
     informativeText,
     "Keep Loaded",
     unloadButtonLabel(failure.resource),
     ""});
  return result && native_dialog::MessageDialogResult::SecondButton == *result;
}

void handleTextureCreationFailures(AppData& appData, const std::vector<TextureCreationFailure>& failures)
{
  for (const TextureCreationFailure& failure : failures) {
    if (!userWantsToUnloadTextureFailure(failure)) {
      continue;
    }

    switch (failure.resource) {
      case TextureCreationFailure::Resource::Image:
        if (appData.removeImage(failure.uid)) {
          spdlog::info("Unloaded non-renderable image {}", failure.uid);
        }
        else if (appData.def(failure.uid) && appData.removeDef(failure.uid)) {
          spdlog::info("Unloaded non-renderable warp field image {}", failure.uid);
        }
        else if (appData.refImageUid() && *appData.refImageUid() == failure.uid) {
          spdlog::warn("Unloading non-renderable reference image {}; clearing loaded project data", failure.uid);
          appData.renderData().m_imageTextures.clear();
          appData.renderData().m_imageTextureLayouts.clear();
          appData.renderData().m_distanceMapTextures.clear();
          appData.renderData().m_segTextures.clear();
          appData.renderData().m_segTextureLayouts.clear();
          appData.renderData().m_uniforms.clear();
          appData.clearProjectData();
        }
        else {
          spdlog::warn("Unable to unload non-renderable image {}", failure.uid);
        }
        break;
      case TextureCreationFailure::Resource::Segmentation:
        if (appData.removeSeg(failure.uid)) {
          spdlog::info("Unloaded non-renderable segmentation {}", failure.uid);
        }
        else {
          spdlog::warn("Unable to unload non-renderable segmentation {}", failure.uid);
        }
        break;
    }
  }
}

texture_setup::TextureLimits logTextureLimitsOnce()
{
  static bool logged = false;
  const texture_setup::TextureLimits limits = queryTextureLimits();
  if (logged) {
    return limits;
  }
  logged = true;

  spdlog::debug(
    "OpenGL texture limits: GL_MAX_TEXTURE_SIZE (1D/2D width/height) = {}, "
    "GL_MAX_3D_TEXTURE_SIZE (3D width/height/depth) = {}, "
    "GL_MAX_ARRAY_TEXTURE_LAYERS = {}",
    limits.maxTextureSize,
    limits.max3DTextureSize,
    limits.maxArrayTextureLayers);
  return limits;
}

template<typename T>
std::vector<T> copyComponentFrameValues(const Image& image, uint32_t component, uint32_t timePoint)
{
  std::vector<T> values;
  values.reserve(image.header().numPixels());

  for (std::size_t index = 0; index < image.header().numPixels(); ++index) {
    const std::optional<T> value = image.value<T>(component, index, timePoint);
    if (!value) {
      return {};
    }
    values.push_back(*value);
  }

  return values;
}

std::size_t flatImageIndex(const glm::uvec3& coord, const glm::uvec3& size)
{
  return static_cast<std::size_t>(coord.x) +
         static_cast<std::size_t>(size.x) *
           (static_cast<std::size_t>(coord.y) + static_cast<std::size_t>(size.y) * static_cast<std::size_t>(coord.z));
}

template<typename T>
std::vector<T>
copyPlanarComponentFrameValues(const Image& image, uint32_t component, uint32_t timePoint, const glm::ivec2& axes)
{
  const glm::uvec3 size = image.header().pixelDimensions();
  std::vector<T> values;
  values.reserve(static_cast<std::size_t>(size[axes.x]) * static_cast<std::size_t>(size[axes.y]));

  glm::uvec3 coord{0u};
  for (uint32_t y = 0; y < size[axes.y]; ++y) {
    coord[axes.y] = y;
    for (uint32_t x = 0; x < size[axes.x]; ++x) {
      coord[axes.x] = x;
      const std::optional<T> value = image.value<T>(component, flatImageIndex(coord, size), timePoint);
      if (!value) {
        return {};
      }
      values.push_back(*value);
    }
  }

  return values;
}

template<typename T>
std::vector<T> copyPlanarComponentFrameValues(
  const Image& image,
  uint32_t component,
  uint32_t timePoint,
  const RenderData::PlanarTextureLayout& layout)
{
  return RenderData::TextureDimension::Texture2D == layout.dimension
           ? copyPlanarComponentFrameValues<T>(image, component, timePoint, layout.axes)
           : copyComponentFrameValues<T>(image, component, timePoint);
}

template<typename T>
bool appendScalarComponentTexture(
  std::vector<GLTexture>& componentTextures,
  const Image& image,
  uint32_t component,
  const T* data,
  const TextureUploadLayout& uploadLayout,
  const GLTexture::PixelStoreSettings& pixelPackSettings,
  const GLTexture::PixelStoreSettings& pixelUnpackSettings,
  tex::WrapMode wrapMode)
{
  if (!data) {
    return false;
  }

  tex::MinificationFilter minFilter = tex::MinificationFilter::Linear;
  tex::MagnificationFilter maxFilter = tex::MagnificationFilter::Linear;
  switch (image.settings().interpolationMode(component)) {
    case InterpolationMode::NearestNeighbor:
      minFilter = tex::MinificationFilter::Nearest;
      maxFilter = tex::MagnificationFilter::Nearest;
      break;
    case InterpolationMode::Linear:
    case InterpolationMode::CubicBsplineConvolution:
      minFilter = tex::MinificationFilter::Linear;
      maxFilter = tex::MagnificationFilter::Linear;
      break;
  }

  static constexpr GLint k_mipmapLevel = 0;
  GLTexture& texture = componentTextures.emplace_back(
    RenderData::TextureDimension::Texture2D == uploadLayout.layout.dimension ? tex::Target::Texture2D
                                                                             : tex::Target::Texture3D,
    GLTexture::MultisampleSettings(),
    pixelPackSettings,
    pixelUnpackSettings);

  texture.generate();
  texture.setMinificationFilter(minFilter);
  texture.setMagnificationFilter(maxFilter);
  texture.setWrapMode(wrapMode);
  texture.setAutoGenerateMipmaps(false);
  texture.setSize(uploadLayout.uploadSize);
  texture.setData(
    k_mipmapLevel,
    GLTexture::getSizedInternalNormalizedRedFormat(image.header().memoryComponentType()),
    GLTexture::getBufferPixelNormalizedRedFormat(image.header().memoryComponentType()),
    GLTexture::getBufferPixelDataType(image.header().memoryComponentType()),
    data);
  return true;
}

template<typename T>
bool appendDeinterleavedComponentTexture(
  std::vector<GLTexture>& componentTextures,
  const Image& image,
  uint32_t component,
  uint32_t timePoint,
  const TextureUploadLayout& uploadLayout,
  const GLTexture::PixelStoreSettings& pixelPackSettings,
  const GLTexture::PixelStoreSettings& pixelUnpackSettings,
  tex::WrapMode wrapMode)
{
  const std::vector<T> values = copyPlanarComponentFrameValues<T>(image, component, timePoint, uploadLayout.layout);
  return values.size() == image.header().numPixels() && appendScalarComponentTexture(
                                                          componentTextures,
                                                          image,
                                                          component,
                                                          values.data(),
                                                          uploadLayout,
                                                          pixelPackSettings,
                                                          pixelUnpackSettings,
                                                          wrapMode);
}

bool appendDeinterleavedComponentTexture(
  std::vector<GLTexture>& componentTextures,
  const Image& image,
  uint32_t component,
  uint32_t timePoint,
  const TextureUploadLayout& uploadLayout,
  const GLTexture::PixelStoreSettings& pixelPackSettings,
  const GLTexture::PixelStoreSettings& pixelUnpackSettings,
  tex::WrapMode wrapMode)
{
  switch (image.header().memoryComponentType()) {
    case ComponentType::Int8:
      return appendDeinterleavedComponentTexture<int8_t>(
        componentTextures,
        image,
        component,
        timePoint,
        uploadLayout,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::UInt8:
      return appendDeinterleavedComponentTexture<uint8_t>(
        componentTextures,
        image,
        component,
        timePoint,
        uploadLayout,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::Int16:
      return appendDeinterleavedComponentTexture<int16_t>(
        componentTextures,
        image,
        component,
        timePoint,
        uploadLayout,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::UInt16:
      return appendDeinterleavedComponentTexture<uint16_t>(
        componentTextures,
        image,
        component,
        timePoint,
        uploadLayout,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::Int32:
      return appendDeinterleavedComponentTexture<int32_t>(
        componentTextures,
        image,
        component,
        timePoint,
        uploadLayout,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::UInt32:
      return appendDeinterleavedComponentTexture<uint32_t>(
        componentTextures,
        image,
        component,
        timePoint,
        uploadLayout,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::Float32:
      return appendDeinterleavedComponentTexture<float>(
        componentTextures,
        image,
        component,
        timePoint,
        uploadLayout,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    default:
      return false;
  }
}

template<typename T>
std::vector<std::byte> copyComponentFrameValuesBytes(
  const Image& image,
  uint32_t component,
  uint32_t timePoint,
  const RenderData::PlanarTextureLayout& layout)
{
  const std::vector<T> values = copyPlanarComponentFrameValues<T>(image, component, timePoint, layout);
  return {
    reinterpret_cast<const std::byte*>(values.data()),
    reinterpret_cast<const std::byte*>(values.data() + values.size())};
}

std::vector<std::byte> copyComponentFrameValuesBytes(
  const Image& image,
  uint32_t component,
  uint32_t timePoint,
  const RenderData::PlanarTextureLayout& layout)
{
  switch (image.header().memoryComponentType()) {
    case ComponentType::Int8:
      return copyComponentFrameValuesBytes<int8_t>(image, component, timePoint, layout);
    case ComponentType::UInt8:
      return copyComponentFrameValuesBytes<uint8_t>(image, component, timePoint, layout);
    case ComponentType::Int16:
      return copyComponentFrameValuesBytes<int16_t>(image, component, timePoint, layout);
    case ComponentType::UInt16:
      return copyComponentFrameValuesBytes<uint16_t>(image, component, timePoint, layout);
    case ComponentType::Int32:
      return copyComponentFrameValuesBytes<int32_t>(image, component, timePoint, layout);
    case ComponentType::UInt32:
      return copyComponentFrameValuesBytes<uint32_t>(image, component, timePoint, layout);
    case ComponentType::Float32:
      return copyComponentFrameValuesBytes<float>(image, component, timePoint, layout);
    default:
      return {};
  }
}

bool uploadActiveTimePointToExistingTexture(
  GLTexture& texture,
  const Image& image,
  uint32_t component,
  uint32_t timePoint,
  const RenderData::PlanarTextureLayout& layout)
{
  static constexpr GLint k_mipmapLevel = 0;
  const bool canUseNativeFrameBuffer = RenderData::TextureDimension::Texture3D == layout.dimension &&
                                       Image::MultiComponentBufferType::SeparateImages == image.bufferType();

  const void* uploadData = canUseNativeFrameBuffer ? image.bufferAsVoid(component, timePoint) : nullptr;
  std::vector<std::byte> copiedValues;
  if (!uploadData) {
    copiedValues = copyComponentFrameValuesBytes(image, component, timePoint, layout);
    const std::size_t expectedBytes =
      static_cast<std::size_t>(texture.size().x) * static_cast<std::size_t>(texture.size().y) *
      static_cast<std::size_t>(texture.size().z) * image.header().memoryComponentSizeInBytes();
    if (copiedValues.size() != expectedBytes) {
      return false;
    }
    uploadData = copiedValues.data();
  }

  if (!uploadData) {
    return false;
  }

  texture.setSubData(
    k_mipmapLevel,
    glm::uvec3{0u},
    texture.size(),
    GLTexture::getBufferPixelNormalizedRedFormat(image.header().memoryComponentType()),
    GLTexture::getBufferPixelDataType(image.header().memoryComponentType()),
    uploadData);
  return true;
}

template<typename T>
std::vector<T> copyPlanarSegValues(const Image& seg, const RenderData::PlanarTextureLayout& layout)
{
  return copyPlanarComponentFrameValues<T>(seg, 0u, 0u, layout);
}

std::vector<std::byte> copyPlanarSegValuesBytes(const Image& seg, const RenderData::PlanarTextureLayout& layout)
{
  switch (seg.header().memoryComponentType()) {
    case ComponentType::UInt8: {
      const auto values = copyPlanarSegValues<uint8_t>(seg, layout);
      return {
        reinterpret_cast<const std::byte*>(values.data()),
        reinterpret_cast<const std::byte*>(values.data() + values.size())};
    }
    case ComponentType::UInt16: {
      const auto values = copyPlanarSegValues<uint16_t>(seg, layout);
      return {
        reinterpret_cast<const std::byte*>(values.data()),
        reinterpret_cast<const std::byte*>(values.data() + values.size())};
    }
    case ComponentType::UInt32: {
      const auto values = copyPlanarSegValues<uint32_t>(seg, layout);
      return {
        reinterpret_cast<const std::byte*>(values.data()),
        reinterpret_cast<const std::byte*>(values.data() + values.size())};
    }
    case ComponentType::Int8: {
      const auto values = copyPlanarSegValues<int8_t>(seg, layout);
      return {
        reinterpret_cast<const std::byte*>(values.data()),
        reinterpret_cast<const std::byte*>(values.data() + values.size())};
    }
    case ComponentType::Int16: {
      const auto values = copyPlanarSegValues<int16_t>(seg, layout);
      return {
        reinterpret_cast<const std::byte*>(values.data()),
        reinterpret_cast<const std::byte*>(values.data() + values.size())};
    }
    case ComponentType::Int32: {
      const auto values = copyPlanarSegValues<int32_t>(seg, layout);
      return {
        reinterpret_cast<const std::byte*>(values.data()),
        reinterpret_cast<const std::byte*>(values.data() + values.size())};
    }
    default:
      return {};
  }
}

} // namespace

TextureCreationResult createImageTexturesWithReport(AppData& appData, const uuid_range_t& imageUids)
{
  static constexpr GLint sk_mipmapLevel = 0; // Load image data into first mipmap level
  static constexpr GLint sk_alignment = 1;   // Pixel pack/unpack alignment is 1 byte

  // Clamping to edge is needed for raycasting, so that an isosurface is not rendered
  // on the volume faces
  static const tex::WrapMode sk_wrapModeClampToEdge = tex::WrapMode::ClampToEdge;
  //    static const tex::WrapMode sk_wrapModeClampToBorder = tex::WrapMode::ClampToBorder;
  //    static const glm::vec4 sk_border{ 0.0f, 0.0f, 0.0f, 0.0f }; // Black border

  TextureCreationResult result;

  spdlog::debug("Begin creating 3D image textures");
  const texture_setup::TextureLimits textureLimits = logTextureLimitsOnce();

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = sk_alignment;
  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  for (const auto& imageUid : imageUids) {
    spdlog::debug("Begin creating texture(s) for components of image {}", imageUid);

    const auto* image = appData.image(imageUid);
    if (!image) {
      image = appData.def(imageUid);
    }
    if (!image) {
      spdlog::warn("Image {} is invalid", imageUid);
      continue;
    }

    if (!image->hasPixelData()) {
      spdlog::debug("Skipping texture creation for header-only image {}", imageUid);
      continue;
    }

    const ComponentType compType = image->header().memoryComponentType();
    const uint32_t numComp = image->header().numComponentsPerPixel();
    const uint32_t activeTimePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
    const glm::uvec3 textureSize = image->header().pixelDimensions();
    const std::optional<texture_setup::TextureUploadLayout> uploadLayout =
      texture_setup::textureUploadLayoutForImage(textureSize, textureLimits);
    if (!uploadLayout) {
      spdlog::error(
        "Image {} ('{}') has dimensions {} and cannot be uploaded as an OpenGL texture. {}",
        imageUid,
        image->settings().displayName(),
        glm::to_string(textureSize),
        texture_setup::textureLimitReason(textureSize, textureLimits));
      result.failures.push_back(makeTextureSizeFailure(
        TextureCreationFailure::Resource::Image,
        imageUid,
        image->settings().displayName(),
        textureSize,
        textureLimits.max3DTextureSize,
        texture_setup::textureLimitReason(textureSize, textureLimits)));
      continue;
    }
    if (
      RenderData::TextureDimension::Texture2D == uploadLayout->layout.dimension && shouldLogPlanarImageUpload(imageUid))
    {
      spdlog::info(
        "Image {} ('{}') exceeds GL_MAX_3D_TEXTURE_SIZE but is planar; uploading as GL_TEXTURE_2D with axes ({}, {}) "
        "and size {}",
        imageUid,
        image->settings().displayName(),
        uploadLayout->layout.axes.x,
        uploadLayout->layout.axes.y,
        glm::to_string(uploadLayout->uploadSize));
    }

    std::vector<GLTexture> componentTextures;

    try {
      switch (image->bufferType()) {
        case Image::MultiComponentBufferType::InterleavedImage: {
          spdlog::debug(
            "Image {} has {} interleaved component(s); creating one scalar texture per logical component.",
            imageUid,
            numComp);
          for (uint32_t comp = 0; comp < numComp; ++comp) {
            if (!appendDeinterleavedComponentTexture(
                  componentTextures,
                  *image,
                  comp,
                  activeTimePoint,
                  *uploadLayout,
                  pixelPackSettings,
                  pixelUnpackSettings,
                  sk_wrapModeClampToEdge))
            {
              spdlog::warn("Image {} could not create a scalar texture for component {}", imageUid, comp);
              componentTextures.clear();
              break;
            }
          }
          break;
        }
        case Image::MultiComponentBufferType::SeparateImages: {
          spdlog::debug(
            "Image {} has {} separate components, so {} textures will be created.",
            imageUid,
            numComp,
            numComp);

          for (uint32_t comp = 0; comp < numComp; ++comp) {
            tex::MinificationFilter minFilter = tex::MinificationFilter::Linear;
            tex::MagnificationFilter maxFilter = tex::MagnificationFilter::Linear;

            switch (image->settings().interpolationMode(comp)) {
              case InterpolationMode::NearestNeighbor: {
                minFilter = tex::MinificationFilter::Nearest;
                maxFilter = tex::MagnificationFilter::Nearest;
                break;
              }
              case InterpolationMode::Linear:
              case InterpolationMode::CubicBsplineConvolution: {
                minFilter = tex::MinificationFilter::Linear;
                maxFilter = tex::MagnificationFilter::Linear;
                break;
              }
            }

            // Use Red format for each component texture:
            const tex::SizedInternalFormat sizedInternalNormalizedFormat =
              GLTexture::getSizedInternalNormalizedRedFormat(compType);

            const tex::BufferPixelFormat bufferPixelNormalizedFormat =
              GLTexture::getBufferPixelNormalizedRedFormat(compType);

            if (RenderData::TextureDimension::Texture2D == uploadLayout->layout.dimension) {
              if (!appendDeinterleavedComponentTexture(
                    componentTextures,
                    *image,
                    comp,
                    activeTimePoint,
                    *uploadLayout,
                    pixelPackSettings,
                    pixelUnpackSettings,
                    sk_wrapModeClampToEdge))
              {
                spdlog::warn("Image {} could not create a planar scalar texture for component {}", imageUid, comp);
                componentTextures.clear();
                break;
              }
            }
            else {
              GLTexture& T = componentTextures.emplace_back(
                tex::Target::Texture3D,
                GLTexture::MultisampleSettings(),
                pixelPackSettings,
                pixelUnpackSettings);

              T.generate();
              T.setMinificationFilter(minFilter);
              T.setMagnificationFilter(maxFilter);
              //                T.setBorderColor( sk_border );
              T.setWrapMode(sk_wrapModeClampToEdge);
              T.setAutoGenerateMipmaps(false); // no mipmapping for images
              T.setSize(textureSize);

              const void* imageBuffer = image->bufferAsVoid(comp, activeTimePoint);
              if (!imageBuffer) {
                spdlog::warn("Image {} has no texture data for component {}", imageUid, comp);
                componentTextures.clear();
                break;
              }

              T.setData(
                sk_mipmapLevel,
                sizedInternalNormalizedFormat,
                bufferPixelNormalizedFormat,
                GLTexture::getBufferPixelDataType(compType),
                imageBuffer);
            }
          }

          spdlog::debug("Done creating {} image component textures", componentTextures.size());
          break;
        }
      } // end switch ( image->bufferType() )
    }
    catch (const std::exception& e) {
      spdlog::error("Image {} ('{}') texture upload failed: {}", imageUid, image->settings().displayName(), e.what());
      result.failures.push_back(makeTextureUploadFailure(
        TextureCreationFailure::Resource::Image,
        imageUid,
        image->settings().displayName(),
        textureSize,
        textureLimits.max3DTextureSize,
        e.what()));
      componentTextures.clear();
    }

    if (componentTextures.empty()) {
      spdlog::warn("No image textures were created for image {}", imageUid);
      continue;
    }

    appData.renderData().m_imageTextures.emplace(imageUid, std::move(componentTextures));
    appData.renderData().m_imageTextureLayouts[imageUid] = uploadLayout->layout;

    result.createdUids.push_back(imageUid);

    spdlog::debug("Done creating texture(s) for image {} ('{}')", imageUid, image->settings().displayName());
  } // end for ( const auto& imageUid : appData.imageUidsOrdered() )

  spdlog::debug("Done creating textures for {} image(s)", result.createdUids.size());

  return result;
}

std::vector<uuids::uuid> createImageTextures(AppData& appData, const uuid_range_t& imageUids)
{
  TextureCreationResult result = createImageTexturesWithReport(appData, imageUids);
  handleTextureCreationFailures(appData, result.failures);
  return result.createdUids;
}

bool refreshImageTexturesForActiveTimePoint(AppData& appData, const uuids::uuid& imageUid)
{
  const Image* image = appData.image(imageUid);
  if (!image) {
    image = appData.def(imageUid);
  }
  if (!image || !image->hasPixelData()) {
    return false;
  }

  const texture_setup::TextureLimits textureLimits = logTextureLimitsOnce();
  const std::optional<TextureUploadLayout> uploadLayout =
    texture_setup::textureUploadLayoutForImage(image->header().pixelDimensions(), textureLimits);
  if (!uploadLayout) {
    appData.renderData().m_imageTextures.erase(imageUid);
    appData.renderData().m_imageTextureLayouts.erase(imageUid);
    createImageTextures(appData, uuid_range_t{imageUid});
    return false;
  }

  auto textureIt = appData.renderData().m_imageTextures.find(imageUid);
  const auto layoutIt = appData.renderData().m_imageTextureLayouts.find(imageUid);
  const bool canUpdateExisting =
    textureIt != std::end(appData.renderData().m_imageTextures) &&
    layoutIt != std::end(appData.renderData().m_imageTextureLayouts) &&
    layoutIt->second.dimension == uploadLayout->layout.dimension &&
    layoutIt->second.axes == uploadLayout->layout.axes &&
    textureIt->second.size() == image->header().numComponentsPerPixel() &&
    std::all_of(textureIt->second.begin(), textureIt->second.end(), [&uploadLayout](const GLTexture& texture) {
      return texture.size() == uploadLayout->uploadSize;
    });

  if (!canUpdateExisting) {
    appData.renderData().m_imageTextures.erase(imageUid);
    appData.renderData().m_imageTextureLayouts.erase(imageUid);
    createImageTextures(appData, uuid_range_t{imageUid});
    return false;
  }

  const uint32_t activeTimePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
  std::vector<GLTexture>& textures = textureIt->second;
  for (uint32_t component = 0; component < image->header().numComponentsPerPixel(); ++component) {
    if (!uploadActiveTimePointToExistingTexture(
          textures.at(component),
          *image,
          component,
          activeTimePoint,
          layoutIt->second))
    {
      spdlog::warn("Could not refresh texture for component {} of image {}", component, imageUid);
      appData.renderData().m_imageTextures.erase(imageUid);
      appData.renderData().m_imageTextureLayouts.erase(imageUid);
      createImageTextures(appData, uuid_range_t{imageUid});
      return false;
    }
  }

  return true;
}

std::unordered_map<uuids::uuid, std::unordered_map<uint32_t, GLTexture>> createDistanceMapTextures(
  const AppData& appData)
{
  static constexpr GLint sk_mipmapLevel = 0; // Load distance map data into first mipmap level
  static constexpr GLint sk_alignment = 1;   // Pixel pack/unpack alignment is 1 byte

  static const tex::WrapMode sk_wrapModeClampToEdge = tex::WrapMode::ClampToEdge;

  // Distance maps have unsigned 8-bit integer components
  static const ComponentType sk_compType = ComponentType::UInt8;

  // Distance map textures are not interpolated
  static const tex::MinificationFilter sk_minFilter = tex::MinificationFilter::Nearest;
  static const tex::MagnificationFilter sk_maxFilter = tex::MagnificationFilter::Nearest;

  // Use Red integer format for each distance map texture:
  const tex::SizedInternalFormat k_sizedInternalNormalizedFormat = GLTexture::getSizedInternalRedFormat(sk_compType);

  // Use this for Red float format:
  // GLTexture::getSizedInternalNormalizedRedFormat( sk_compType );

  const tex::BufferPixelFormat k_bufferPixelNormalizedFormat = GLTexture::getBufferPixelRedFormat(sk_compType);

  // Use this for Red float format:
  // GLTexture::getBufferPixelNormalizedRedFormat( sk_compType );

  // Map from image UID to vector of textures for the distance maps of the image components.
  std::unordered_map<uuids::uuid, std::unordered_map<uint32_t, GLTexture>> mapTextures;

  if (0 == appData.numImages()) {
    spdlog::warn("No images are loaded for which to create distance map textures");
    return mapTextures;
  }

  spdlog::debug("Begin creating 3D distance map textures for image components");

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = sk_alignment;
  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  for (const auto& imageUid : appData.imageUidsOrdered()) {
    spdlog::debug("Begin creating distance map texture(s) for components of image {}", imageUid);

    // const auto* image = appData.image(imageUid);
    // if (!image) {
    //   spdlog::warn("Image {} is invalid", imageUid);
    //   continue;
    // }

    const auto result = appData.getImage(imageUid);
    if (!result) {
      spdlog::warn("{}", result.error());
      continue;
    }

    const Image& image = result->get();
    const uint32_t numComp = image.header().numComponentsPerPixel();

    if (imageUsesPlanar2DTexture(appData, imageUid)) {
      spdlog::debug(
        "Skipping 3D distance map texture creation for image {} because the image is uploaded as GL_TEXTURE_2D",
        imageUid);
      continue;
    }

    // Map of component index to texture
    std::unordered_map<uint32_t, GLTexture> componentTextures;

    for (uint32_t comp = 0; comp < numComp; ++comp) {
      const std::map<double, Image>& maps = appData.distanceMaps(imageUid, comp);
      if (maps.empty()) {
        spdlog::warn("No distance map for component {} of image {}", comp, imageUid);
        continue;
      }

      // Get the first map:
      const Image& map = maps.begin()->second;

      auto it = componentTextures.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(comp),
        std::forward_as_tuple(
          tex::Target::Texture3D,
          GLTexture::MultisampleSettings(),
          pixelPackSettings,
          pixelUnpackSettings));

      it.first->second.generate();
      it.first->second.setMinificationFilter(sk_minFilter);
      it.first->second.setMagnificationFilter(sk_maxFilter);
      it.first->second.setWrapMode(sk_wrapModeClampToEdge);
      it.first->second.setAutoGenerateMipmaps(false);
      it.first->second.setSize(map.header().pixelDimensions());

      it.first->second.setData(
        sk_mipmapLevel,
        k_sizedInternalNormalizedFormat,
        k_bufferPixelNormalizedFormat,
        GLTexture::getBufferPixelDataType(sk_compType),
        map.bufferAsVoid(0));
    }

    spdlog::debug("Done creating {} distance map textures for image components", componentTextures.size());
    mapTextures.emplace(imageUid, std::move(componentTextures));
  }

  spdlog::debug("Done creating textures for {} distance map(s)", mapTextures.size());
  return mapTextures;
}

TextureCreationResult createSegTexturesWithReport(AppData& appData, const uuid_range_t& segUids)
{
  // Load the first pixel component of the segmentation image.
  // (Segmentations should have only one component.)
  constexpr uint32_t k_comp0 = 0;

  constexpr GLint k_mipmapLevel = 0; // Load seg data into first mipmap level
  constexpr GLint k_alignment = 1;   // Pixel pack/unpack alignment is 1 byte

  static const tex::WrapMode sk_wrapMode = tex::WrapMode::ClampToBorder;
  static const glm::vec4 sk_border{0.0f, 0.0f, 0.0f, 0.0f}; // Black border

  // Nearest-neighbor interpolation is used for segmentation textures:
  static const tex::MinificationFilter sk_minFilter = tex::MinificationFilter::Nearest;
  static const tex::MagnificationFilter sk_maxFilter = tex::MagnificationFilter::Nearest;

  std::unordered_map<uuids::uuid, GLTexture> textures;

  TextureCreationResult result;

  spdlog::debug("Begin creating 3D segmentation textures");
  const texture_setup::TextureLimits textureLimits = logTextureLimitsOnce();

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = k_alignment;
  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  // Loop through images in order of index
  for (const auto& segUid : segUids) {
    const auto* seg = appData.seg(segUid);
    if (!seg) {
      spdlog::warn("Segmentation {} is invalid", segUid);
      continue;
    }

    if (!seg->hasPixelData()) {
      spdlog::debug("Skipping texture creation for header-only segmentation {}", segUid);
      continue;
    }

    const ComponentType compType = seg->header().memoryComponentType();
    const glm::uvec3 textureSize = seg->header().pixelDimensions();
    const std::optional<texture_setup::TextureUploadLayout> uploadLayout =
      texture_setup::textureUploadLayoutForImage(textureSize, textureLimits);
    if (!uploadLayout) {
      spdlog::error(
        "Segmentation {} ('{}') has dimensions {} and cannot be uploaded as an OpenGL texture. {}",
        segUid,
        seg->settings().displayName(),
        glm::to_string(textureSize),
        texture_setup::textureLimitReason(textureSize, textureLimits));
      result.failures.push_back(makeTextureSizeFailure(
        TextureCreationFailure::Resource::Segmentation,
        segUid,
        seg->settings().displayName(),
        textureSize,
        textureLimits.max3DTextureSize,
        texture_setup::textureLimitReason(textureSize, textureLimits)));
      continue;
    }
    if (RenderData::TextureDimension::Texture2D == uploadLayout->layout.dimension && shouldLogPlanarSegUpload(segUid)) {
      spdlog::info(
        "Segmentation {} ('{}') exceeds GL_MAX_3D_TEXTURE_SIZE but is planar; uploading as GL_TEXTURE_2D with axes "
        "({}, {}) and size {}",
        segUid,
        seg->settings().displayName(),
        uploadLayout->layout.axes.x,
        uploadLayout->layout.axes.y,
        glm::to_string(uploadLayout->uploadSize));
    }

    try {
      auto it = appData.renderData().m_segTextures.try_emplace(
        segUid,
        RenderData::TextureDimension::Texture2D == uploadLayout->layout.dimension ? tex::Target::Texture2D
                                                                                  : tex::Target::Texture3D,
        GLTexture::MultisampleSettings(),
        pixelPackSettings,
        pixelUnpackSettings);

      if (!it.second) continue;
      GLTexture& T = it.first->second;

      T.generate();
      T.setMinificationFilter(sk_minFilter);
      T.setMagnificationFilter(sk_maxFilter);
      T.setBorderColor(sk_border);
      T.setWrapMode(sk_wrapMode);
      T.setAutoGenerateMipmaps(false); // no mipmapping for segmentations
      T.setSize(uploadLayout->uploadSize);

      std::vector<std::byte> planarValues;
      const void* segBuffer = seg->bufferAsVoid(k_comp0);
      if (RenderData::TextureDimension::Texture2D == uploadLayout->layout.dimension) {
        planarValues = copyPlanarSegValuesBytes(*seg, uploadLayout->layout);
        segBuffer = planarValues.data();
      }

      T.setData(
        k_mipmapLevel,
        GLTexture::getSizedInternalRedFormat(compType),
        GLTexture::getBufferPixelRedFormat(compType),
        GLTexture::getBufferPixelDataType(compType),
        segBuffer);
      appData.renderData().m_segTextureLayouts[segUid] = uploadLayout->layout;
    }
    catch (const std::exception& e) {
      appData.renderData().m_segTextures.erase(segUid);
      appData.renderData().m_segTextureLayouts.erase(segUid);
      spdlog::error(
        "Segmentation {} ('{}') texture upload failed: {}",
        segUid,
        seg->settings().displayName(),
        e.what());
      result.failures.push_back(makeTextureUploadFailure(
        TextureCreationFailure::Resource::Segmentation,
        segUid,
        seg->settings().displayName(),
        textureSize,
        textureLimits.max3DTextureSize,
        e.what()));
      continue;
    }

    spdlog::debug("Created texture for segmentation {} ('{}')", segUid, seg->settings().displayName());

    result.createdUids.push_back(segUid);
  }

  spdlog::debug("Done creating {} segmentation textures", result.createdUids.size());
  return result;
}

std::vector<uuids::uuid> createSegTextures(AppData& appData, const uuid_range_t& segUids)
{
  TextureCreationResult result = createSegTexturesWithReport(appData, segUids);
  handleTextureCreationFailures(appData, result.failures);
  return result.createdUids;
}

std::unordered_map<uuids::uuid, GLTexture> createImageColorMapTextures(const AppData& appData)
{
  static const glm::vec4 sk_border{0.0f, 0.0f, 0.0f, 0.0f};
  std::unordered_map<uuids::uuid, GLTexture> textures;

  if (0 == appData.numImageColorMaps()) {
    spdlog::warn("No image color maps loaded for which to create textures");
    return textures;
  }

  spdlog::debug("Begin creating image color map textures");

  // Loop through color maps in order of index
  for (std::size_t i = 0; i < appData.numImageColorMaps(); ++i) {
    const auto cmapUid = appData.imageColorMapUid(i);
    if (!cmapUid) {
      spdlog::warn("Image color map index {} is invalid", i);
      continue;
    }

    const auto* map = appData.imageColorMap(*cmapUid);
    if (!map) {
      spdlog::warn("Image color map {} is invalid", *cmapUid);
      continue;
    }

    auto it = textures.emplace(*cmapUid, tex::Target::Texture1D);
    if (!it.second) continue;

    GLTexture& T = it.first->second;

    T.generate();
    T.setSize(glm::uvec3{map->numColors(), 1, 1});

    T.setData(
      0,
      tex::SizedInternalFormat::RGBA32F,
      tex::BufferPixelFormat::RGBA,
      tex::BufferPixelDataType::Float32,
      map->data_RGBA_F32());

    if (map->transparentBorder()) {
      T.setWrapMode(tex::WrapMode::ClampToBorder);
      T.setBorderColor(sk_border);
    }
    else {
      // We should never sample outside the texture coordinate range [0.0, 1.0], anyway
      T.setWrapMode(tex::WrapMode::ClampToEdge);
    }

    T.setAutoGenerateMipmaps(false);

    switch (map->interpolationMode()) {
      case ImageColorMap::InterpolationMode::Nearest: {
        T.setMinificationFilter(tex::MinificationFilter::Nearest);
        T.setMagnificationFilter(tex::MagnificationFilter::Nearest);
        break;
      }
      case ImageColorMap::InterpolationMode::Linear: {
        T.setMinificationFilter(tex::MinificationFilter::Linear);
        T.setMagnificationFilter(tex::MagnificationFilter::Linear);
        break;
      }
    }

    SPDLOG_TRACE("Generated texture for image color map {}", *cmapUid);
  }

  spdlog::debug("Done creating {} image color map textures", textures.size());
  return textures;
}

std::unordered_map<uuids::uuid, GLBufferTexture> createLabelColorTableTextures(const AppData& appData)
{
  // static const glm::vec4 sk_border{ 0.0f, 0.0f, 0.0f, 0.0f };

  std::unordered_map<uuids::uuid, GLBufferTexture> bufTextures;

  if (0 == appData.numLabelTables()) {
    spdlog::warn("No parcellation label color tables loaded for which to create buffer textures");
    return bufTextures;
  }

  spdlog::debug("Begin creating label color map buffer textures");

  // Loop through label tables in order of index
  for (std::size_t i = 0; i < appData.numLabelTables(); ++i) {
    const auto tableUid = appData.labelTableUid(i);
    if (!tableUid) {
      spdlog::error("Label table index {} is invalid", i);
      continue;
    }

    const auto* table = appData.labelTable(*tableUid);
    if (!table) {
      spdlog::error("Label table {} is invalid", *tableUid);
      continue;
    }

    int maxBufTexSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBufTexSize);

    if (table->numColorBytes_RGBA_U8() > static_cast<size_t>(maxBufTexSize)) {
      spdlog::error(
        "Number of bytes ({}) in label color table {} exceeds "
        "maximum buffer texture size of {} bytes",
        table->numColorBytes_RGBA_U8(),
        *tableUid,
        maxBufTexSize);
      continue;
    }

    auto it = bufTextures.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(*tableUid),
      std::forward_as_tuple(table->bufferTextureFormat_RGBA_U8(), BufferUsagePattern::StaticDraw));

    if (!it.second) continue;

    GLBufferTexture& T = it.first->second;

    T.generate();

    T.allocate(table->numColorBytes_RGBA_U8(), table->colorData_RGBA_nonpremult_U8());

    spdlog::debug(
      "Generated and set data for buffer texture of size {} for label color table {}",
      table->numColorBytes_RGBA_U8(),
      *tableUid);

    spdlog::debug("Generated buffer texture for label color table {}", *tableUid);
  }

  spdlog::debug("Done creating {} label color map buffer textures", bufTextures.size());
  return bufTextures;
}
