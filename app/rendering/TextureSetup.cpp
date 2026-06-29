#include "rendering/TextureSetup.h"

#include "logic/app/Data.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace
{

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

template<typename T>
bool appendScalarComponentTexture(
  std::vector<GLTexture>& componentTextures,
  const Image& image,
  uint32_t component,
  const T* data,
  const GLTexture::PixelStoreSettings& pixelPackSettings,
  const GLTexture::PixelStoreSettings& pixelUnpackSettings,
  tex::WrapMode wrapMode)
{
  if (!data) {
    return false;
  }

  tex::MinificationFilter minFilter;
  tex::MagnificationFilter maxFilter;
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
    tex::Target::Texture3D,
    GLTexture::MultisampleSettings(),
    pixelPackSettings,
    pixelUnpackSettings);

  texture.generate();
  texture.setMinificationFilter(minFilter);
  texture.setMagnificationFilter(maxFilter);
  texture.setWrapMode(wrapMode);
  texture.setAutoGenerateMipmaps(false);
  texture.setSize(image.header().pixelDimensions());
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
  const GLTexture::PixelStoreSettings& pixelPackSettings,
  const GLTexture::PixelStoreSettings& pixelUnpackSettings,
  tex::WrapMode wrapMode)
{
  const std::vector<T> values = copyComponentFrameValues<T>(image, component, timePoint);
  return values.size() == image.header().numPixels() && appendScalarComponentTexture(
                                                          componentTextures,
                                                          image,
                                                          component,
                                                          values.data(),
                                                          pixelPackSettings,
                                                          pixelUnpackSettings,
                                                          wrapMode);
}

bool appendDeinterleavedComponentTexture(
  std::vector<GLTexture>& componentTextures,
  const Image& image,
  uint32_t component,
  uint32_t timePoint,
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
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::UInt8:
      return appendDeinterleavedComponentTexture<uint8_t>(
        componentTextures,
        image,
        component,
        timePoint,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::Int16:
      return appendDeinterleavedComponentTexture<int16_t>(
        componentTextures,
        image,
        component,
        timePoint,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::UInt16:
      return appendDeinterleavedComponentTexture<uint16_t>(
        componentTextures,
        image,
        component,
        timePoint,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::Int32:
      return appendDeinterleavedComponentTexture<int32_t>(
        componentTextures,
        image,
        component,
        timePoint,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::UInt32:
      return appendDeinterleavedComponentTexture<uint32_t>(
        componentTextures,
        image,
        component,
        timePoint,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    case ComponentType::Float32:
      return appendDeinterleavedComponentTexture<float>(
        componentTextures,
        image,
        component,
        timePoint,
        pixelPackSettings,
        pixelUnpackSettings,
        wrapMode);
    default:
      return false;
  }
}

} // namespace

std::vector<uuids::uuid> createImageTextures(AppData& appData, uuid_range_t imageUids)
{
  static constexpr GLint sk_mipmapLevel = 0; // Load image data into first mipmap level
  static constexpr GLint sk_alignment = 1;   // Pixel pack/unpack alignment is 1 byte

  // Clamping to edge is needed for raycasting, so that an isosurface is not rendered
  // on the volume faces
  static const tex::WrapMode sk_wrapModeClampToEdge = tex::WrapMode::ClampToEdge;
  //    static const tex::WrapMode sk_wrapModeClampToBorder = tex::WrapMode::ClampToBorder;
  //    static const glm::vec4 sk_border{ 0.0f, 0.0f, 0.0f, 0.0f }; // Black border

  // Map from image UID to textures used for component rendering. Each logical component gets a
  // scalar texture, regardless of whether the CPU image buffer is separated or interleaved. This
  // matches the shader interface, where RGB/RGBA/vector modes sample u_imgTex[component].
  std::unordered_map<uuids::uuid, std::vector<GLTexture>> imageTextures;

  std::vector<uuids::uuid> createdImageTexUids;

  spdlog::debug("Begin creating 3D image textures");

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

    std::vector<GLTexture> componentTextures;

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
          tex::MinificationFilter minFilter;
          tex::MagnificationFilter maxFilter;

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
          T.setSize(image->header().pixelDimensions());

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

        spdlog::debug("Done creating {} image component textures", componentTextures.size());
        break;
      }
    } // end switch ( image->bufferType() )

    if (componentTextures.empty()) {
      spdlog::warn("No image textures were created for image {}", imageUid);
      continue;
    }

    appData.renderData().m_imageTextures.emplace(imageUid, std::move(componentTextures));

    createdImageTexUids.push_back(imageUid);

    spdlog::debug("Done creating texture(s) for image {} ('{}')", imageUid, image->settings().displayName());
  } // end for ( const auto& imageUid : appData.imageUidsOrdered() )

  spdlog::debug("Done creating textures for {} image(s)", imageTextures.size());

  return createdImageTexUids;
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

std::vector<uuids::uuid> createSegTextures(AppData& appData, uuid_range_t segUids)
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

  std::vector<uuids::uuid> createdSegTexUids;

  spdlog::debug("Begin creating 3D segmentation textures");

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

    auto it = appData.renderData().m_segTextures.try_emplace(
      segUid,
      tex::Target::Texture3D,
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
    T.setSize(seg->header().pixelDimensions());

    T.setData(
      k_mipmapLevel,
      GLTexture::getSizedInternalRedFormat(compType),
      GLTexture::getBufferPixelRedFormat(compType),
      GLTexture::getBufferPixelDataType(compType),
      seg->bufferAsVoid(k_comp0));

    spdlog::debug("Created texture for segmentation {} ('{}')", segUid, seg->settings().displayName());

    createdSegTexUids.push_back(segUid);
  }

  spdlog::debug("Done creating {} segmentation textures", textures.size());
  return createdSegTexUids;
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

    int maxBufTexSize;
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
