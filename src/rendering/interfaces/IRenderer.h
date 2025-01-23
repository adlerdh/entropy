#ifndef I_RENDERER_H
#define I_RENDERER_H

#include "common/CoordinateFrame.h"
#include "common/Viewport.h"
#include "logic/camera/Camera.h"

#include <glm/fwd.hpp>

#include <memory>
#include <utility>

/**
 * @brief Interface for a class that renders IDrawable objects.
 *
 * @note render() and update() may be executed in the GUI's rendering thread
 */
class IRenderer
{
public:
  virtual ~IRenderer() = default;

  virtual void initialize() = 0;
  virtual void render() = 0;
  virtual void teardown() = 0;

  /**
     * @brief Resize the viewport of the render target
     * @param[in] viewport Target viewport
     */
  virtual void resize(const Viewport& viewport) = 0;

  /**
     * @brief Update the rendering of the scene
     * @param[in] camera Scene view camera
     * @param[in] crosshairs Coordinate frame of scene crosshairs
     */
  virtual void update(const camera::Camera& camera, const CoordinateFrame& crosshairs) = 0;

  /**
     * @brief Set the renderer's ability to point pick object IDs and depths in the scene.
     */
  virtual void setEnablePointPicking(bool enable) = 0;

  /**
     * @brief Get ID and depth of point picked in rendered scene
     * @param[in] ndcPos 2D NDC coordinates of point
     * @return ID and NDC z-depth of drawable object picked (ID 0 means no object picked)
     */
  virtual std::pair<uint16_t, float> pickObjectIdAndNdcDepth(const glm::vec2& ndcPos) = 0;
};

#endif // I_RENDERER_H
