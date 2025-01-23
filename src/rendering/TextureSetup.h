#ifndef TEXTURE_SETUP_H
#define TEXTURE_SETUP_H

#include "common/UuidRange.h"

#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"

#include <unordered_map>
#include <uuid.h>

class AppData;

// Return vector of image UIDs for which textures were created
std::vector<uuids::uuid> createImageTextures(AppData& appData, uuid_range_t imageUids);

// Return vector of seg UIDs for which textures were created
std::vector<uuids::uuid> createSegTextures(AppData& appData, uuid_range_t segUids);

std::unordered_map<uuids::uuid, std::unordered_map<uint32_t, GLTexture> > createDistanceMapTextures(
  const AppData& appData
);

std::unordered_map<uuids::uuid, GLTexture> createImageColorMapTextures(const AppData& appData);

std::unordered_map<uuids::uuid, GLBufferTexture> createLabelColorTableTextures(const AppData& appData
);

#endif // TEXTURE_SETUP_H
