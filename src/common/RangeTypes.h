#ifndef RANGE_TYPES_H
#define RANGE_TYPES_H

#include <boost/range/any_range.hpp>

/// Convenience alias for a forward-traversable range.
/// Useful when we don't want to expose the container type used internally by an object.
template<class T>
using ForwardRange = boost::any_range<T, boost::forward_traversal_tag, T&, std::ptrdiff_t>;

#endif // RANGE_TYPES_H
