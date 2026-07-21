#include "image/Image.h"

#include "common/Exception.hpp"
#include "internal/ImageUtility.tpp"
#include "image/ImageUtility.h"

#include <spdlog/spdlog.h>

#include <span>

QuantileOfValue Image::valueToQuantile(uint32_t comp, int64_t value) const
{
  if (comp >= m_header.numComponentsPerPixel()) {
    spdlog::error(
      "Invalid image component {} (image has {}) when converting value {} to quantile",
      comp,
      m_header.numComponentsPerPixel(),
      value);
    throwDebug("Invalid image component");
  }

  if (m_settings.usingExactQuantiles()) {
    switch (m_header.memoryComponentType()) {
      case ComponentType::Int8:
        return convertValueToQuantile<int8_t>(std::span{m_dataSorted_int8[comp]}, static_cast<int8_t>(value));
      case ComponentType::UInt8:
        return convertValueToQuantile<uint8_t>(std::span{m_dataSorted_uint8[comp]}, static_cast<uint8_t>(value));
      case ComponentType::Int16:
        return convertValueToQuantile<int16_t>(std::span{m_dataSorted_int16[comp]}, static_cast<int16_t>(value));
      case ComponentType::UInt16:
        return convertValueToQuantile<uint16_t>(std::span{m_dataSorted_uint16[comp]}, static_cast<uint16_t>(value));
      case ComponentType::Int32:
        return convertValueToQuantile<int32_t>(std::span{m_dataSorted_int32[comp]}, static_cast<int32_t>(value));
      case ComponentType::UInt32:
        return convertValueToQuantile<uint32_t>(std::span{m_dataSorted_uint32[comp]}, static_cast<uint32_t>(value));
      case ComponentType::Float32:
        return convertValueToQuantile<float>(std::span{m_dataSorted_float32[comp]}, static_cast<float>(value));
      default:
        spdlog::error("Invalid memory component type '{}'", m_header.memoryComponentTypeAsString());
        throwDebug("Invalid memory component type");
    }
  }

  const double q = m_tdigests.at(comp).cdf(static_cast<double>(value));

  QuantileOfValue qov;
  qov.lowerQuantile = q;
  qov.upperQuantile = q;
  qov.lowerIndex = 0;
  qov.upperIndex = 0;
  qov.lowerValue = value;
  qov.upperValue = value;
  qov.foundValue = true;
  return qov;
}

QuantileOfValue Image::valueToQuantile(uint32_t comp, double value) const
{
  if (comp >= m_header.numComponentsPerPixel()) {
    spdlog::error(
      "Invalid image component {} (image has {}) when converting value {} to quantile",
      comp,
      m_header.numComponentsPerPixel(),
      value);
    throwDebug("Invalid image component");
  }

  if (m_settings.usingExactQuantiles()) {
    switch (m_header.memoryComponentType()) {
      case ComponentType::Int8:
        return convertValueToQuantile(std::span{m_dataSorted_int8[comp]}, static_cast<int8_t>(value));
      case ComponentType::UInt8:
        return convertValueToQuantile(std::span{m_dataSorted_uint8[comp]}, static_cast<uint8_t>(value));
      case ComponentType::Int16:
        return convertValueToQuantile(std::span{m_dataSorted_int16[comp]}, static_cast<int16_t>(value));
      case ComponentType::UInt16:
        return convertValueToQuantile(std::span{m_dataSorted_uint16[comp]}, static_cast<uint16_t>(value));
      case ComponentType::Int32:
        return convertValueToQuantile(std::span{m_dataSorted_int32[comp]}, static_cast<int32_t>(value));
      case ComponentType::UInt32:
        return convertValueToQuantile(std::span{m_dataSorted_uint32[comp]}, static_cast<uint32_t>(value));
      case ComponentType::Float32:
        return convertValueToQuantile(std::span{m_dataSorted_float32[comp]}, static_cast<float>(value));
      default:
        spdlog::error("Invalid memory component type '{}'", m_header.memoryComponentTypeAsString());
        throwDebug("Invalid memory component type");
    }
  }

  const double q = m_tdigests.at(comp).cdf(value);

  QuantileOfValue qov;
  qov.lowerQuantile = q;
  qov.upperQuantile = q;
  qov.lowerIndex = 0;
  qov.upperIndex = 0;
  qov.lowerValue = value;
  qov.upperValue = value;
  qov.foundValue = true;
  return qov;
}

double Image::quantileToValue(uint32_t comp, double quantile) const
{
  if (comp >= m_header.numComponentsPerPixel()) {
    spdlog::error(
      "Invalid image component {} (image has {}) when converting quantile {} to value",
      comp,
      m_header.numComponentsPerPixel(),
      quantile);
    throwDebug("Invalid image component");
  }

  if (m_settings.usingExactQuantiles()) {
    switch (m_header.memoryComponentType()) {
      case ComponentType::Int8:
        return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_int8[comp]}, quantile));
      case ComponentType::UInt8:
        return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_uint8[comp]}, quantile));
      case ComponentType::Int16:
        return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_int16[comp]}, quantile));
      case ComponentType::UInt16:
        return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_uint16[comp]}, quantile));
      case ComponentType::Int32:
        return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_int32[comp]}, quantile));
      case ComponentType::UInt32:
        return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_uint32[comp]}, quantile));
      case ComponentType::Float32:
        return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_float32[comp]}, quantile));
      default:
        spdlog::error("Invalid memory component type '{}'", m_header.memoryComponentTypeAsString());
        throwDebug("Invalid memory component type");
    }
  }

  return m_tdigests.at(comp).quantile(quantile);
}
