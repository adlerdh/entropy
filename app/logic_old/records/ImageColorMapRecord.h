#pragma once

#include "image/ImageColorMap.h"
#include "logic_old/RenderableRecord.h"
#include "rendering/utility/gl/GLTexture.h"

/**
 * Record for an image color map. It is represented on GPU as a 2D texture.
 */
using ImageColorMapRecord = RenderableRecord<ImageColorMap, GLTexture>;
