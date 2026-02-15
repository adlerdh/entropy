#pragma once

#include <type_traits>
#include <utility>

#if defined(ENTROPY_USE_TL_EXPECTED) && ENTROPY_USE_TL_EXPECTED

#include <tl/expected.hpp>

namespace entropy_expected {
using tl::expected;
using tl::unexpected;
using tl::unexpect;
using tl::make_unexpected;
}

#else

#if defined(__has_include)
#if __has_include(<expected>)
#include <expected>
#endif
#endif

#if defined(__cpp_lib_expected) && (__cpp_lib_expected >= 202211L)

namespace entropy_expected {
using std::expected;
using std::unexpected;
using std::unexpect;

template<class E>
constexpr std::unexpected<std::decay_t<E>> make_unexpected(E&& e) {
  return std::unexpected<std::decay_t<E>>(std::forward<E>(e));
}
}

#else

#include <tl/expected.hpp>

namespace entropy_expected {
using tl::expected;
using tl::unexpected;
using tl::unexpect;
using tl::make_unexpected;
}

#endif
#endif
