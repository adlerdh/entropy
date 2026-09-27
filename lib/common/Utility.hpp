#pragma once

#include <array>
#include <memory>

namespace utility
{

template<typename T, typename... Args>
struct first_type
{
  /// The first type in a variadic template argument list.
  using type = T;
};

/**
 * @brief Make an array out of a sequence of arguments of the same type.
 * @param refs Values used to initialize the array.
 * @return std::array containing the forwarded values.
 * @see
 * https://stackoverflow.com/questions/35313043/how-to-use-enum-class-values-as-part-of-for-loop
 */
template<typename... Args>
std::array<typename first_type<Args...>::type, sizeof...(Args)> make_array(Args&&... refs)
{
  return std::array<typename first_type<Args...>::type, sizeof...(Args)>{{std::forward<Args>(refs)...}};
}

/**
 * @brief Static pointer cast for weak pointers.
 * @param r Weak pointer to cast.
 * @return Weak pointer cast to the requested pointee type.
 * @note This will throw an exception if the weak_ptr has expired
 */
template<class T, class U>
std::weak_ptr<T> static_pointer_cast(std::weak_ptr<U> const& r)
{
  return std::static_pointer_cast<T>(std::shared_ptr<U>(r));
}

} // namespace utility
