#ifndef UNDERLYING_ENUM_TYPE_H
#define UNDERLYING_ENUM_TYPE_H

#include <cstdint>
#include <ostream>
#include <type_traits>

template<class T>
typename std::underlying_type<T>::type underlyingType(const T& x)
{
  return static_cast<typename std::underlying_type<T>::type>(x);
}

template<class T>
int32_t underlyingType_asInt32(const T& x)
{
  return static_cast<int32_t>(static_cast<typename std::underlying_type<T>::type>(x));
}

template<class T>
uint32_t underlyingType_asUInt32(const T& x)
{
  return static_cast<uint32_t>(static_cast<typename std::underlying_type<T>::type>(x));
}

template<typename T>
std::ostream& operator<<(
  typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e
)
{
  return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

#endif // UNDERLYING_ENUM_TYPE_H
