#pragma once

#include "viewer/LayoutTypes.h"

#include <uuid.h>

#include <list>
#include <optional>
#include <unordered_map>

namespace viewer
{

/**
 * @brief Camera synchronization groups keyed by sync mode and group UID.
 */
class CameraSyncGroups
{
public:
  using uuid = uuids::uuid;
  using Group = std::list<uuid>;

  CameraSyncGroups();

  /**
   * @brief Create an empty group for a sync mode.
   *
   * @param mode Camera property synchronized by the group.
   * @return UID of the new group.
   */
  uuid addGroup(CameraSyncMode mode);

  /**
   * @brief Return a group, or nullptr when it does not exist.
   *
   * @param mode Camera property synchronized by the group.
   * @param groupUid Group UID to find.
   * @return Group pointer, or nullptr.
   */
  const Group* group(CameraSyncMode mode, const uuid& groupUid) const;
  Group* group(CameraSyncMode mode, const uuid& groupUid);

  /**
   * @brief Find the group for a sync mode that contains a view.
   *
   * @param mode Camera property synchronized by the group.
   * @param viewUid View UID whose membership should be found.
   * @return Group UID when the view is a member.
   */
  std::optional<uuid> groupUidContainingView(CameraSyncMode mode, const uuid& viewUid) const;

  /**
   * @brief Add a view to a group if the group UID is present and exists.
   *
   * @param mode Camera property synchronized by the group.
   * @param groupUid Optional group UID to join.
   * @param viewUid View UID to add.
   */
  void addViewToGroup(CameraSyncMode mode, const std::optional<uuid>& groupUid, const uuid& viewUid);

private:
  using GroupMap = std::unordered_map<uuid, Group>;

  GroupMap& groupsFor(CameraSyncMode mode);
  const GroupMap* findGroupsFor(CameraSyncMode mode) const;

  std::unordered_map<CameraSyncMode, GroupMap> m_groups; //!< Groups by sync mode.
};

} // namespace viewer
