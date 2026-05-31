#pragma once

#include <expected>
#include <type_traits>
#include <utility>

namespace entropy_expected
{
using std::expected;
using std::unexpect;
using std::unexpected;

template<class E>
constexpr std::unexpected<std::decay_t<E> > make_unexpected(E&& e)
{
  return std::unexpected<std::decay_t<E> >(std::forward<E>(e));
}
} // namespace entropy_expected
