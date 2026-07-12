#pragma once

#include "common/UuidRange.h"

#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <uuid.h>

class AppData;

/**
 * @brief Description of an image or segmentation texture that could not be created.
 *
 * Failures are returned to the app layer so the user can decide whether to keep non-renderable data loaded for
 * metadata/project inspection or unload it from the current project.
 */
struct TextureCreationFailure
{
  /// Type of resource whose texture upload failed.
  enum class Resource
  {
    Image,       //!< Image texture upload failed.
    Segmentation //!< Segmentation texture upload failed.
  };

  Resource resource;                  //!< Resource type.
  uuids::uuid uid;                    //!< Resource UID.
  std::string displayName;            //!< User-facing resource name.
  std::array<uint32_t, 3> dimensions; //!< Resource dimensions in voxels.
  int max3DTextureSize;               //!< GL_MAX_3D_TEXTURE_SIZE reported by the current OpenGL context.
  std::string reason;                 //!< User-facing explanation of the failure.
};

/**
 * @brief Result of a batch texture creation pass.
 */
struct TextureCreationResult
{
  std::vector<uuids::uuid> createdUids;         //!< Resource UIDs for which textures were created successfully.
  std::vector<TextureCreationFailure> failures; //!< Texture creation failures that require user/app handling.
};

/**
 * @brief Create or recreate image textures and report failures without showing user prompts.
 * @param appData Application data containing images and render data.
 * @param imageUids Image UIDs for which textures should be created.
 * @return Created image UIDs and texture creation failures.
 */
TextureCreationResult createImageTexturesWithReport(AppData& appData, uuid_range_t imageUids);

/**
 * @brief Create or recreate image textures and show user prompts for failures.
 * @param appData Application data containing images and render data.
 * @param imageUids Image UIDs for which textures should be created.
 * @return Image UIDs for which textures were created successfully.
 */
std::vector<uuids::uuid> createImageTextures(AppData& appData, uuid_range_t imageUids);

/**
 * @brief Refresh an existing image texture from the image's active time point when possible.
 *
 * This avoids reallocating texture storage for compatible time-series updates. If the existing texture is missing or no
 * longer compatible with the image data, the function falls back to recreating the image textures.
 *
 * @param appData Application data containing images and render data.
 * @param imageUid Image UID whose active time point should be uploaded.
 * @return True when a compatible texture was refreshed or recreated successfully.
 */
bool refreshImageTexturesForActiveTimePoint(AppData& appData, const uuids::uuid& imageUid);

/**
 * @brief Create or recreate segmentation textures and report failures without showing user prompts.
 * @param appData Application data containing segmentations and render data.
 * @param segUids Segmentation UIDs for which textures should be created.
 * @return Created segmentation UIDs and texture creation failures.
 */
TextureCreationResult createSegTexturesWithReport(AppData& appData, uuid_range_t segUids);

/**
 * @brief Create or recreate segmentation textures and show user prompts for failures.
 * @param appData Application data containing segmentations and render data.
 * @param segUids Segmentation UIDs for which textures should be created.
 * @return Segmentation UIDs for which textures were created successfully.
 */
std::vector<uuids::uuid> createSegTextures(AppData& appData, uuid_range_t segUids);

/**
 * @brief Create distance-map textures for all available image distance maps.
 * @param appData Application data containing image derived data and render data.
 * @return Distance-map textures keyed by image UID and component index.
 */
std::unordered_map<uuids::uuid, std::unordered_map<uint32_t, GLTexture> > createDistanceMapTextures(
  const AppData& appData);

/**
 * @brief Create OpenGL textures for all image color maps.
 * @param appData Application data containing image color maps.
 * @return Color-map textures keyed by color-map UID.
 */
std::unordered_map<uuids::uuid, GLTexture> createImageColorMapTextures(const AppData& appData);

/**
 * @brief Create label color-table buffer textures for all segmentations.
 * @param appData Application data containing segmentation label tables.
 * @return Label color-table buffer textures keyed by segmentation UID.
 */
std::unordered_map<uuids::uuid, GLBufferTexture> createLabelColorTableTextures(const AppData& appData);
