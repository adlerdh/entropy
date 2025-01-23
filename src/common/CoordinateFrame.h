#ifndef COORDINATE_FRAME_H
#define COORDINATE_FRAME_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/**
 * @brief A 3D coordinate frame. The frame is defined by an origin in World space and a
 * rotation relative into World space. Functions are provided that transform World space
 * coordinates into the frame's coordinates, and vice-versa. The rotation is represented
 * internally as a quaternion.
 */
class CoordinateFrame
{
public:
  /**
     * @brief Construct the frame with an identity transformation
     * (i.e. zero origin and identity rotation).
     */
  explicit CoordinateFrame();

  /**
     * @brief Construct the frame with given origin in World space and rotation from Frame to
     * World space.
     *
     * @param[in] worldOrigin Frame origin position in World space
     * @param[in] world_T_frame_rotation Quaternion rotation from Frame to World space
     */
  CoordinateFrame(glm::vec3 worldOrigin, glm::quat world_T_frame_rotation);

  /**
     * @brief Construct the frame with given origin in World space and rotation from Frame to
     * World space. The rotation is defined by an angle-axis pair.
     *
     * @param[in] worldOrigin Origin position in World space
     * @param[in] angleDegrees Angle of rotation defined in degrees, counter-clockwise about the axis
     * @param[in] axis Axis of rotation in World space
     */
  CoordinateFrame(glm::vec3 worldOrigin, float angleDegrees, const glm::vec3& worldAxis);

  /**
     * @brief Construct the frame with given origin in World sapce and rotation from Frame to
     * World space. The rotation is defined by two pairs of matching axes in World and Frame spaces.
     *
     * @param[in] worldOrigin Origin position in World space
     * @param[in] frameAxis1 First Frame-space axis
     * @param[in] worldAxis1 First World-space axis
     * @param[in] frameAxis2 Second Frame-space axis
     * @param[in] worldAxis2 Second World-space axis
     *
     * @note The angles between the input Frame-space and World-space axes must be equal.
     */
  CoordinateFrame(
    glm::vec3 worldOrigin,
    const glm::vec3& frameAxis1,
    const glm::vec3& worldAxis1,
    const glm::vec3& frameAxis2,
    const glm::vec3& worldAxis2
  );

  CoordinateFrame(const CoordinateFrame&) = default;
  CoordinateFrame& operator=(const CoordinateFrame&) = default;

  CoordinateFrame(CoordinateFrame&&) = default;
  CoordinateFrame& operator=(CoordinateFrame&&) = default;

  ~CoordinateFrame() = default;

  /// Get the transformation from World to Frame space as a 4x4 rigid-body matrix
  glm::mat4 frame_T_world() const;

  /// Get the transformation from Frame to World space as a 4x4 rigid-body matrix
  glm::mat4 world_T_frame() const;

  /// Get the frame's World-space origin position
  glm::vec3 worldOrigin() const;

  /// Get the frame's rotation into World space as a quaternion
  glm::quat world_T_frame_rotation() const;

  /**
     * @brief Set the frame's origin in World space.
     *
     * @param[in] worldOrigin World-space origin position
     */
  void setWorldOrigin(glm::vec3 worldOrigin);

  /**
     * @brief Set the frame's rotation relative to World space.
     *
     * @param[in] world_T_frame_rotation Rotation from frame to World (defined by quaternion)
     */
  void setFrameToWorldRotation(glm::quat world_T_frame_rotation);

  /**
     * @brief Set the frame's rotation relative relative to World space.
     *
     * @param[in] angleDegrees Rotation defined by an angle-axis pair
     * @param[in] worldAxis World-space axis of rotation
     */
  void setFrameToWorldRotation(float angleDegrees, const glm::vec3& worldAxis);

  /**
     * @brief Set the frame's rotation into World space.
     * The rotation is defined by two pairs of matching axes in World and Frame space.
     *
     * @param[in] worldOrigin World-space origin position
     * @param[in] frameAxis1 First Frame-space axis
     * @param[in] worldAxis1 First World-space axis
     * @param[in] frameAxis2 Second Frame-space axis
     * @param[in] worldAxis2 Second World-space axis
     * @param[in] requireEqualAngles Flag that requires the angle between input frame and
     *            world axes to be equal
     */
  void setFrameToWorldRotation(
    const glm::vec3& frameAxis1,
    const glm::vec3& worldAxis1,
    const glm::vec3& frameAxis2,
    const glm::vec3& worldAxis2,
    bool requireEqualAngles
  );

  /**
     * @brief Set the frame transformation to identity.
     */
  void setIdentity();

  /**
     * @brief operator+= for composing this frame (lhs) with another frame (rhs).
     * The frame origins are added and the rotations are multiplied.
     */
  CoordinateFrame& operator+=(const CoordinateFrame& rhs);

  /**
     * @brief operator+ for composing this frame (lhs) with another frame (rhs).
     * The frame origins are added and the rotations are multiplied.
     */
  CoordinateFrame operator+(const CoordinateFrame& rhs) const;

private:
  /// Frame origin defined in World space
  glm::vec3 m_worldFrameOrigin;

  /// Quaternion rotation from Frame to World space
  glm::quat m_world_T_frame_rotation;
};

#endif // COORDINATE_FRAME_H
