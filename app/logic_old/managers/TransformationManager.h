#ifndef TRANSFORMATION_MANAGER_H
#define TRANSFORMATION_MANAGER_H

#include "common/CoordinateFrame.h"
#include "logic/TransformationState.h"

#include <unordered_map>

/**
 * @brief Manages transformations across the application.
 *
 * @note Crosshairs in Reference Imagery and Slide Stack views have the same World origin position
 * but possibly different rotations.
 */
class TransformationManager
{
public:
  explicit TransformationManager();

  TransformationManager(const TransformationManager&) = delete;
  TransformationManager& operator=(const TransformationManager&) = delete;

  TransformationManager(TransformationManager&&) = default;
  TransformationManager& operator=(TransformationManager&&) = default;

  ~TransformationManager() = default;

  /// Stage the World-space origin of all crosshairs
  void stageCrosshairsOrigin(const glm::vec3& worldOrigin);

  const CoordinateFrame& getCrosshairsFrame(const TransformationState&) const;

  void stageCrosshairsFrame(CoordinateFrame);
  void commitCrosshairsFrame();

  const CoordinateFrame& getSlideStackCrosshairsFrame(const TransformationState&) const;

  const CoordinateFrame& getSlideStackFrame(const TransformationState&) const;

  void stageSlideStackFrame(CoordinateFrame);
  void commitSlideStackFrame();

private:
  /// Crosshairs used in reference imagery views
  std::unordered_map<TransformationState, CoordinateFrame> m_referenceCrosshairsFrames;

  /// Crosshairs used in Slide Stack views
  std::unordered_map<TransformationState, CoordinateFrame> m_slideStackCrosshairsFrames;

  /// Coordinate frame of the Slide Stack
  std::unordered_map<TransformationState, CoordinateFrame> m_slideStackFrames;
};

#endif // TRANSFORMATION_MANAGER_H
