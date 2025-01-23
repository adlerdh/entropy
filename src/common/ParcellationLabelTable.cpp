#include "common/ParcellationLabelTable.h"
#include "common/Exception.hpp"
#include "common/MathFuncs.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>
#include <utility>

namespace
{

static constexpr std::size_t sk_seed = 1234567890;

static const auto sk_hueMinMax = std::make_pair(0.0f, 360.0f);
static const auto sk_satMinMax = std::make_pair(0.5f, 1.0f);
static const auto sk_valMinMax = std::make_pair(0.5f, 1.0f);

} // namespace

ParcellationLabelTable::ParcellationLabelTable(std::size_t labelCount, std::size_t maxLabelCount)
  : m_colors_RGBA_U8()
  , m_properties()
  , m_maxLabelCount(std::min(maxLabelCount, labelCountUpperBound()))
{
  static const std::vector<float> sk_startAngles{
    0.0f, 120.0f, 240.0f, 60.0f, 180.0f, 300.0f, 30.0f, 150.0f, 270.0f, 90.0f, 210.0f, 330.0f
  };

  std::size_t labelCountAdjusted = labelCount;

  if (labelCountAdjusted < 12)
  {
    spdlog::warn("Parcellation label table must have at least 12 labels");
    labelCountAdjusted = 12;
  }

  if (labelCountAdjusted > m_maxLabelCount)
  {
    spdlog::warn(
      "Parcellation label count ({}) exceeds maximum ({})", labelCountAdjusted, m_maxLabelCount
    );
    labelCountAdjusted = m_maxLabelCount;
  }

  std::vector<glm::vec3> rgbValues;

  // The first label (0) is always black:
  rgbValues.push_back(glm::vec3{0.0f, 0.0f, 0.0f});

  // Insert the twelve "primary colors" for labels 1-12:
  for (float s : sk_startAngles)
  {
    rgbValues.push_back(glm::rgbColor(glm::vec3{s, 1.0f, 1.0f}));
  }

  const std::vector<glm::vec3> hsvSamples = math::generateRandomHsvSamples(
    labelCountAdjusted - 12, sk_hueMinMax, sk_satMinMax, sk_valMinMax, sk_seed
  );

  std::
    transform(std::begin(hsvSamples), std::end(hsvSamples), std::back_inserter(rgbValues), glm::rgbColor<float, glm::precision::defaultp>);

  m_colors_RGBA_U8.resize(labelCountAdjusted);

  for (std::size_t i = 0; i < labelCountAdjusted; ++i)
  {
    LabelProperties props;

    std::ostringstream ss;

    if (0 == i)
    {
      // Label index 0 is always used as the background label,
      // so it is fully transparent and not visible in 2D/3D views
      ss << "Background";
      props.m_alpha = 0u;
      props.m_visible = false;
      props.m_showMesh = false;
    }
    else
    {
      ss << "Region " << i;
      props.m_alpha = 255u;
      props.m_visible = true;
      props.m_showMesh = false;
    }

    props.m_name = ss.str();
    props.m_color = glm::u8vec3{255.0f * rgbValues[i]};

    m_properties.emplace_back(std::move(props));

    // Update the color
    updateVector(i);
  }
}

glm::u8vec4 ParcellationLabelTable::color_RGBA_nonpremult_U8(std::size_t index) const
{
  checkLabelIndex(index);
  return m_colors_RGBA_U8.at(index);
}

std::size_t ParcellationLabelTable::numLabels() const
{
  return m_colors_RGBA_U8.size();
}

std::size_t ParcellationLabelTable::maxNumLabels() const
{
  return m_maxLabelCount;
}

std::size_t ParcellationLabelTable::numColorBytes_RGBA_U8() const
{
  return m_colors_RGBA_U8.size() * sizeof(glm::u8vec4);
}

const uint8_t* ParcellationLabelTable::colorData_RGBA_nonpremult_U8() const
{
  return glm::value_ptr(m_colors_RGBA_U8[0]);
}

