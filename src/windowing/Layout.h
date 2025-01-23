#ifndef LAYOUT_H
#define LAYOUT_H

#include "windowing/ControlFrame.h"
#include "windowing/View.h"

#include <uuid.h>

#include <list>
#include <memory>
#include <unordered_map>

/// @brief Represents a set of views rendered together in the window at one time
class Layout : public ControlFrame
{
public:
  Layout(bool isLightbox);

  void setImageRendered(const AppData& appData, size_t index, bool visible) override;
  void setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults) override;

  void setMetricImages(const std::list<uuids::uuid>& imageUids) override;
  void setImageUsedForMetric(const AppData& appData, size_t index, bool visible) override;

  void updateImageOrdering(uuid_range_t orderedImageUids) override;

  void setViewType(const ViewType& viewType) override;
  void setRenderMode(const camera::ViewRenderMode& renderMode) override;
  void setIntensityProjectionMode(const camera::IntensityProjectionMode& renderMode) override;

  const uuids::uuid& uid() const;
  bool isLightbox() const;

  void recenter();

  std::unordered_map<uuids::uuid, std::shared_ptr<View> >& views();
  const std::unordered_map<uuids::uuid, std::shared_ptr<View> >& views() const;

  std::unordered_map<uuids::uuid, std::list<uuids::uuid> >& cameraRotationSyncGroups();
  const std::unordered_map<uuids::uuid, std::list<uuids::uuid> >& cameraRotationSyncGroups() const;

  std::unordered_map<uuids::uuid, std::list<uuids::uuid> >& cameraTranslationSyncGroups();
  const std::unordered_map<uuids::uuid, std::list<uuids::uuid> >& cameraTranslationSyncGroups() const;

  std::unordered_map<uuids::uuid, std::list<uuids::uuid> >& cameraZoomSyncGroups();
  const std::unordered_map<uuids::uuid, std::list<uuids::uuid> >& cameraZoomSyncGroups() const;

private:
  void updateAllViewsInLayout();

  uuids::uuid m_uid;

  /// Views of the layout, keyed by their UID
  std::unordered_map<uuids::uuid, std::shared_ptr<View> > m_views;

  /// For each synchronization type, a map of synchronization group UID to the list of view UIDs in the group
  std::unordered_map<uuids::uuid, std::list<uuids::uuid> > m_cameraRotationSyncGroups;
  std::unordered_map<uuids::uuid, std::list<uuids::uuid> > m_cameraTranslationSyncGroups;
  std::unordered_map<uuids::uuid, std::list<uuids::uuid> > m_cameraZoomSyncGroups;

  /// If true, then this layout has UI controls that affect all of its views,
  /// rather than each view having its own UI controls
  bool m_isLightbox;
};

#endif // LAYOUT_H
