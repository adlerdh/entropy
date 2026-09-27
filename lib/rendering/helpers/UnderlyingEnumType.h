#pragma once

#include <cstdint>
#include <ostream>
#include <type_traits>

/**
 * @brief Return an enum value as its declared underlying type.
 *
 * This is still used by the OpenGL wrapper layer because the local GL enum classes intentionally store OpenGL enum
 * constants while preserving type safety at API boundaries. In C++23 this is similar to `std::to_underlying`, but the
 * project keeps this helper for compatibility with existing call sites and the explicit integer-width helpers below.
 */
template<class T>
typename std::underlying_type<T>::type underlyingType(const T& x)
{
  return static_cast<typename std::underlying_type<T>::type>(x);
}

/**
 * @brief Return an enum value as a signed 32-bit integer.
 */
template<class T>
int32_t underlyingType_asInt32(const T& x)
{
  return static_cast<int32_t>(static_cast<typename std::underlying_type<T>::type>(x));
}

/**
 * @brief Return an enum value as an unsigned 32-bit integer.
 */
template<class T>
uint32_t underlyingType_asUInt32(const T& x)
{
  return static_cast<uint32_t>(static_cast<typename std::underlying_type<T>::type>(x));
}

/**
 * @brief Stream an enum value as its underlying integer value.
 */
template<typename T>
std::ostream& operator<<(typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e)
{
  return stream << static_cast<typename std::underlying_type<T>::type>(e);
}
