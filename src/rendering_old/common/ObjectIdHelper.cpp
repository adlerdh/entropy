#include "rendering_old/common/ObjectIdHelper.h"
#include "rendering/utility/UnderlyingEnumType.h"

bool isDrawableType(uint16_t objectId, const DrawableType& type)
{
  const uint32_t typeNum = static_cast<uint32_t>(underlyingType(type));

  if ((objectId >> 12) & typeNum)
  {
    return true;
  }

  return false;
}
