#include "image/CtWindowing.h"
#include "image/DicomSeries.h"
#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/ImageDerivedData.h"
#include "image/ImageHeader.h"
#include "image/ImageHeaderOverrides.h"
#include "image/ImageIoInfo.h"
#include "image/ImageSettings.h"
#include "image/ImageSpatialMetadata.h"
#include "image/ImageTimeAxis.h"
#include "image/ImageTransformations.h"
#include "image/ImageTypes.h"
#include "image/ImageUtility.h"
#include "image/ImageWindowDefaults.h"
#include "image/ImageWriter.h"
#include "image/Isosurface.h"
#include "image/RegionStatistics.h"
#include "image/SegUtil.h"
#include "image/TimePlaybackController.h"
#include "image/WarpInversion.h"
#include "image/external/TDigest.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("image public headers are available through the library include interface", "[image][headers]")
{
  SUCCEED("EntropyImage public headers compiled successfully");
}
