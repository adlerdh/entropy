#include "rendering_old/drawables/DynamicTransformation.h"

namespace
{
static const glm::mat4 sk_ident{1.0f};
}

DynamicTransformation::DynamicTransformation(
  std::string name, GetterType<std::optional<glm::mat4> > thisToParentTxProvider
)
  : Transformation(std::move(name), sk_ident)
  , m_thisToParentTxProvider(thisToParentTxProvider)
{
}

void DynamicTransformation::setMatrixProvider(GetterType<std::optional<glm::mat4> > provider)
{
  m_thisToParentTxProvider = provider;
}

void DynamicTransformation::
  doUpdate(double /*time*/, const Viewport&, const camera::Camera&, const CoordinateFrame&)
{
  if (!m_thisToParentTxProvider)
  {
    setVisible(false);
    return;
  }

  const auto parent_O_this = m_thisToParentTxProvider();
  if (!parent_O_this)
  {
    setVisible(false);
    return;
  }

  setVisible(true);
  set_parent_T_this(*parent_O_this);
}
