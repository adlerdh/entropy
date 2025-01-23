#pragma once

#include <array>

/**
 * @brief Method used to compute number of histogram bins in a scalar (single component) image
 * @see https://numpy.org/doc/stable/reference/generated/numpy.histogram_bin_edges.html
 * @see https://en.wikipedia.org/wiki/Histogram#Number_of_bins_and_width
 */
enum class NumBinsComputationMethod
{
  SquareRoot,
  Sturges,
  Rice,
  Scott,
  FreedmanDiaconis
};

/**
 * @brief Settings used for computing and displaying the histogram of a single image component
 */
struct HistogramSettings
{
  /// Method used for compute initial value for number of histogram bins
  NumBinsComputationMethod m_numBinsMethod = NumBinsComputationMethod::FreedmanDiaconis;
  int m_numBins;     //!< Number of bins in histogram
  double m_binWidth; //!< Bin width

  bool m_isCumulative
    = false; //!< Whether the histogram is cumulative (\c ImPlotHistogramFlags_Cumulative)
  bool m_isDensity
    = false; //!< Whehter the histogram shows probability densities (true) or counts (false) (\c ImPlotHistogramFlags_Density)
  bool m_isHorizontal
    = false; //!< Whether the histogram is horizontal, i.e. bins go left to right (\c ImPlotHistogramFlags_Horizontal)
  bool m_isLogScale = false; //!< Apply log (base 10) to histogram bin counts or probabilities

  /// Intensity range (inclusive) over which to compute histogram.
  /// If not defined, then the image component's [min, max] range will be used
  std::array<double, 2> m_intensityRange{0.0, 0.0};
  bool m_useCustomIntensityRange = false; //!< Whether to use the custom intensity range
};
