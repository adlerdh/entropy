#include "rendering/drawables/DrawableBase.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>

const Uniforms::SamplerIndexType DrawableBase::OpaqueDepthTexSamplerIndex{0};

const Uniforms::SamplerIndexType DrawableBase::DepthBlenderTexSamplerIndex{0};
const Uniforms::SamplerIndexType DrawableBase::FrontBlenderTexSamplerIndex{1};

DrawableBase::DrawableBase(const std::string& name, const DrawableType& type)
  : m_uid()
  , m_name(std::move(name))
  , m_type(type)
  , m_renderId(0)
  ,

  m_children()
  ,

  m_parentRenderingData()
  , m_myRenderingData()
  ,

  m_parent_T_this(1.0f)
  , m_masterOpacityMultiplier(1.0f)
  , m_pickable(false)
  , m_enabled(true)
  , m_visible(true)

{
  updateRenderingData();
}

bool DrawableBase::addChild(std::weak_ptr<DrawableBase> child)
{
  auto c = child.lock();

  if (!c)
  {
    return false;
  }

  auto pred = [&c](std::weak_ptr<DrawableBase> other)
  {
    if (auto o = other.lock())
    {
      return (o->uid() == c->uid());
    }
    else
    {
      return false;
    }
  };

  // Add the drawable if it is not yet a child
  auto it = std::find_if(std::begin(m_children), std::end(m_children), pred);

  if (std::end(m_children) == it)
  {
    m_children.push_back(child);
    return true;
  }

  return false;
}

bool DrawableBase::removeChild(const DrawableBase& child)
{
  auto pred = [&child](std::weak_ptr<DrawableBase> other)
  {
    if (auto o = other.lock())
    {
      return (o->uid() == child.uid());
    }
    else
    {
      return false;
    }
  };

  auto it = std::find_if(std::begin(m_children), std::end(m_children), pred);

  if (std::end(m_children) != it)
  {
    m_children.erase(it);
    return true;
  }

  return false;
}

void DrawableBase::render(const RenderStage& stage, const ObjectsToRender& objectsToRender)
{
  if (!isEnabled() || !isVisible())
  {
    // Do not render Drawable or its children
    return;
  }

  // If this node is opaque, then all children must be opaque

  doSetupState();

  switch (objectsToRender)
  {
  case ObjectsToRender::Opaque:
  {
    if (isOpaque())
    {
      doRender(stage);
    }
    break;
  }
  case ObjectsToRender::Translucent:
  {
    if (!isOpaque())
    {
      doRender(stage);
    }
    break;
  }
  case ObjectsToRender::Pickable:
  {
    if (isPickable())
    {
      doRender(stage);
    }
    break;
  }
  case ObjectsToRender::All:
  {
    doRender(stage);
    break;
  }
  }

  doTeardownState();

  // Render children
  for (auto& child : m_children)
  {
    if (auto c = child.lock())
    {
      c->render(stage, objectsToRender);
    }
  }
}

void DrawableBase::update(
  double time,
  const Viewport& viewport,
  const camera::Camera& camera,
  const CoordinateFrame& crosshairs,
  const AccumulatedRenderingData& parentData
)
{
  if (!isEnabled())
  {
    return;
  }

  // Save off parent data that may be used in doUpdate()
  m_parentRenderingData = parentData;
  updateRenderingData();

  // Update this drawable
  doUpdate(time, viewport, camera, crosshairs);

  for (auto& child : m_children)
  {
    if (auto c = child.lock())
    {
      c->update(time, viewport, camera, crosshairs, getAccumulatedRenderingData());
    }
  }
}

const AccumulatedRenderingData& DrawableBase::getAccumulatedRenderingData() const
{
  return m_myRenderingData;
}

void DrawableBase::setMasterOpacityMultiplier(float multiplier)
{
  if (multiplier < 0.0f || 1.0f < multiplier)
  {
    return;
  }

  m_masterOpacityMultiplier = multiplier;
  updateRenderingData();
}

void DrawableBase::setPickable(bool pickable)
{
  m_pickable = pickable;
  updateRenderingData();
}

bool DrawableBase::isPickable() const
{
  return getAccumulatedRenderingData().m_pickable;
}

void DrawableBase::setVisible(bool visible)
{
  m_visible = visible;
}

bool DrawableBase::isVisible() const
{
  return m_visible;
}

void DrawableBase::setEnabled(bool enabled)
{
  m_enabled = enabled;
}

bool DrawableBase::isEnabled() const
{
  return m_enabled;
}

float DrawableBase::masterOpacityMultiplier() const
{
  return m_masterOpacityMultiplier;
}

bool DrawableBase::isOpaque() const
{
  // Default implementation of this function returns false (i.e. non-opaque)
  // iff the accumulated master opacity multiplier is less than 1.
  if (getAccumulatedRenderingData().m_masterOpacityMultiplier < 1.0f)
  {
    return false;
  }

  return true;
}

DrawableOpacity DrawableBase::opacityFlag() const
{
  return DrawableOpacity{OpacityFlag::Unknown, OpacityFlag::Unknown};
}

void DrawableBase::set_parent_T_this(glm::mat4 parent_T_this)
{
  m_parent_T_this = std::move(parent_T_this);
  updateRenderingData();
}

const glm::mat4& DrawableBase::parent_T_this() const
{
  return m_parent_T_this;
}

void DrawableBase::printTree(int depth) const
{
  std::string tabs;
  for (int i = 0; i < depth; ++i)
  {
    tabs += "\t";
  }

  spdlog::info("{}{}", tabs, m_name);

  for (auto& child : m_children)
  {
    if (auto c = child.lock())
    {
      c->printTree(depth + 1);
    }
  }
}

const uuids::uuid& DrawableBase::uid() const
{
  return m_uid;
}

void DrawableBase::setRenderId(uint32_t id)
{
  m_renderId = id;
}

uint32_t DrawableBase::getRenderId() const
{
  return m_renderId;
}

void DrawableBase::updateRenderingData()
{
  // Chain the transformations from this object to its parent to the World:
  m_myRenderingData.m_world_T_object = m_parentRenderingData.m_world_T_object * m_parent_T_this;

  // Multiply the opacity factor of this object with its parent's opacity factor:
  m_myRenderingData.m_masterOpacityMultiplier = m_parentRenderingData.m_masterOpacityMultiplier
                                                * m_masterOpacityMultiplier;

  // AND together the pickable flags of this object and its parent:
  m_myRenderingData.m_pickable = (m_parentRenderingData.m_pickable & m_pickable);
}
