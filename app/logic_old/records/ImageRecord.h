#ifndef IMAGE_RECORD_H
#define IMAGE_RECORD_H

#include "image/Image.h"
#include "logic_old/RenderableRecord.h"
#include "rendering_old/records/ImageGpuRecord.h"

/**
 * Record for an image. Nominally a 3D image, but could also be 1D or 2D.
 */
using ImageRecord = RenderableRecord<Image, ImageGpuRecord>;

#endif // IMAGE_RECORD_H
