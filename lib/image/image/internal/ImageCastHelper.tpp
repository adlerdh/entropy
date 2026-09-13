#pragma once

#include "common/Exception.hpp"
#include "common/Types.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

/**
 * @brief Copy and clamp a raw typed buffer into a destination component vector.
 * @tparam SrcCompType Source component type in \p buffer.
 * @tparam DstCompType Destination component type.
 * @param[in] buffer Raw source buffer with \p numElements entries.
 * @param[in] numElements Number of components to copy.
 * @return Destination vector with values clamped to the destination type range.
 */
template<typename SrcCompType, typename DstCompType>
std::vector<DstCompType> createBuffer_dispatch(const void* buffer, std::size_t numElements)
{
  if (!buffer) {
    spdlog::error("Cannot create an image buffer from a null source buffer");
    throwDebug("Cannot create an image buffer from a null source buffer");
  }

  std::vector<DstCompType> data(numElements, 0);
  const SrcCompType* bufferCast = static_cast<const SrcCompType*>(buffer);
  constexpr long double k_lowestValue = static_cast<long double>(std::numeric_limits<DstCompType>::lowest());
  constexpr long double k_maximumValue = static_cast<long double>(std::numeric_limits<DstCompType>::max());

  for (std::size_t i = 0; i < numElements; ++i) {
    if constexpr (std::is_floating_point_v<SrcCompType>) {
      if (std::isnan(bufferCast[i])) {
        if constexpr (std::is_floating_point_v<DstCompType>) {
          data[i] = static_cast<DstCompType>(bufferCast[i]);
        }
        else {
          data[i] = DstCompType{};
        }
        continue;
      }
    }

    const long double value = static_cast<long double>(bufferCast[i]);
    if (value <= k_lowestValue) {
      data[i] = std::numeric_limits<DstCompType>::lowest();
    }
    else if (value >= k_maximumValue) {
      data[i] = std::numeric_limits<DstCompType>::max();
    }
    else {
      data[i] = static_cast<DstCompType>(bufferCast[i]);
    }
  }

  return data;
}

/**
 * @brief Copy a raw buffer of runtime component type into a typed destination vector.
 * @tparam DstCompType Destination component type.
 * @param[in] buffer Raw source buffer with \p numElements entries.
 * @param[in] numElements Number of components to copy.
 * @param[in] srcComponentType Runtime component type of \p buffer.
 * @return Destination vector with values converted to \p DstCompType.
 */
template<typename DstCompType>
std::vector<DstCompType> createBuffer(const void* buffer, std::size_t numElements, ComponentType srcComponentType)
{
  switch (srcComponentType) {
    case ComponentType::UInt8: {
      return createBuffer_dispatch<uint8_t, DstCompType>(buffer, numElements);
    }
    case ComponentType::Int8: {
      return createBuffer_dispatch<int8_t, DstCompType>(buffer, numElements);
    }
    case ComponentType::UInt16: {
      return createBuffer_dispatch<uint16_t, DstCompType>(buffer, numElements);
    }
    case ComponentType::Int16: {
      return createBuffer_dispatch<int16_t, DstCompType>(buffer, numElements);
    }
    case ComponentType::UInt32: {
      return createBuffer_dispatch<uint32_t, DstCompType>(buffer, numElements);
    }
    case ComponentType::Int32: {
      return createBuffer_dispatch<int32_t, DstCompType>(buffer, numElements);
    }
    case ComponentType::ULong: {
      return createBuffer_dispatch<unsigned long, DstCompType>(buffer, numElements);
    }
    case ComponentType::Long: {
      return createBuffer_dispatch<long, DstCompType>(buffer, numElements);
    }
    case ComponentType::ULongLong: {
      return createBuffer_dispatch<unsigned long long, DstCompType>(buffer, numElements);
    }
    case ComponentType::LongLong: {
      return createBuffer_dispatch<long long, DstCompType>(buffer, numElements);
    }
    case ComponentType::Float32: {
      return createBuffer_dispatch<float, DstCompType>(buffer, numElements);
    }
    case ComponentType::Float64: {
      return createBuffer_dispatch<double, DstCompType>(buffer, numElements);
    }
    case ComponentType::LongDouble: {
      return createBuffer_dispatch<long double, DstCompType>(buffer, numElements);
    }

    default:
    case ComponentType::Undefined: {
      spdlog::error("Unknown component type when creating buffer");
      throwDebug("Unknown component type when creating buffer");
    }
  }
}
