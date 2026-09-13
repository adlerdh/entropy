#include "rendering/ShaderTextureDimension.h"

#include <iterator>

namespace rendering
{

std::unordered_map<std::string, std::string> shaderReplacementsForTextureDimension(
  const std::unordered_map<std::string, std::string>& replacements,
  const TextureDimension dimension,
  const TextureLookupReplacementSources& lookupSources)
{
  std::unordered_map<std::string, std::string> result = replacements;

  if (TextureDimension::Texture2D == dimension) {
    result["IMAGE_SAMPLER_TYPE"] = "sampler2D";
    result["SEG_SAMPLER_TYPE"] = "usampler2D";

    if (const auto it = result.find("TEXTURE_LOOKUP_FUNCTION"); it != std::end(result)) {
      if (it->second == lookupSources.cubic3D) {
        it->second = lookupSources.cubic2D;
      }
      else if (it->second == lookupSources.floatingPointLinear3D) {
        it->second = lookupSources.floatingPointLinear2D;
      }
      else {
        it->second = lookupSources.linear2D;
      }
    }

    if (const auto it = result.find("UINT_TEXTURE_LOOKUP_FUNCTION"); it != std::end(result)) {
      it->second = lookupSources.uintLinear2D;
    }

    return result;
  }

  result["IMAGE_SAMPLER_TYPE"] = "sampler3D";
  result["SEG_SAMPLER_TYPE"] = "usampler3D";
  return result;
}

} // namespace rendering
