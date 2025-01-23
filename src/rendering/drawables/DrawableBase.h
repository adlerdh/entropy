#ifndef DRAWABLE_BASE_H
#define DRAWABLE_BASE_H

#include "rendering/common/DrawableOpacity.h"
#include "rendering/interfaces/IDrawable.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLErrorChecker.h"

#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>

#include <uuid.h>

#include <list>
#include <memory>
#include <string>

/**
 * @brief Base class for all drawable objects.
 */
class DrawableBase : public IDrawable
{
public:
  DrawableBase(const std::string& name, const DrawableType& type);

  DrawableBase(const DrawableBase&) = default;
  DrawableBase& operator=(const DrawableBase&) = default;

  DrawableBase(DrawableBase&&) = default;
  DrawableBase& operator=(DrawableBase&&) = default;

  ~DrawableBase() override = default;

  void render(const RenderStage& stage, const ObjectsToRender& objects) override;

  void update(
    double time,
    const Viewport& viewport,
    const camera::Camera& camera,
    const CoordinateFrame& crosshairs,
    const AccumulatedRenderingData& parentData
  ) override;

  void printTree(int depth) const override;

  /// @todo This is going to be removed
  virtual bool isOpaque() const;

  /// @todo This will be the new way of reporting opacity from drawables
  virtual DrawableOpacity opacityFlag() const;

  /// Get this drawable's accumulated rendering data
  const AccumulatedRenderingData& getAccumulatedRenderingData() const;

  /// Add a new child to this drawable in sequence behind the last child.
  /// @return True iff the child was added.
  bool addChild(std::weak_ptr<DrawableBase> child);

  /// Remove an existing child from this drawable.
  /// @return True iff the child was removed.
  bool removeChild(const DrawableBase& child);

  /// Get the unique identifier of this drawable.
  const uuids::uuid& uid() const;

  /// Set the master opacity multiplier of this drawable.
  void setMasterOpacityMultiplier(float multiplier);

  /// Get the master opacity multiplier of this drawable.
  float masterOpacityMultiplier() const;

  /// Set whether the user can point pick on the drawable.
  void setPickable(bool pickable);

  /// Get the pickable setting.
  bool isPickable() const;

  /// Set wheter this drawable is visible (i.e. rendered).
  /// Applies property to only this drawable, not to its descendants.
  void setVisible(bool visible);

  bool isVisible() const;

  /// Get whether the drawable is enabled (i.e. rendered and updates).
  /// Applies property to only this drawable, not to its descendants.
  void setEnabled(bool set);

  bool isEnabled() const;

  /// Matrix transformation from this Drawable to its parent Drawable. It is safe to call this
  /// function from within doUpdate().
  /// @note This call does NOT recursively update transformations of all of the Drawable's
  /// children and descendants. To do so, it is necessary to call update() at the top level.
  void set_parent_T_this(glm::mat4 parent_T_this);

  const glm::mat4& parent_T_this() const;

  uint32_t getRenderId() const;

  /// @todo Should add comment that user is responsible for binding these textures prior to render

  /// To be used with DDPStage::Initialize
  static const Uniforms::SamplerIndexType OpaqueDepthTexSamplerIndex;

  /// To be used with DDPStage::Peel
  static const Uniforms::SamplerIndexType DepthBlenderTexSamplerIndex;
  static const Uniforms::SamplerIndexType FrontBlenderTexSamplerIndex;

protected:
  virtual void doRender(const RenderStage&) {}
  virtual void doSetupState() {}
  virtual void doTeardownState() {}

  virtual void doUpdate(
    double /*time*/,
    const Viewport& /*viewport*/,
    const camera::Camera& /*camera*/,
    const CoordinateFrame& /*crosshairs*/
  )
  {
  }

  void setRenderId(uint32_t id);

  GLErrorChecker m_errorChecker;

  /// Unique ID of this object
  uuids::uuid m_uid;

  /// Displayable name of this object
  std::string m_name;

  /// Type of this object
  DrawableType m_type;

  /// Render ID of this object
  uint32_t m_renderId;

private:
  /// Update this object's rendering data by accumulating its data with its parent's data
  void updateRenderingData();

  /// List of weak pointers to this object's child drawables
  std::list<std::weak_ptr<DrawableBase> > m_children;

  /// Accumulated data saved off from this object's parent
  AccumulatedRenderingData m_parentRenderingData;

  /// Accumulated data for this object, which will be propagated to its children
  AccumulatedRenderingData m_myRenderingData;

  /// Affine transformation from this object to its parent
  glm::mat4 m_parent_T_this;

  /// Master multiplier across all color layers for this drawable
  float m_masterOpacityMultiplier;

  /// Flag for whether this drawable is pickable
  bool m_pickable;

  /// Flag that enables/disables rendering and updating of this drawable and all of its children
  bool m_enabled;

  /// Flag that shows/hides this drawable
  bool m_visible;
};

#endif // DRAWABLE_BASE_H
