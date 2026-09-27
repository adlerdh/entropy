#include "viewer/CameraSyncGroups.h"

#include "common/UuidUtility.h"

#include <algorithm>
#include <utility>

namespace viewer
{

CameraSyncGroups::CameraSyncGroups()
  : m_groups{{CameraSyncMode::Rotation, {}}, {CameraSyncMode::Translation, {}}, {CameraSyncMode::Zoom, {}}}
{
}

CameraSyncGroups::uuid CameraSyncGroups::addGroup(CameraSyncMode mode)
{
  auto& groups = groupsFor(mode);

  uuid newUid = generateRandomUuid();
  while (!groups.try_emplace(newUid).second) {
    newUid = generateRandomUuid();
  }
  return newUid;
}

const CameraSyncGroups::Group* CameraSyncGroups::group(CameraSyncMode mode, const uuid& groupUid) const
{
  const auto* groups = findGroupsFor(mode);
  if (!groups) {
    return nullptr;
  }

  const auto it = groups->find(groupUid);
  return it != groups->end() ? &it->second : nullptr;
}

CameraSyncGroups::Group* CameraSyncGroups::group(CameraSyncMode mode, const uuid& groupUid)
{
  auto& groups = groupsFor(mode);
  const auto it = groups.find(groupUid);
  return it != groups.end() ? &it->second : nullptr;
}

std::optional<CameraSyncGroups::uuid> CameraSyncGroups::groupUidContainingView(CameraSyncMode mode, const uuid& viewUid)
  const
{
  const auto* groups = findGroupsFor(mode);
  if (!groups) {
    return std::nullopt;
  }

  for (const auto& [groupUid, viewUids] : *groups) {
    if (std::find(viewUids.begin(), viewUids.end(), viewUid) != viewUids.end()) {
      return groupUid;
    }
  }

  return std::nullopt;
}

void CameraSyncGroups::addViewToGroup(CameraSyncMode mode, const std::optional<uuid>& groupUid, const uuid& viewUid)
{
  if (!groupUid) {
    return;
  }

  auto& groups = groupsFor(mode);
  const auto target = groups.find(*groupUid);
  if (target == groups.end()) {
    return;
  }

  // A view may belong to at most one group for a given synchronization mode.
  // Enforcing that invariant here prevents an interaction from being applied
  // more than once and keeps groupUidContainingView deterministic.
  for (auto& [uid, viewUids] : groups) {
    if (uid != *groupUid) {
      viewUids.remove(viewUid);
    }
  }

  if (std::find(target->second.begin(), target->second.end(), viewUid) == target->second.end()) {
    target->second.push_back(viewUid);
  }
}

CameraSyncGroups::GroupMap& CameraSyncGroups::groupsFor(CameraSyncMode mode)
{
  return m_groups.try_emplace(mode).first->second;
}

const CameraSyncGroups::GroupMap* CameraSyncGroups::findGroupsFor(CameraSyncMode mode) const
{
  const auto it = m_groups.find(mode);
  return it != m_groups.end() ? &it->second : nullptr;
}

} // namespace viewer
