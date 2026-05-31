#ifndef TRANSFORMATION_H
#define TRANSFORMATION_H

#include "rendering/drawables/DrawableBase.h"

#include "common/ObjectCounter.hpp"

class Transformation : public DrawableBase, public ObjectCounter<Transformation>
{
public:
  Transformation(std::string name, const glm::mat4& parent_O_this = glm::mat4{1.0f});

  ~Transformation() override = default;

  void setMatrix(glm::mat4 parent_O_this);

  const glm::mat4& matrix() const;

  /// @todo implement update() and pass down my matrix to children.
  /// Remove parent_O_this from all other nodes!
};

#endif // TRANSFORMATION_H
