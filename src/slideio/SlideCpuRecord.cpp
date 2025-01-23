#include "slideio/SlideCpuRecord.h"
#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/transform.hpp>

#include <array>
#include <sstream>

namespace slideio
{

SlideCpuRecord::SlideCpuRecord(SlideHeader header, SlideProperties props)
  : m_header(std::move(header))
  , m_properties(std::move(props))
  , m_transformation()
  , m_fileLevels()
  , m_createdLevels()
{
}

const SlideHeader& SlideCpuRecord::header() const
{
  return m_header;
}

SlideHeader& SlideCpuRecord::header()
{
  return m_header;
}

void SlideCpuRecord::setHeader(SlideHeader header)
{
  m_header = std::move(header);
}

const SlideProperties& SlideCpuRecord::properties() const
{
  return m_properties;
}

SlideProperties& SlideCpuRecord::properties()
{
  return m_properties;
}

void SlideCpuRecord::setProperties(SlideProperties props)
{
  m_properties = std::move(props);
}

const SlideTransformation& SlideCpuRecord::transformation() const
{
  return m_transformation;
}

SlideTransformation& SlideCpuRecord::transformation()
{
  return m_transformation;
}

void SlideCpuRecord::setTransformation(SlideTransformation tx)
{
  m_transformation = std::move(tx);
}

size_t SlideCpuRecord::numFileLevels() const
{
  return m_fileLevels.size();
}

size_t SlideCpuRecord::numCreatedLevels() const
{
  return m_createdLevels.size();
}

const SlideLevel& SlideCpuRecord::fileLevel(size_t i) const
{
  if (i < m_fileLevels.size())
  {
    return m_fileLevels.at(i);
  }

  std::ostringstream ss;
  ss << "Invalid slide file level " << i << " requested" << std::ends;
  throw_debug(ss.str());
}

const SlideLevel& SlideCpuRecord::createdLevel(size_t i) const
{
  if (i < m_createdLevels.size())
  {
    return m_createdLevels.at(i);
  }

  std::ostringstream ss;
  ss << "Invalid slide created level " << i << " requested" << std::ends;
  throw_debug(ss.str());
}

void SlideCpuRecord::addFileLevel(SlideLevel level)
{
  m_fileLevels.push_back(std::move(level));
}

void SlideCpuRecord::addCreatedLevel(SlideLevel level)
{
  m_createdLevels.push_back(std::move(level));
}

} // namespace slideio
