#include "logic/managers/TransformationManager.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <iostream>

namespace
{

static const CoordinateFrame
  sk_defaultFrame{glm::vec3{0.0f, 0.0f, 0.0f}, 0.0f, glm::vec3{0.0f, 0.0f, 1.0f}};

static const std::unordered_map<TransformationState, CoordinateFrame> sk_defaultFrames{
  {TransformationState::Staged, sk_defaultFrame}, {TransformationState::Committed, sk_defaultFrame}
};

} // namespace

TransformationManager::TransformationManager()
  : m_referenceCrosshairsFrames{sk_defaultFrames}
  , m_slideStackCrosshairsFrames{sk_defaultFrames}
  , m_slideStackFrames{sk_defaultFrames}
{
}

void TransformationManager::stageCrosshairsOrigin(const glm::vec3& worldOrigin)
{
  m_referenceCrosshairsFrames[TransformationState::Staged].setWorldOrigin(worldOrigin);
  m_slideStackCrosshairsFrames[TransformationState::Staged].setWorldOrigin(worldOrigin);
}

const CoordinateFrame& TransformationManager::getCrosshairsFrame(const TransformationState& state
) const
{
  return m_referenceCrosshairsFrames.at(state);
}

void TransformationManager::stageCrosshairsFrame(CoordinateFrame frame)
{
  // All crosshairs have the same origin position
  stageCrosshairsOrigin(frame.worldOrigin());

  m_referenceCrosshairsFrames[TransformationState::Staged] = std::move(frame);
}

void TransformationManager::commitCrosshairsFrame()
{
  m_referenceCrosshairsFrames[TransformationState::Committed]
    = m_referenceCrosshairsFrames[TransformationState::Staged];
}

const CoordinateFrame& TransformationManager::getSlideStackCrosshairsFrame(
  const TransformationState& state
) const
{
  return m_slideStackCrosshairsFrames.at(state);
}

const CoordinateFrame& TransformationManager::getSlideStackFrame(const TransformationState& state
) const
{
  return m_slideStackFrames.at(state);
}

void TransformationManager::stageSlideStackFrame(CoordinateFrame frame)
{
  m_slideStackFrames[TransformationState::Staged] = frame;

  m_slideStackCrosshairsFrames[TransformationState::Staged].setFrameToWorldRotation(
    frame.world_O_frame_rotation()
  );
}

void TransformationManager::commitSlideStackFrame()
{
  m_slideStackFrames[TransformationState::Committed]
    = m_slideStackFrames[TransformationState::Staged];

  m_slideStackCrosshairsFrames[TransformationState::Committed]
    = m_slideStackCrosshairsFrames[TransformationState::Staged];
}
