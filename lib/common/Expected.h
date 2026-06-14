#pragma once

#include <expected>
#include <type_traits>
#include <utility>

namespace entropy_expected
{
/// Project namespace alias for C++23 std::expected.
using std::expected;

/// Project namespace alias for constructing unexpected values in-place.
using std::unexpect;

/// Project namespace alias for std::unexpected.
using std::unexpected;

/**
 * @brief Construct an unexpected value while decaying the error type.
 * @param e Error value to store.
 * @return std::unexpected containing @p e.
 */
template<class E>
constexpr std::unexpected<std::decay_t<E> > make_unexpected(E&& e)
{
  return std::unexpected<std::decay_t<E> >(std::forward<E>(e));
}
} // namespace entropy_expected
