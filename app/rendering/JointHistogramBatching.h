#pragma once

#include <algorithm>
#include <cstdint>

namespace rendering
{

inline constexpr std::uint64_t kJointHistogramMaxVoxelsPerBatch = 1'000'000u;

/** Keep interaction previews small even when the previous transform was unusually cheap. */
inline constexpr std::uint64_t kJointHistogramPreviewVoxels = 65'536u;
inline constexpr std::uint64_t kJointHistogramRefinementVoxels = 2'097'152u;
inline constexpr std::uint64_t kJointHistogramWorkChunkVoxels = 16'384u;
inline constexpr double kJointHistogramTargetGpuNanoseconds = 6'000'000.0;

inline double jointHistogramUpdateCost(double previous, double measured)
{
  // React immediately to expensive work, but recover cautiously when overlap/contention decreases.
  measured = std::max(0.1, measured);
  return std::max(measured, 0.85 * previous + 0.15 * measured);
}

inline std::uint64_t jointHistogramSubmissionLimit(double nanosecondsPerVoxel, bool newTransform)
{
  const double ceiling = newTransform ? kJointHistogramPreviewVoxels : kJointHistogramRefinementVoxels;
  return static_cast<std::uint64_t>(
    std::clamp(kJointHistogramTargetGpuNanoseconds / nanosecondsPerVoxel, 1.0, ceiling));
}

/** One disjoint strided subset of the complete linear voxel domain. */
struct JointHistogramBatch
{
  std::uint64_t offset = 0;
  std::uint64_t stride = 0;
  std::uint64_t size = 0;
};

/** Return the number of interleaved batches needed without exceeding the requested batch size. */
constexpr std::uint64_t jointHistogramBatchCount(const std::uint64_t voxelCount, const std::uint64_t maximumBatchSize)
{
  return voxelCount > 0u && maximumBatchSize > 0u ? 1u + (voxelCount - 1u) / maximumBatchSize : 0u;
}

/**
 * Return a batch whose indices are `offset + localIndex * stride`.
 * Consecutive batches partition the voxel domain while each one samples across its full linear extent.
 */
constexpr JointHistogramBatch jointHistogramBatch(
  const std::uint64_t voxelCount,
  const std::uint64_t maximumBatchSize,
  const std::uint64_t batchIndex)
{
  const std::uint64_t batchCount = jointHistogramBatchCount(voxelCount, maximumBatchSize);
  if (batchIndex >= batchCount) {
    return {};
  }
  return {batchIndex, batchCount, 1u + (voxelCount - 1u - batchIndex) / batchCount};
}

/** Interleave XY samples in every Z slice, drawing all slices as instances in each batch. */
constexpr std::uint64_t jointHistogramPlaneBatchCount(
  const std::uint64_t planeSize,
  const std::uint64_t depth,
  const std::uint64_t maximumBatchSize)
{
  if (depth == 0 || depth > maximumBatchSize) {
    return 0;
  }
  return jointHistogramBatchCount(planeSize, maximumBatchSize / depth);
}

} // namespace rendering
