#pragma once

#include "common/Exception.hpp"
#include "common/Types.h"

#include <spdlog/spdlog.h>

#include <limits>
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
  // Lowest and max values of destination component type, cast to source component type
  static const SrcCompType k_lowestValue = static_cast<SrcCompType>(std::numeric_limits<DstCompType>::lowest());
  static const SrcCompType k_maximumValue = static_cast<SrcCompType>(std::numeric_limits<DstCompType>::max());

  std::vector<DstCompType> data(numElements, 0);

  if (!buffer) {
    spdlog::error("Null buffer when creating buffer: returning zero data");
    return data;
  }

  const SrcCompType* bufferCast = static_cast<const SrcCompType*>(buffer);

  // Clamp values to destination range [lowest, maximum] prior to cast:
  for (std::size_t i = 0; i < numElements; ++i) {
    data[i] = static_cast<DstCompType>(std::min(std::max(bufferCast[i], k_lowestValue), k_maximumValue));
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
      throw_debug("Unknown component type when creating buffer")
    }
  }

  return std::vector<DstCompType>{};
}
