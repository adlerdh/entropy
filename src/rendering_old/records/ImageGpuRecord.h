#ifndef IMAGE_GPU_RECORD_H
#define IMAGE_GPU_RECORD_H

#include <memory>

class GLTexture;

class ImageGpuRecord
{
public:
  explicit ImageGpuRecord(std::shared_ptr<GLTexture> texture);
  ImageGpuRecord() = delete;

  ImageGpuRecord(const ImageGpuRecord&) = default;
  ImageGpuRecord& operator=(const ImageGpuRecord&) = default;

  ImageGpuRecord(ImageGpuRecord&&) = default;
  ImageGpuRecord& operator=(ImageGpuRecord&&) = default;

  ~ImageGpuRecord() = default;

  // Return as non-const, since users need access to
  // non-const member functions of GLTexture
  std::weak_ptr<GLTexture> texture();

private:
  std::shared_ptr<GLTexture> m_texture;
};

#endif // IMAGE_GPU_RECORD_H
