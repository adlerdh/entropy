#ifndef DYNAMIC_TRANSFORMATION_H
#define DYNAMIC_TRANSFORMATION_H

#include "rendering_old/drawables/Transformation.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include <functional>
#include <optional>

class DynamicTransformation : public Transformation, public ObjectCounter<DynamicTransformation>
{
public:
  DynamicTransformation(
    std::string name, GetterType<std::optional<glm::mat4> > thisToParentTxProvider
  );

  ~DynamicTransformation() override = default;

  /// Set function that provides the "parent_O_this" transformation for this drawable.
  void setMatrixProvider(GetterType<std::optional<glm::mat4> >);

private:
  void doUpdate(double /*time*/, const Viewport&, const camera::Camera&, const CoordinateFrame&)
    override;

  GetterType<std::optional<glm::mat4> > m_thisToParentTxProvider;
};

#endif // DYNAMIC_TRANSFORMATION_H
