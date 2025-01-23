#ifndef COMMON_PUBLIC_TYPES_H
#define COMMON_PUBLIC_TYPES_H

#include <functional>
#include <optional>

/// @note These convenience types were used in HistoloZee, but haven't yet been used in Entropy.

/// Function that enqueues re-render (update) calls for all views.
/// It is expected that the application's window system will take
/// care of actually calling "render" on the view at the correct time.
using AllViewsUpdaterType = std::function<void(void)>;

/// Function that resets cameras of all views to their default states
using AllViewsResetterType = std::function<void()>;

/// Function that recenters cameras of all views
using AllViewsRecenterType = std::function<void(
  bool recenterCrosshairs,
  bool recenterOnCurrentCrosshairsPosition,
  bool resetObliqueOrientation,
  const std::optional<bool>& resetZoom
)>;

/// Shorthand for a function that returns an object
template<class T>
using GetterType = std::function<T(void)>;

/// Shorthand for a function that sets an object
template<class T>
using SetterType = std::function<void(T)>;

/// Function for querying an object by based on an ID
template<class ValueType, class IdType>
using QuerierType = std::function<ValueType(const IdType&)>;

#endif // COMMON_PUBLIC_TYPES_H
