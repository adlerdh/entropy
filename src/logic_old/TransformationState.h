#ifndef TRANSFORMATION_STATE_H
#define TRANSFORMATION_STATE_H

/**
 * @brief Describes the state of an object's transformation.
 */
enum class TransformationState
{
  /// The value is being actively changed and has been staged. It is not yet committed
  /// and should not be reflected in the transformations of the view cameras.
  Staged,

  /// The value has been committed, so it should be reflected in the transformations of the
  /// view cameras. This is a "final" state.
  Committed
};

#endif // TRANSFORMATION_STATE_H
