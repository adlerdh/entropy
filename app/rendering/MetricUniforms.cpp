#include "rendering/Rendering.h"

#include "image/ImageColorMap.h"
#include "logic/app/Data.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

void Rendering::updateMetricUniforms()
{
  auto update = [this](RenderData::MetricParams& params, const char* name) {
    if (const auto cmapUid = m_appData.imageColorMapUid(params.m_colorMapIndex)) {
      if (const auto* map = m_appData.imageColorMap(*cmapUid)) {
        params.m_cmapSlopeIntercept = map->slopeIntercept(params.m_invertCmap);
      }
      else {
        spdlog::error("Null image color map {} on updating uniforms for {} metric", *cmapUid, name);
      }
    }
    else {
      spdlog::error(
        "Invalid image color map at index {} on updating uniforms for {} metric",
        params.m_colorMapIndex,
        name);
    }
  };

  update(m_appData.renderData().m_squaredDifferenceParams, "Difference");
  update(m_appData.renderData().m_localNccParams, "Local NCC");
  update(m_appData.renderData().m_localLinearResidualParams, "Local Linear Residual");
  update(m_appData.renderData().m_jointHistogramParams, "Joint Histogram");
}
