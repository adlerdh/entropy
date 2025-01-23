#include "logic/colormap/ParcellationLabelTable.h"

#include "common/HZeeException.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//#include "opencv2/highgui/highgui.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <utility>

namespace
{

std::vector<glm::vec3> generateRandomHSVSamples(
  size_t numSamples,
  const std::pair<float, float>& hueMinMax,
  const std::pair<float, float>& satMinMax,
  const std::pair<float, float>& valMinMax
)
{
  // Could randomly seed the random number generator, but let's use a fixed seed for now,
  // so that we always get the same color table.

  // Will be used to obtain a seed for the random number engine: std::random_device rd;
  // Standard mersenne_twister_engine seeded with rd(): std::mt19937 generator( rd() );

  static constexpr unsigned int sk_seed = 1234567890;
  std::default_random_engine generator(sk_seed);

  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  const float A = (satMinMax.second * satMinMax.second - satMinMax.first * satMinMax.first);

  const float B = satMinMax.first * satMinMax.first;

  const float C = ( valMinMax.second * valMinMax.second * valMinMax.second -
                      valMinMax.first * valMinMax.first * valMinMax.first );

  const float D = valMinMax.first * valMinMax.first * valMinMax.first;

  std::vector<glm::vec3> samples;
  samples.reserve(numSamples);

  for (size_t i = 0; i < numSamples; ++i)
  {
    float hue = (hueMinMax.second - hueMinMax.first) * dist(generator) + hueMinMax.first;
    float sat = std::sqrt(dist(generator) * A + B);
    float val = std::pow(dist(generator) * C + D, 1.0f / 3.0f);

    samples.push_back(glm::vec3{hue, sat, val});
  }

  return samples;
}

glm::vec3 convertHSVtoRGB(const glm::vec3& hsv)
{
  cv::Mat3f hsvMat(cv::Vec3f(hsv[0], hsv[1], hsv[2]));

  cv::Mat3f rgbMat;
  cv::cvtColor(hsvMat, rgbMat, cv::COLOR_HSV2RGB);

  cv::Vec3f rgbPixel(rgbMat.at<cv::Vec3f>(0, 0));
  return glm::vec3{rgbPixel[0], rgbPixel[1], rgbPixel[2]};
}

} // namespace

ParcellationLabelTable::ParcellationLabelTable(size_t labelCount)
  : m_colors_RGBA_F32()
  , m_properties()
{
  static const std::vector<float> sk_startAngles{0.0f, 120.0f, 240.0f, 60.0f, 180.0f, 300.0f};

  if (labelCount < 7)
  {
    throw_debug("Parcellation must have at least 7 labels");
  }

  std::vector<glm::vec3> rgbValues;

  // The first label (0) is always black:
  rgbValues.push_back(glm::vec3{0.0f, 0.0f, 0.0f});

  // Insert the six primary colors for labels 1-7:
  for (float s : sk_startAngles)
  {
    rgbValues.push_back(convertHSVtoRGB(glm::vec3{s, 1.0f, 1.0f}));
  }

  static const auto sk_hueMinMax = std::make_pair(0.0f, 360.0f);
  static const auto sk_satMinMax = std::make_pair(0.6f, 1.0f);
  static const auto sk_valMinMax = std::make_pair(0.7f, 1.0f);

  const std::vector<glm::vec3> hsvSamples
    = generateRandomHSVSamples(labelCount - 7, sk_hueMinMax, sk_satMinMax, sk_valMinMax);

  std::transform(
    std::begin(hsvSamples), std::end(hsvSamples), std::back_inserter(rgbValues), convertHSVtoRGB
  );

  m_colors_RGBA_F32.resize(labelCount);

  for (size_t i = 0; i < labelCount; ++i)
  {
    LabelProperties props;

    std::ostringstream ss;

    if (0 == i)
    {
      // Label index 0 is always used as the background label,
      // so it is fully transparent and not visible in 2D/3D views
      ss << "None";
      props.m_alpha = 0.0f;
      props.m_visible = false;
      props.m_showMesh = false;
    }
    else
    {
      ss << "Region " << i;
      props.m_alpha = 1.0f;
      props.m_visible = true;
      props.m_showMesh = false;
    }

    props.m_name = ss.str();
    props.m_color = rgbValues[i];

    m_properties.emplace_back(std::move(props));

    // Update the color in m_colors_RGBA_F32
    updateColorRGBA(i);
  }

  //    cv::Mat mat(100, 361, CV_32FC3, cv::Scalar(0.0f, 0.0f, 0.0f) );
  //    cv::imshow( "Display window", mat );
}

glm::vec4 ParcellationLabelTable::color_RGBA_premult_F32(size_t index) const
{
  checkLabelIndex(index);
  return m_colors_RGBA_F32.at(index);
}

size_t ParcellationLabelTable::numLabels() const
{
  return m_colors_RGBA_F32.size();
}

size_t ParcellationLabelTable::numColorBytes_RGBA_F32() const
{
  return m_colors_RGBA_F32.size() * sizeof(glm::vec4);
}

const float* ParcellationLabelTable::colorData_RGBA_premult_F32() const
{
  return glm::value_ptr(m_colors_RGBA_F32[0]);
}

tex::SizedInternalBufferTextureFormat ParcellationLabelTable::bufferTextureFormat_RGBA_F32()
{
  static const tex::SizedInternalBufferTextureFormat sk_format
    = tex::SizedInternalBufferTextureFormat::RGBA32F;

  return sk_format;
}

const std::string& ParcellationLabelTable::getName(size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_name;
}

void ParcellationLabelTable::setName(size_t index, std::string name)
{
  checkLabelIndex(index);
  m_properties[index].m_name = std::move(name);
}

bool ParcellationLabelTable::getVisible(size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_visible;
}

void ParcellationLabelTable::setVisible(size_t index, bool show)
{
  checkLabelIndex(index);
  m_properties[index].m_visible = show;
  updateColorRGBA(index);
}

bool ParcellationLabelTable::getShowMesh(size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_showMesh;
}

void ParcellationLabelTable::setShowMesh(size_t index, bool show)
{
  checkLabelIndex(index);
  m_properties[index].m_showMesh = show;
}

glm::vec3 ParcellationLabelTable::getColor(size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_color;
}

void ParcellationLabelTable::setColor(size_t index, const glm::vec3& color)
{
  checkLabelIndex(index);

  m_properties[index].m_color = color;
  updateColorRGBA(index);
}

float ParcellationLabelTable::getAlpha(size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_alpha;
}

void ParcellationLabelTable::setAlpha(size_t index, float alpha)
{
  checkLabelIndex(index);

  if (alpha < 0.0f || 1.0f < alpha)
  {
    return;
  }

  m_properties[index].m_alpha = alpha;
  updateColorRGBA(index);
}

void ParcellationLabelTable::updateColorRGBA(size_t index)
{
  checkLabelIndex(index);

  // Modulate opacity by visibility:
  const float a = getAlpha(index) * (getVisible(index) ? 1.0f : 0.0f);

  // Pre-multiplied RGBA:
  m_colors_RGBA_F32[index] = a * glm::vec4{getColor(index), 1.0f};
}

void ParcellationLabelTable::checkLabelIndex(size_t index) const
{
  if (index >= m_properties.size())
  {
    std::ostringstream ss;
    ss << "Invalid label index " << index << std::ends;
    throw_debug(ss.str());
  }
}
