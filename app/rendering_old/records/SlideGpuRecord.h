#ifndef SLIDE_GPU_RECORD_H
#define SLIDE_GPU_RECORD_H

#include <memory>

class GLTexture;

class SlideGpuRecord
{
public:
  explicit SlideGpuRecord(std::shared_ptr<GLTexture> texture);
  SlideGpuRecord() = delete;

  SlideGpuRecord(const SlideGpuRecord&) = default;
  SlideGpuRecord& operator=(const SlideGpuRecord&) = default;

  SlideGpuRecord(SlideGpuRecord&&) = default;
  SlideGpuRecord& operator=(SlideGpuRecord&&) = default;

  ~SlideGpuRecord() = default;

  // Return as non-const, since users need access to
  // non-const member functions of GLTexture
  std::weak_ptr<GLTexture> texture();

private:
  std::shared_ptr<GLTexture> m_texture;

  /// Level being rendered
  //    int m_activeLevel;
};

#endif // SLIDE_GPU_RECORD_H
