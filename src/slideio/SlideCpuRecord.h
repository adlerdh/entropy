#ifndef SLIDE_CPU_RECORD_H
#define SLIDE_CPU_RECORD_H

#include "slideio/SlideHeader.h"
#include "slideio/SlideLevel.h"
#include "slideio/SlideProperties.h"
#include "slideio/SlideTransformation.h"

#include <vector>

namespace slideio
{

class SlideCpuRecord
{
public:
  explicit SlideCpuRecord(SlideHeader, SlideProperties);

  SlideCpuRecord(const SlideCpuRecord&) = delete;
  SlideCpuRecord& operator=(const SlideCpuRecord&) = delete;

  SlideCpuRecord(SlideCpuRecord&&) = default;
  SlideCpuRecord& operator=(SlideCpuRecord&&) = default;

  ~SlideCpuRecord() = default;

  const SlideHeader& header() const;
  SlideHeader& header();
  void setHeader(SlideHeader);

  const SlideProperties& properties() const;
  SlideProperties& properties();
  void setProperties(SlideProperties);

  const SlideTransformation& transformation() const;
  SlideTransformation& transformation();
  void setTransformation(SlideTransformation);

  size_t numFileLevels() const;
  size_t numCreatedLevels() const;

  const SlideLevel& fileLevel(size_t i) const;
  const SlideLevel& createdLevel(size_t i) const;

  void addFileLevel(SlideLevel);
  void addCreatedLevel(SlideLevel);

private:
  SlideHeader m_header;
  SlideProperties m_properties;
  SlideTransformation m_transformation;

  /// Levels present in the file
  /// @note Levels are arranged from largest to smallest
  std::vector<SlideLevel> m_fileLevels;

  /// Levels created by this program
  /// @note Levels are arranged from largest to smallest
  std::vector<SlideLevel> m_createdLevels;
};

} // namespace slideio

#endif // SLIDE_CPU_RECORD_H
