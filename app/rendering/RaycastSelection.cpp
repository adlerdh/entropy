#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "windowing/View.h"

#include <optional>

std::optional<Rendering::ImgSegPair> Rendering::raycastImageForView(const View& view)
{
  const CurrentImages imageSegPairs = getImageAndSegUidsForImageShaders(
    rendering::raycastableImageUids(view.visibleImages(), m_appData.renderData().m_imageTextureLayouts));

  if (imageSegPairs.empty()) {
    return std::nullopt;
  }

  return imageSegPairs.front();
}
