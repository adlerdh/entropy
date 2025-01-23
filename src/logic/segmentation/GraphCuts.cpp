#include "logic/segmentation/GraphCuts.h"
#include "logic/segmentation/GridCutsWrappers.h"
#include "logic/segmentation/SegHelpers.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <thread>
#include <unordered_map>
#include <vector>

namespace
{

static const int32_t NUM_THREADS = static_cast<int32_t>(std::thread::hardware_concurrency());

} // namespace

bool graphCutsBinarySegmentation(
  const GraphNeighborhoodType& hoodType,
  double terminalCapacity,
  const LabelType& fgSeedValue,
  const LabelType& /*bgSeedValue*/,
  const glm::ivec3& dims,
  const VoxelDistances& voxelDistances,
  std::function<double(int x, int y, int z, int dx, int dy, int dz)> getImageWeight,
  std::function<LabelType(int x, int y, int z)> getSeedValue,
  std::function<void(int x, int y, int z, LabelType value)> setResultSegValue
)
{
  using namespace std::chrono;

  // Type used for the graph cuts to represent:
  // -capcities of edges between nodes and terminals
  // -capacities of edges between nodes and their neighbors
  // -total flow
  using T = float;

  static constexpr bool multithread = false;

  spdlog::trace("Start creating grid");
  auto start = high_resolution_clock::now();

  std::unique_ptr<GridGraph_3D_Base_Wrapper<T, T, T> > grid = nullptr;

  switch (hoodType)
  {
  case GraphNeighborhoodType::Neighbors6:
  {
    if (multithread)
    {
      const int blockSize = std::max(32, std::min(dims.x, std::min(dims.y, dims.z)) / NUM_THREADS);
      spdlog::info("Number of threads: {}; block size: {}", NUM_THREADS, blockSize);
      grid = std::make_unique<
        GridGraph_3D_6C_MT_Wrapper<T, T, T> >(dims.x, dims.y, dims.z, NUM_THREADS, blockSize);
    }
    else
    {
      grid = std::make_unique<GridGraph_3D_6C_Wrapper<T, T, T> >(dims.x, dims.y, dims.z);
    }
    break;
  }
  case GraphNeighborhoodType::Neighbors26:
  {
    grid = std::make_unique<GridGraph_3D_26C_Wrapper<T, T, T> >(dims.x, dims.y, dims.z);
    break;
  }
  }

  spdlog::trace("Done creating grid");
  auto stop = high_resolution_clock::now();

  auto duration = duration_cast<milliseconds>(stop - start);
  spdlog::trace("Grid creation time: {} msec", duration.count());

  if (!grid)
  {
    spdlog::error("Null grid for graph cuts segmentation");
    return false;
  }

  spdlog::trace("Start filling grid");
  start = high_resolution_clock::now();

  if (multithread && GraphNeighborhoodType::Neighbors6 == hoodType)
  {
    const std::size_t N = dims.x * dims.y * dims.z;

    std::unique_ptr<T[]> cap_source = std::make_unique<float[]>(N);
    std::unique_ptr<T[]> cap_sink = std::make_unique<float[]>(N);
    std::unique_ptr<T[]> cap_lee = std::make_unique<float[]>(N);
    std::unique_ptr<T[]> cap_gee = std::make_unique<float[]>(N);
    std::unique_ptr<T[]> cap_ele = std::make_unique<float[]>(N);
    std::unique_ptr<T[]> cap_ege = std::make_unique<float[]>(N);
    std::unique_ptr<T[]> cap_eel = std::make_unique<float[]>(N);
    std::unique_ptr<T[]> cap_eeg = std::make_unique<float[]>(N);

    auto getIndex = [&dims](int x, int y, int z) -> std::size_t
    { return z * (dims.x * dims.y) + y * dims.x + x; };

    // Compute capacity for edge from X to X + dX
    auto computeNeighCap =
      [&getImageWeight](int x, int y, int z, int dx, int dy, int dz, double dist)
    { return static_cast<T>(getImageWeight(x, y, z, dx, dy, dz) / dist); };

    for (int z = 0; z < dims.z; ++z)
    {
      for (int y = 0; y < dims.y; ++y)
      {
        for (int x = 0; x < dims.x; ++x)
        {
          const LabelType seed = getSeedValue(x, y, z);
          const std::size_t index = getIndex(x, y, z);

          cap_source[index] = (seed > 0 && seed != fgSeedValue) ? terminalCapacity : 0.0;
          cap_sink[index] = (seed == fgSeedValue) ? terminalCapacity : 0.0;

          cap_lee[index] = computeNeighCap(x, y, z, -1, 0, 0, voxelDistances.distX);
          cap_gee[index] = computeNeighCap(x, y, z, 1, 0, 0, voxelDistances.distX);

          cap_ele[index] = computeNeighCap(x, y, z, 0, -1, 0, voxelDistances.distY);
          cap_ege[index] = computeNeighCap(x, y, z, 0, 1, 0, voxelDistances.distY);

          cap_eel[index] = computeNeighCap(x, y, z, 0, 0, -1, voxelDistances.distZ);
          cap_eeg[index] = computeNeighCap(x, y, z, 0, 0, 1, voxelDistances.distZ);
        }
      }
    }

    grid->set_caps(
      cap_source.get(),
      cap_sink.get(),
      cap_lee.get(),
      cap_gee.get(),
      cap_ele.get(),
      cap_ege.get(),
      cap_eel.get(),
      cap_eeg.get()
    );
  }
  else if (GraphNeighborhoodType::Neighbors26 == hoodType)
  {
    // Set symmetric capacities for edges from X to X + dX and from X + dX to X
    auto setNeighCaps =
      [&grid, &getImageWeight](int x, int y, int z, int dx, int dy, int dz, double dist)
    {
      const T cap = static_cast<T>(getImageWeight(x, y, z, dx, dy, dz) / dist);

      grid->set_neighbor_cap(grid->node_id(x, y, z), dx, dy, dz, cap);
      grid->set_neighbor_cap(grid->node_id(x + dx, y + dy, z + dz), -dx, -dy, -dz, cap);
    };

    for (int z = 0; z < dims.z; ++z)
    {
      const bool ZL = (z > 0);
      const bool ZH = (z < (dims.z - 1));

      for (int y = 0; y < dims.y; ++y)
      {
        const bool YL = (y > 0);
        const bool YH = (y < (dims.y - 1));

        for (int x = 0; x < dims.x; ++x)
        {
          const bool XL = (x > 0);
          const bool XH = (x < (dims.x - 1));

          const LabelType seed = getSeedValue(x, y, z);

          grid->set_terminal_cap(
            grid->node_id(x, y, z),
            (seed > 0 && seed != fgSeedValue) ? terminalCapacity : 0.0,
            (seed == fgSeedValue) ? terminalCapacity : 0.0
          );

          // 6 face neighbors:
          if (XH)
          {
            setNeighCaps(x, y, z, 1, 0, 0, voxelDistances.distX);
          }
          if (YH)
          {
            setNeighCaps(x, y, z, 0, 1, 0, voxelDistances.distY);
          }
          if (ZH)
          {
            setNeighCaps(x, y, z, 0, 0, 1, voxelDistances.distZ);
          }

          if (GraphNeighborhoodType::Neighbors26 == hoodType)
          {
            // 12 edge neighbors:
            if (XH && YH)
            {
              setNeighCaps(x, y, z, 1, 1, 0, voxelDistances.distXY);
            }
            if (XL && YH)
            {
              setNeighCaps(x, y, z, -1, 1, 0, voxelDistances.distXY);
            }
            if (XH && ZH)
            {
              setNeighCaps(x, y, z, 1, 0, 1, voxelDistances.distXZ);
            }
            if (XL && ZH)
            {
              setNeighCaps(x, y, z, -1, 0, 1, voxelDistances.distXZ);
            }
            if (YH && ZH)
            {
              setNeighCaps(x, y, z, 0, 1, 1, voxelDistances.distYZ);
            }
            if (YL && ZH)
            {
              setNeighCaps(x, y, z, 0, -1, 1, voxelDistances.distYZ);
            }

            // 8 vertex neighbors:
            if (XH && YH && ZH)
            {
              setNeighCaps(x, y, z, 1, 1, 1, voxelDistances.distXYZ);
            }
            if (XL && YH && ZH)
            {
              setNeighCaps(x, y, z, -1, 1, 1, voxelDistances.distXYZ);
            }
            if (XH && YL && ZH)
            {
              setNeighCaps(x, y, z, 1, -1, 1, voxelDistances.distXYZ);
            }
            if (XH && YH && ZL)
            {
              setNeighCaps(x, y, z, 1, 1, -1, voxelDistances.distXYZ);
            }
          }
        }
      }
    }
  }

  spdlog::trace("Done filling grid");
  stop = high_resolution_clock::now();

  duration = duration_cast<milliseconds>(stop - start);
  spdlog::trace("Grid fill time: {} msec", duration.count());

  spdlog::trace("Start computing max flow");
  start = high_resolution_clock::now();
  {
    grid->compute_maxflow();
  }
  stop = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(stop - start);

  spdlog::trace("Done computing max flow");
  spdlog::trace("Graph cuts execution time: {} msec", duration.count());

  spdlog::trace("Start reading back segmentation results");
  for (int z = 0; z < dims.z; ++z)
  {
    for (int y = 0; y < dims.y; ++y)
    {
      for (int x = 0; x < dims.x; ++x)
      {
        const LabelType label = static_cast<LabelType>(
          grid->get_segment(grid->node_id(x, y, z)) ? fgSeedValue : 0
        );

        setResultSegValue(x, y, z, label);
      }
    }
  }
  spdlog::trace("Done reading back segmentation results");

  return true;
}

