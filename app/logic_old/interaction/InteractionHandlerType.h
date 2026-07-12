#pragma once

/**
 * @brief Describes the types of interaction handlers
 */
enum class InteractionHandlerType
{
  Camera,
  Crosshairs,
  RefImageTransform,
  SlideTransform,
  StackTransform,
  WindowLevel
};

/**
 * @brief All interaction mode types. Only one is active at a time.
 * @todo This belongs in InteractionModes.h
 */
enum class InteractionModeType
{
  CameraTranslate,
  CameraRotate,
  CameraZoom,
  CrosshairsPointer,
  RefImageRotate,
  RefImageTranslate,
  RefImageWindowLevel,
  SlideRotate,
  SlideStretch,
  SlideTranslate,
  StackRotate,
  StackTranslate
};
