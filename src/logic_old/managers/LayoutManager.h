#ifndef LAYOUT_MANAGER_H
#define LAYOUT_MANAGER_H

#include "common/UID.h"
#include "common/UIDRange.h"

#include "gui/layout/LayoutData.h"
#include "gui/layout/ViewType.h"
#include "gui/layout/ViewTypeRange.h"

#include <boost/range/any_range.hpp>

#include <list>
#include <memory>
#include <optional>
#include <string>

class QWidget;

namespace gui
{
class GLWidget;
class ViewWidget;
} // namespace gui

class LayoutManager final
{
public:
  /// Construct layouts from descriptions in JSON-format configuration file
  explicit LayoutManager(const std::string& layoutConfig);

  LayoutManager(const LayoutManager&) = delete;
  LayoutManager& operator=(const LayoutManager&) = delete;

  LayoutManager(LayoutManager&&) = default;
  LayoutManager& operator=(LayoutManager&&) = default;

  ~LayoutManager();

  /// Ordered UIDs of the layouts
  uid_range_t getOrderedLayoutUids() const;

  /// Get layout tab data for a given layout UID.
  /// @throws If layout with given UID does not exist.
  const gui::LayoutTabData& getLayoutTabData(const UID& layoutUid);

  /// Get layout tab data for a given layout tab index.
  /// @throws If layout with given UID does not exist.
  const gui::LayoutTabData& getLayoutTabData(int layoutIndex);

  /// Set the widget for its given location in the layout. The widget knows its view UID.
  void setViewWidget(gui::ViewWidget*);

  /// Get the view UIDs
  uid_range_t getViewUids() const;

  /// Get the view UIDs and their corresponding view types
  view_type_range_t getViewTypes() const;

  /// Get the type of a view. If the view UID does not exist, \c std::nullopt is returned
  std::optional<gui::ViewType> getViewType(const UID& viewUid) const;

  /// Get a list of UIDs of all views with a given view type
  std::list<UID> getViewUidsOfType(const gui::ViewType&) const;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif // LAYOUT_MANAGER_H
