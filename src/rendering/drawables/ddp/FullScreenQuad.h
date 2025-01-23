#ifndef FULL_SCREEN_QUAD_H
#define FULL_SCREEN_QUAD_H

#include "rendering/drawables/DrawableBase.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include "common/ObjectCounter.hpp"

#include <memory>

class MeshGpuRecord;

class FullScreenQuad : public DrawableBase, public ObjectCounter<FullScreenQuad>
{
public:
  FullScreenQuad(const std::string& name);

  FullScreenQuad(const FullScreenQuad&) = delete;
  FullScreenQuad& operator=(const FullScreenQuad&) = delete;

  FullScreenQuad(FullScreenQuad&&) = default;
  FullScreenQuad& operator=(FullScreenQuad&&) = default;

  ~FullScreenQuad() override;

protected:
  bool drawVao();

private:
  bool initBuffer();
  bool initVao();

  GLVertexArrayObject m_vao;
  std::unique_ptr<GLVertexArrayObject::IndexedDrawParams> m_vaoParams;
  std::unique_ptr<MeshGpuRecord> m_meshGpuRecord;
};

#endif // FULL_SCREEN_QUAD_H