tex::SizedInternalBufferTextureFormat ParcellationLabelTable::bufferTextureFormat_RGBA_U8()
{
  static const tex::SizedInternalBufferTextureFormat sk_format
    = tex::SizedInternalBufferTextureFormat::RGBA8_UNorm;

  return sk_format;
}

std::size_t ParcellationLabelTable::numBytesPerLabel_U8()
{
  return 4; // 1 byte per component
}

std::size_t ParcellationLabelTable::labelCountUpperBound()
{
  return static_cast<std::size_t>(1U << 16);
}

const std::string& ParcellationLabelTable::getName(std::size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_name;
}

void ParcellationLabelTable::setName(std::size_t index, std::string name)
{
  checkLabelIndex(index);
  m_properties[index].m_name = std::move(name);
}

bool ParcellationLabelTable::getVisible(std::size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_visible;
}

void ParcellationLabelTable::setVisible(std::size_t index, bool show)
{
  checkLabelIndex(index);
  m_properties[index].m_visible = show;
  updateVector(index);
}

bool ParcellationLabelTable::getShowMesh(std::size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_showMesh;
}

void ParcellationLabelTable::setShowMesh(std::size_t index, bool show)
{
  checkLabelIndex(index);
  m_properties[index].m_showMesh = show;
}

glm::u8vec3 ParcellationLabelTable::getColor(std::size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_color;
}

void ParcellationLabelTable::setColor(std::size_t index, const glm::u8vec3& color)
{
  checkLabelIndex(index);
  m_properties[index].m_color = color;
  updateVector(index);
}

uint8_t ParcellationLabelTable::getAlpha(std::size_t index) const
{
  checkLabelIndex(index);
  return m_properties.at(index).m_alpha;
}

void ParcellationLabelTable::setAlpha(std::size_t index, uint8_t alpha)
{
  checkLabelIndex(index);
  m_properties[index].m_alpha = alpha;
  updateVector(index);
}

std::vector<std::size_t> ParcellationLabelTable::addLabels(std::size_t count)
{
  std::vector<std::size_t> newIndices;

  if (0 == count)
  {
    return newIndices; // None to add
  }

  if (numLabels() + count > maxNumLabels())
  {
    spdlog::error(
      "Unable to add {} new labels: exceeds maximum number of labels allowed ({}) "
      "for this parcellation label table",
      count,
      maxNumLabels()
    );

    return newIndices;
  }

  // Generate random colors
  const std::size_t last = m_colors_RGBA_U8.size();
  const std::size_t seed = sk_seed + last;

  const std::vector<glm::vec3> hsvSamples
    = math::generateRandomHsvSamples(count, sk_hueMinMax, sk_satMinMax, sk_valMinMax, seed);

  std::vector<glm::vec3> rgbValues;

  std::
    transform(std::begin(hsvSamples), std::end(hsvSamples), std::back_inserter(rgbValues), glm::rgbColor<float, glm::precision::defaultp>);

  m_colors_RGBA_U8.resize(last + count);

  for (std::size_t i = last; i < last + count; ++i)
  {
    newIndices.push_back(i);

    LabelProperties props;

    std::ostringstream ss;
    ss << "Region " << i;

    props.m_alpha = 255;
    props.m_visible = true;
    props.m_showMesh = false;
    props.m_name = ss.str();
    props.m_color = rgbValues[i - last];

    m_properties.emplace_back(std::move(props));

    updateVector(i);
  }

  return newIndices;
}

void ParcellationLabelTable::updateVector(std::size_t index)
{
  checkLabelIndex(index);

  // Modulate opacity by visibility:
  const uint8_t a = getAlpha(index) * (getVisible(index) ? 1 : 0);
  m_colors_RGBA_U8[index] = glm::u8vec4{getColor(index), a};
}

void ParcellationLabelTable::checkLabelIndex(std::size_t index) const
{
  if (index >= m_properties.size())
  {
    std::ostringstream ss;
    ss << "Invalid label index " << index << std::ends;
    throw_debug(ss.str())
  }
}
