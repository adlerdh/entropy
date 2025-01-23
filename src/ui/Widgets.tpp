#pragma once

#include "image/ImageSettings.h"
#include <implot.h>
#include <string>

/// @todo Icon for histogram: https://forkaweso.me/Fork-Awesome/icon/area-chart/
template<typename T>
void drawImageHistogram(
  const T* data, int dataSize, ImageSettings& settings, const std::string& imagePrecisionFormat
)
{
  HistogramSettings& histoSettings = settings.histogramSettings();

  ImPlotHistogramFlags flags = 0;
  flags |= (histoSettings.m_isCumulative) ? ImPlotHistogramFlags_Cumulative : 0;
  flags |= (histoSettings.m_isDensity) ? ImPlotHistogramFlags_Density : 0;
  flags |= (histoSettings.m_isHorizontal) ? ImPlotHistogramFlags_Horizontal : 0;

  std::string plotTitle;

  if (histoSettings.m_isCumulative)
  {
    plotTitle = "Cumulative Histogram";
    if (histoSettings.m_isDensity)
    {
      plotTitle += " Density";
    }
  }
  else
  {
    plotTitle = "Histogram";
    if (histoSettings.m_isDensity)
    {
      plotTitle += " Density";
    }
  }

  std::string countAxisLabel;
  if (histoSettings.m_isLogScale)
  {
    countAxisLabel = (histoSettings.m_isDensity) ? "log(Probability)" : "log(Count)";
  }
  else
  {
    countAxisLabel = (histoSettings.m_isDensity) ? "Probability" : "Count";
  }

  const double intensityAxisMin = (histoSettings.m_useCustomIntensityRange)
                                    ? histoSettings.m_intensityRange[0]
                                    : settings.componentStatistics().m_minimum;

  const double intensityAxisMax = (histoSettings.m_useCustomIntensityRange)
                                    ? histoSettings.m_intensityRange[1]
                                    : settings.componentStatistics().m_maximum;

  const double intensityAxisRange = intensityAxisMax - intensityAxisMin;

  if (ImPlot::BeginPlot(plotTitle.c_str()))
  {
    if (histoSettings.m_isHorizontal)
    {
      ImPlot::SetupAxes(
        countAxisLabel.c_str(), "Intensity", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit
      );
      if (histoSettings.m_isLogScale)
      {
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
      }
    }
    else
    {
      ImPlot::SetupAxes(
        "Intensity", countAxisLabel.c_str(), ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit
      );
      if (histoSettings.m_isLogScale)
      {
        ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
      }
    }

    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);

    ImPlot::PlotHistogram(
      "##PlotHistogram",
      data,
      dataSize,
      histoSettings.m_numBins,
      1.0,
      ImPlotRange(intensityAxisMin, intensityAxisMax),
      flags
    );

    const auto windowLowHigh = settings.windowValuesLowHigh();
    const ImPlotInfLinesFlags infLineFlags = (histoSettings.m_isHorizontal)
                                               ? ImPlotInfLinesFlags_Horizontal
                                               : 0;

    ImPlot::PushColormap(ImPlotColormap_Deep);

    if (intensityAxisMin <= windowLowHigh.first)
    {
      ImPlot::PlotInfLines("##WindowInfLine1", &windowLowHigh.first, 1, infLineFlags);
    }

    if (windowLowHigh.second <= intensityAxisMax)
    {
      ImPlot::PlotInfLines("##WindowInfLine2", &windowLowHigh.second, 1, infLineFlags);
    }

    ImPlot::PopColormap();
    ImPlot::EndPlot();
  }

  ImGui::DragInt("Bin count", &(histoSettings.m_numBins), 1, 1, dataSize);

  float binWidth = intensityAxisRange / static_cast<float>(histoSettings.m_numBins);

  const float binWidthSpeed = isIntegerType(settings.componentType())
                                ? 1.0f
                                : (intensityAxisRange / 1000.0f);

  if (ImGui::DragFloat(
        "Bin width", &binWidth, binWidthSpeed, 0.0f, intensityAxisRange, imagePrecisionFormat.c_str()
      ))
  {
    if (binWidth > 0.0)
    {
      histoSettings.m_numBins = static_cast<int>(std::ceil(intensityAxisRange / binWidth));
    }
  }

  ImGui::Checkbox("Cumulative", &histoSettings.m_isCumulative);
  ImGui::SameLine();
  ImGui::Checkbox("Density", &histoSettings.m_isDensity);
  ImGui::SameLine();
  ImGui::Checkbox("Horizontal", &histoSettings.m_isHorizontal);
  ImGui::SameLine();
  ImGui::Checkbox("Log scale", &histoSettings.m_isLogScale);
  ImGui::Checkbox("Set intensity range", &histoSettings.m_useCustomIntensityRange);

  if (histoSettings.m_useCustomIntensityRange)
  {
    if (isFloatingType(settings.componentType()))
    {
      const float rangeMin = static_cast<float>(settings.componentStatistics().m_minimum);
      const float rangeMax = static_cast<float>(settings.componentStatistics().m_maximum);

      const std::string minValuesFormatString = std::string("Min: ") + imagePrecisionFormat;
      const std::string maxValuesFormatString = std::string("Max: ") + imagePrecisionFormat;

      float rangeLow = histoSettings.m_intensityRange[0];
      float rangeHigh = histoSettings.m_intensityRange[1];
      const float floatSpeed = (rangeHigh - rangeLow) / 1000.0f;

      if (ImGui::DragFloatRange2(
            "Range",
            &rangeLow,
            &rangeHigh,
            floatSpeed,
            rangeMin,
            rangeMax,
            minValuesFormatString.c_str(),
            maxValuesFormatString.c_str(),
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        histoSettings.m_intensityRange[0] = static_cast<double>(rangeLow);
        histoSettings.m_intensityRange[1] = static_cast<double>(rangeHigh);
      }
    }
    else
    {
      const int rangeMin = static_cast<int>(settings.componentStatistics().m_minimum);
      const int rangeMax = static_cast<int>(settings.componentStatistics().m_maximum);
      const float speed = 1.0f;

      int rangeLow = static_cast<int>(histoSettings.m_intensityRange[0]);
      int rangeHigh = static_cast<int>(histoSettings.m_intensityRange[1]);

      if (ImGui::DragIntRange2(
            "Intensity range",
            &rangeLow,
            &rangeHigh,
            speed,
            rangeMin,
            rangeMax,
            "Min: %d",
            "Max: %d",
            ImGuiSliderFlags_AlwaysClamp
          ))
      {
        histoSettings.m_intensityRange[0] = static_cast<double>(rangeLow);
        histoSettings.m_intensityRange[1] = static_cast<double>(rangeHigh);
      }
    }
  }
}
