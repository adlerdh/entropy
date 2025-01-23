#include "logic/managers/LayoutManager.h"
#include "gui/layout/SplitterLayoutConstruction.h"
#include "gui/view/ViewWidget.h"

#include "common/HZeeException.hpp"

#include <boost/range.hpp>
#include <boost/range/adaptor/map.hpp>

/// @todo Put this in gui code
#include <QSplitter>

#include <list>
#include <unordered_map>

class QWidget;

struct LayoutManager::Impl
{
  /// Construct from layout configuration
  explicit Impl(const std::string& layoutConfig);

  ~Impl() = default;

  /// Vector of layout tabs that is parsed from configuration file. This is used to construct
  /// the layout tab widgets displayed in the UI.
  gui::AllLayoutTabs m_parsedLayouts;

  /// List of all layout UIDs, in order of their appearance in the UI.
  std::list<UID> m_orderedLayoutUids;

  /// Map from each layout UID to its layout data.
  std::unordered_map<UID, gui::LayoutTabData> m_layoutData;

  /// Map from each view UID to its ViewType.
  std::unordered_map<UID, gui::ViewType> m_viewTypes;

  /// Map from each view UID to its containing QSplitter widget.
  std::unordered_map<UID, QSplitter*> m_viewSplitters;
};

LayoutManager::LayoutManager(const std::string& layoutConfig)
  : m_impl(std::make_unique<Impl>(layoutConfig))
{
}

LayoutManager::~LayoutManager() = default;

uid_range_t LayoutManager::getOrderedLayoutUids() const
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  return m_impl->m_orderedLayoutUids;
}

const gui::LayoutTabData& LayoutManager::getLayoutTabData(const UID& layoutUid)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  const auto it = m_impl->m_layoutData.find(layoutUid);
  if (std::end(m_impl->m_layoutData) == it)
  {
    std::ostringstream ss;
    ss << "Central Widget not found for requested layout " << layoutUid << std::ends;
    throw_debug(ss.str());
  }

  return it->second;
}

const gui::LayoutTabData& LayoutManager::getLayoutTabData(int layoutIndex)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  auto it = m_impl->m_orderedLayoutUids.begin();
  std::advance(it, layoutIndex);
  return getLayoutTabData(*it);
}

void LayoutManager::setViewWidget(gui::ViewWidget* viewWidget)
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  if (!viewWidget)
  {
    throw_debug("Null ViewWidget");
  }

  // Get the QSplitter to which this widget belongs:
  const auto it = m_impl->m_viewSplitters.find(viewWidget->getViewUid());
  if (std::end(m_impl->m_viewSplitters) == it)
  {
    std::ostringstream ss;
    ss << "View UID " << viewWidget->getViewUid() << " not found" << std::ends;
    std::cerr << ss.str() << std::endl;
    return;
  }

  /// @todo Put this in gui code: doesn't belong here
  if (QSplitter* splitter = it->second)
  {
    // Add the widget to the layout
    splitter->addWidget(viewWidget);
  }
  else
  {
    std::ostringstream ss;
    ss << "Splitter for view UID " << viewWidget->getViewUid() << " is null" << std::ends;
    std::cerr << ss.str() << std::endl;
  }
}

uid_range_t LayoutManager::getViewUids() const
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  return (m_impl->m_viewTypes | boost::adaptors::map_keys);
}

std::optional<gui::ViewType> LayoutManager::getViewType(const UID& viewUid) const
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  const auto it = m_impl->m_viewTypes.find(viewUid);
  if (std::end(m_impl->m_viewTypes) == it)
  {
    std::ostringstream ss;
    ss << "View UID " << viewUid << " not found" << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  return it->second;
}

std::list<UID> LayoutManager::getViewUidsOfType(const gui::ViewType& viewType) const
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  std::list<UID> viewUids;

  for (const auto& view : m_impl->m_viewTypes)
  {
    if (view.second == viewType)
    {
      viewUids.push_back(view.first);
    }
  }

  return viewUids;
}

view_type_range_t LayoutManager::getViewTypes() const
{
  if (!m_impl)
  {
    throw_debug("Null implementation");
  }

  return m_impl->m_viewTypes;
}

LayoutManager::Impl::Impl(const std::string& layoutConfig)
  : m_parsedLayouts()
  , m_orderedLayoutUids()
  , m_layoutData()
  , m_viewTypes()
  , m_viewSplitters()
{
  try
  {
    m_parsedLayouts = std::move(nlohmann::json::parse(layoutConfig));
  }
  catch (const std::exception& e)
  {
    std::ostringstream ss;
    ss << "Error parsing layout configuration from JSON:\n" << e.what() << std::ends;
    throw_debug(ss.str());
  }

  std::tie(m_layoutData, m_orderedLayoutUids, m_viewTypes, m_viewSplitters)
    = gui::makeLayouts(m_parsedLayouts /*layout::k_layoutTabs*/);
}
