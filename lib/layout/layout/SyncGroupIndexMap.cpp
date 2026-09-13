#include "layout/SyncGroupIndexMap.h"

namespace layout
{

std::optional<std::size_t> SyncGroupIndexMap::indexForGroupUid(const std::optional<uuid>& groupUid)
{
  if (!groupUid) {
    return std::nullopt;
  }

  const auto existing = m_indicesByUid.find(*groupUid);
  if (existing != m_indicesByUid.end()) {
    return existing->second;
  }

  const std::size_t index = m_indicesByUid.size();
  m_indicesByUid.emplace(*groupUid, index);
  m_uidsByIndex.emplace(index, *groupUid);
  return index;
}

std::optional<SyncGroupIndexMap::uuid> SyncGroupIndexMap::groupUidForIndex(
  const std::optional<std::size_t>& groupIndex) const
{
  if (!groupIndex) {
    return std::nullopt;
  }

  const auto existing = m_uidsByIndex.find(*groupIndex);
  if (existing == m_uidsByIndex.end()) {
    return std::nullopt;
  }
  return existing->second;
}

SyncGroupIndexMap::uuid SyncGroupIndexMap::bindGroupIndex(std::size_t groupIndex, const uuid& groupUid)
{
  const auto [it, inserted] = m_uidsByIndex.emplace(groupIndex, groupUid);
  if (inserted) {
    m_indicesByUid.emplace(groupUid, groupIndex);
  }
  return it->second;
}

} // namespace layout
