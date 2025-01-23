#ifndef VIEWPORT_H
#define VIEWPORT_H

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

/**
 * @brief Viewport class that follows the OpenGL convension:
 * - Viewport dimensions are measured in device-independent pixel units.
 * - Pixel coordinate (0, 0) is the bottom-left corner of the viewport.
 * - The left-to-right and bottom-to-top directions are both positive.
 *
 * @note If not otherwise specified in this class, the values for left,
 * bottom, width, and height are all measured in device-independent pixel units.
 *
 * @note Some functions explicitly specify left, bottom, width, and height
 * in device-independent pixel units.
 *
 * @see The Qt documentation provides a good explanation of how the devicePixelRatio
 * is used to support high-resolution displays. From those docs:
 *
 * Geometry in Qt5 is specified in device-independent pixels.
 * This includes widget and item geometry, event geometry, desktop, window,
 * and screen geometry, and animation velocities. Rendered output is in
 * device pixels, which corresponds to the display resolution.
 *
 * devicePixelRatio is the ratio between the device-independent pixels
 * (used by the application, subject to scaling by the operating system)
 * and device pixel coordinates (pixels of the display device).
 */
class Viewport
{
public:
  /// Construct 1x1 viewport with bottom-left at (0, 0)
  Viewport();

  /// Construct viewport with given bottom-left coordiates and dimensions
  /// @param left Left coordinate in device-independent pixel units
  /// @param bottom Bottom coordinate in device-independent pixel units
  /// @param width Width in device-independent pixel units
  /// @param height Height coordinate in device-independent pixel units
  Viewport(float left, float bottom, float width, float height);
  Viewport(glm::vec4 viewportAsVec4);

  Viewport(const Viewport&) = default;
  Viewport& operator=(const Viewport&) = default;

  Viewport(Viewport&&) = default;
  Viewport& operator=(Viewport&&) = default;

  ~Viewport() = default;

  /// Set the left coordinate in device-independent pixel units
  void setLeft(float left);

  /// Set the bottom coordinate in device-independent pixel units
  void setBottom(float bottom);

  /// Set the width in device-independent pixel units
  void setWidth(float width);

  /// Set the height in device-independent pixel units
  void setHeight(float height);

  /// Get the left coordinate in device-independent pixel units
  float left() const;

  /// Get the bottom coordinate in device-independent pixel units
  float bottom() const;

  /// Get the width in device-independent pixel units
  float width() const;

  /// Get the height in device-independent pixel units
  float height() const;

  /// Get the viewport area in device-independent pixel units
  float area() const;

  /// Set the viewport from a vec4 in device-independent pixel units: { left, bottom, width, height }
  void setAsVec4(const glm::vec4& viewport);

  /// Get the viewport as a vec4 in device-independent pixel units: {left, bottom, width, height }
  glm::vec4 getAsVec4() const;

  /// Get the viewport as a vec4 in device pixel units: {left, bottom, width, height }
  glm::vec4 getDeviceAsVec4() const;

  /// Get the left coordinate in device pixel units
  float deviceLeft() const;

  /// Get the bottom coordinate in device pixel units
  float deviceBottom() const;

  /// Get the width in device pixel units
  float deviceWidth() const;

  /// Get the height in device pixel units
  float deviceHeight() const;

  /// Get the area in device pixel units
  float deviceArea() const;

  /// Get the viewport device-independent pixel aspect ratio: width / height
  float aspectRatio() const;

  /// Get the viewport device aspect ratio: width / height
  float deviceAspectRatio() const;

  /// Set the number of display device pixels per logical pixel
  void setDevicePixelRatio(glm::vec2 ratio);

  /// Get the number of device pixels per logical pixel
  glm::vec2 devicePixelRatio() const;

private:
  /// @note Left, bottom, width, and height are stored in device-independent pixel units

  float m_left;
  float m_bottom;
  float m_width;
  float m_height;

  /// Number of display device pixels per logical pixel
  glm::vec2 m_devicePixelRatio;
};

#endif // VIEWPORT_H