bool graphCutsMultiLabelSegmentation(
  const GraphNeighborhoodType& hoodType,
  double terminalCapacity,
  const glm::ivec3& dims,
  const VoxelDistances& voxelDistances,
  std::function<double(int x, int y, int z, int dx, int dy, int dz)> /*getImageWeight*/,
  std::function<double(int index1, int index2)> getImageWeight1D,
  std::function<LabelType(int x, int y, int z)> getSeedValue,
  std::function<void(int x, int y, int z, LabelType value)> setResultSegValue
)
{
  using namespace std::chrono;

  // Type used for the alpha expansion algorithm to represent:
  // -data and smoothness costs
  // -resulting energy
  using T = float;

  static constexpr bool sk_ignoreBackgroundLabel = true;
  const LabelIndexMaps labelMaps = createLabelIndexMaps(dims, getSeedValue, sk_ignoreBackgroundLabel);
  const std::size_t numLabels = labelMaps.labelToIndex.size();

  spdlog::debug("Start creating expansion");

  std::vector<T> dataCosts(dims.x * dims.y * dims.z * numLabels, 0);

  auto getIndex = [&dims](int x, int y, int z) -> std::size_t
  { return z * (dims.x * dims.y) + y * dims.x + x; };

  for (int z = 0; z < dims.z; ++z)
  {
    for (int y = 0; y < dims.y; ++y)
    {
      for (int x = 0; x < dims.x; ++x)
      {
        const LabelType seedLabel = getSeedValue(x, y, z);

        for (std::size_t labelIndex = 0; labelIndex < numLabels; ++labelIndex)
        {
          const LabelType label = labelMaps.indexToLabel.at(labelIndex);

          dataCosts[getIndex(x, y, z) * numLabels + labelIndex] = (seedLabel == label)
                                                                    ? 0.0f
                                                                    : terminalCapacity;
        }
      }
    }
  }

  /*
    // For 3D grids with 6 and 26 connected neighboring system,
    // there are 6 an 13 smoothness tables for each pixel, respectively.
    const std::size_t numSmoothnessTables =
        ( GraphCutsNeighborhoodType::Neighbors6 == hoodType ) ? 3 : 13;

    T** smoothnessCosts = new T*[numSmoothnessTables * dims.x * dims.y * dims.z];

    // Set smoothness constraints for the 6 face-, 12 edge-, and 8 vertex-neighbors.
    // The order is specified in the AlphaExpansion documentation.

    for ( int z = 0; z < dims.z; ++z )
    {
//        const bool ZL = ( z > 0 );
        const bool ZH = ( z < ( dims.z - 1 ) );

        for ( int y = 0; y < dims.y; ++y )
        {
            const bool YL = ( y > 0 );
            const bool YH = ( y < ( dims.y - 1 ) );

            for ( int x = 0; x < dims.x; ++x )
            {
                const bool XL = ( x > 0 );
                const bool XH = ( x < ( dims.x - 1 ) );

                const std::size_t index = getIndex(x, y, z);

                for ( std::size_t t = 0; t < numSmoothnessTables; ++t )
                {
                    smoothnessCosts[numSmoothnessTables * index + t] = new T[numLabels * numLabels];
                }

                for ( std::size_t label = 0; label < numLabels; ++label )
                {
                    for ( std::size_t otherLabel = 0; otherLabel < numLabels; ++otherLabel )
                    {
                        const size_t l = otherLabel * numLabels + label;
                        const bool B = ( label != otherLabel );

                        if ( GraphCutsNeighborhoodType::Neighbors6 == hoodType )
                        {
                            smoothnessCosts[numSmoothnessTables * index +  0][l] = (XH && B) ? getImageWeight(x, y, z,  1,  0,  0) / voxelDistances.distX : 0;
                            smoothnessCosts[numSmoothnessTables * index +  1][l] = (YH && B) ? getImageWeight(x, y, z,  0,  1,  0) / voxelDistances.distY : 0;
                            smoothnessCosts[numSmoothnessTables * index +  2][l] = (ZH && B) ? getImageWeight(x, y, z,  0,  0,  1) / voxelDistances.distZ : 0;
                        }
                        else
                        {
                            smoothnessCosts[numSmoothnessTables * index +  0][l] = (XH && B)             ? getImageWeight(x, y, z,  1,  0,  0) / voxelDistances.distX   : 0;
                            smoothnessCosts[numSmoothnessTables * index +  1][l] = (YH && B)             ? getImageWeight(x, y, z,  0,  1,  0) / voxelDistances.distY   : 0;
                            smoothnessCosts[numSmoothnessTables * index +  2][l] = (XH && YL && B)       ? getImageWeight(x, y, z,  1, -1,  0) / voxelDistances.distXY  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  3][l] = (XH && YH && B)       ? getImageWeight(x, y, z,  1,  1,  0) / voxelDistances.distXY  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  4][l] = (ZH && B)             ? getImageWeight(x, y, z,  0,  0,  1) / voxelDistances.distZ   : 0;
                            smoothnessCosts[numSmoothnessTables * index +  5][l] = (YL && ZH && B)       ? getImageWeight(x, y, z,  0, -1,  1) / voxelDistances.distYZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  6][l] = (YH && ZH && B)       ? getImageWeight(x, y, z,  0,  1,  1) / voxelDistances.distYZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  7][l] = (XL && ZH && B)       ? getImageWeight(x, y, z, -1,  0,  1) / voxelDistances.distXZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index +  8][l] = (XL && YL && ZH && B) ? getImageWeight(x, y, z, -1, -1,  1) / voxelDistances.distXYZ : 0;
                            smoothnessCosts[numSmoothnessTables * index +  9][l] = (XL && YH && ZH && B) ? getImageWeight(x, y, z, -1,  1,  1) / voxelDistances.distXYZ : 0;
                            smoothnessCosts[numSmoothnessTables * index + 10][l] = (XH && ZH && B)       ? getImageWeight(x, y, z,  1,  0,  1) / voxelDistances.distXZ  : 0;
                            smoothnessCosts[numSmoothnessTables * index + 11][l] = (XH && YL && ZH && B) ? getImageWeight(x, y, z,  1, -1,  1) / voxelDistances.distXYZ : 0;
                            smoothnessCosts[numSmoothnessTables * index + 12][l] = (XH && YH && ZH && B) ? getImageWeight(x, y, z,  1,  1,  1) / voxelDistances.distXYZ : 0;
                        }
                    }
                }
            }
        }
    }*/

  // Distances between neighbors along x, y, and z directions of the 3D image grid:
  const int xDiff = 1;
  const int yDiff = dims.y;
  const int zDiff = dims.x * dims.y;

  auto smoothFn = [&getImageWeight1D,
                   &voxelDistances,
                   &yDiff,
                   &zDiff](int index1, int index2, int label1, int label2) -> double
  {
    if (label1 == label2)
    {
      return 0.0;
    }

    const int diff = std::abs(index1 - index2);
    double dist = 1.0;

    if (xDiff == diff)
    {
      dist = voxelDistances.distX;
    }
    else if (yDiff == diff)
    {
      dist = voxelDistances.distY;
    }
    else if (zDiff == diff)
    {
      dist = voxelDistances.distZ;
    }
    else if ((xDiff + yDiff) == diff)
    {
      dist = voxelDistances.distXY;
    }
    else if ((xDiff + zDiff) == diff)
    {
      dist = voxelDistances.distXZ;
    }
    else if ((yDiff + zDiff) == diff)
    {
      dist = voxelDistances.distYZ;
    }
    else if ((xDiff + yDiff + zDiff) == diff)
    {
      dist = voxelDistances.distXYZ;
    }

    return getImageWeight1D(index1, index2) / dist;
  };

  std::unique_ptr<AlphaExpansion_3D_Base_Wrapper<LabelType, T, T> > expansion = nullptr;

  switch (hoodType)
  {
  case GraphNeighborhoodType::Neighbors6:
  {
    const int blockSize = std::max(32, std::min(dims.x, std::min(dims.y, dims.z)) / NUM_THREADS);
    spdlog::info("Number of threads: {}; block size: {}", NUM_THREADS, blockSize);

    expansion = std::make_unique<AlphaExpansion_3D_6C_MT_Wrapper<LabelType, T, T> >(
      dims.x, dims.y, dims.z, numLabels, dataCosts.data(), smoothFn, NUM_THREADS, blockSize
    );
    break;
  }
  case GraphNeighborhoodType::Neighbors26:
  {
    expansion = std::make_unique<AlphaExpansion_3D_26C_Wrapper<LabelType, T, T> >(
      dims.x, dims.y, dims.z, numLabels, dataCosts.data(), smoothFn
    );
    break;
  }
  }

  spdlog::debug("Done creating expansion");

  spdlog::debug("Start computing expansion");
  auto start = high_resolution_clock::now();
  {
    expansion->perform();
  }
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(stop - start);

  spdlog::debug("Done computing expansion");
  spdlog::debug("Graph cuts (with alpha expansion) execution time: {} msec", duration.count());

  spdlog::debug("Start reading back segmentation results");
  LabelType* labeling = expansion->get_labeling();

  for (int z = 0; z < dims.z; ++z)
  {
    for (int y = 0; y < dims.y; ++y)
    {
      for (int x = 0; x < dims.x; ++x)
      {
        const std::size_t labelIndex = labeling[getIndex(x, y, z)];
        const LabelType label = labelMaps.indexToLabel.at(labelIndex);
        setResultSegValue(x, y, z, label);
      }
    }
  }
  spdlog::debug("Done reading back segmentation results");

  /*
    for ( int z = 0; z < dims.z; ++z ) {
        for ( int y = 0; y < dims.y; ++y ) {
            for ( int x = 0; x < dims.x; ++x )
            {
                const std::size_t index = getIndex(x, y, z);

                for ( std::size_t t = 0; t < numSmoothnessTables; ++t )
                {
                    delete[] smoothnessCosts[numSmoothnessTables * index + t];
                    smoothnessCosts[numSmoothnessTables * index + t] = nullptr;
                }
            }
        }
    }

    delete[] smoothnessCosts;
    smoothnessCosts = nullptr;
*/

  return true;
}
