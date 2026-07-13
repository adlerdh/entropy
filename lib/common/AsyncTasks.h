#pragma once

#include <uuid.h>

#include <optional>
#include <string>

/**
 * @brief Enumeration of asynchronous task categories emitted by background workers.
 */
enum class AsyncTasks
{
  IsosurfaceMeshGeneration //!< Build an isosurface mesh from image or segmentation data
};

/**
 * @brief Metadata and completion status for a single asynchronous task.
 */
struct AsyncTaskDetails
{
  AsyncTasks task;         //!< Type of task
  std::string description; //!< Description of the task
  uuids::uuid taskUid;     //!< UID of the task

  std::optional<uuids::uuid> imageUid = std::nullopt;    //!< UID of image associated with the task
  std::optional<uint32_t> imageComponent = std::nullopt; //!< Image component associated with the task
  std::optional<uuids::uuid> objectUid = std::nullopt;   //!< UID of the object created by the task
  bool success = false;                                  //!< Flag indicating task success (true) or failure (false)
};
