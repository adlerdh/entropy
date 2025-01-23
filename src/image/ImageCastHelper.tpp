#ifndef IMAGE_CAST_HELPER_TPP
#define IMAGE_CAST_HELPER_TPP

#include "common/Exception.hpp"

#include <itkCommonEnums.h>
#include <spdlog/spdlog.h>

#include <limits>
#include <vector>

/**
 * @brief createBuffer_dispatch
 * @param buffer
 * @param numElements
 * @return
 */
template<typename SrcCompType, typename DstCompType>
std::vector<DstCompType> createBuffer_dispatch(const void* buffer, std::size_t numElements)
{
  // Lowest and max values of destination component type, cast to source component type
  static const SrcCompType k_lowestValue = static_cast<SrcCompType>(
    std::numeric_limits<DstCompType>::lowest()
  );
  static const SrcCompType k_maximumValue = static_cast<SrcCompType>(
    std::numeric_limits<DstCompType>::max()
  );

  std::vector<DstCompType> data(numElements, 0);

  if (!buffer)
  {
    spdlog::error("Null buffer when creating buffer: returning zero data");
    return data;
  }

  const SrcCompType* bufferCast = static_cast<const SrcCompType*>(buffer);

  // Clamp values to destination range [lowest, maximum] prior to cast:
  for (std::size_t i = 0; i < numElements; ++i)
  {
    data[i] = static_cast<DstCompType>(
      std::min(std::max(bufferCast[i], k_lowestValue), k_maximumValue)
    );
  }

  return data;
}

/**
 * @brief createBuffer
 * @param buffer
 * @param numElements
 * @param srcComponentType Type of components in \c buffer
 * @return
 */
template<typename DstCompType>
std::vector<DstCompType> createBuffer(
  const void* buffer, std::size_t numElements, const itk::IOComponentEnum& srcComponentType
)
{
  using CType = itk::IOComponentEnum;

  switch (srcComponentType)
  {
  case CType::UCHAR:
  {
    return createBuffer_dispatch<uint8_t, DstCompType>(buffer, numElements);
  }
  case CType::CHAR:
  {
    return createBuffer_dispatch<int8_t, DstCompType>(buffer, numElements);
  }
  case CType::USHORT:
  {
    return createBuffer_dispatch<uint16_t, DstCompType>(buffer, numElements);
  }
  case CType::SHORT:
  {
    return createBuffer_dispatch<int16_t, DstCompType>(buffer, numElements);
  }
  case CType::UINT:
  {
    return createBuffer_dispatch<uint32_t, DstCompType>(buffer, numElements);
  }
  case CType::INT:
  {
    return createBuffer_dispatch<int32_t, DstCompType>(buffer, numElements);
  }
  case CType::ULONG:
  {
    return createBuffer_dispatch<unsigned long, DstCompType>(buffer, numElements);
  }
  case CType::LONG:
  {
    return createBuffer_dispatch<long, DstCompType>(buffer, numElements);
  }
  case CType::ULONGLONG:
  {
    return createBuffer_dispatch<unsigned long long, DstCompType>(buffer, numElements);
  }
  case CType::LONGLONG:
  {
    return createBuffer_dispatch<long long, DstCompType>(buffer, numElements);
  }
  case CType::FLOAT:
  {
    return createBuffer_dispatch<float, DstCompType>(buffer, numElements);
  }
  case CType::DOUBLE:
  {
    return createBuffer_dispatch<double, DstCompType>(buffer, numElements);
  }
  case CType::LDOUBLE:
  {
    return createBuffer_dispatch<long double, DstCompType>(buffer, numElements);
  }

  default:
  case CType::UNKNOWNCOMPONENTTYPE:
  {
    spdlog::error("Unknown component type when creating buffer");
    throw_debug("Unknown component type when creating buffer")
  }
  }

  return std::vector<DstCompType>{};
}

#endif // IMAGE_CAST_HELPER_TPP
