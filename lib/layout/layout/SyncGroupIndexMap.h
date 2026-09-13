#pragma once

#include <uuid.h>

#include <cstddef>
#include <optional>
#include <unordered_map>

namespace layout
{

/**
 * @brief Stable mapping between runtime sync group UIDs and serialized group indices.
 */
class SyncGroupIndexMap
{
public:
  using uuid = uuids::uuid;

  /**
   * @brief Return the serialized index for a runtime group UID.
   * @param groupUid Optional runtime group UID.
   * @return Existing or newly assigned index, or `std::nullopt` when `groupUid` is absent.
   */
  std::optional<std::size_t> indexForGroupUid(const std::optional<uuid>& groupUid);

  /**
   * @brief Return a runtime group UID for a serialized index.
   * @param groupIndex Optional serialized group index.
   * @return Runtime group UID, or `std::nullopt` when `groupIndex` is absent or unbound.
   */
  std::optional<uuid> groupUidForIndex(const std::optional<std::size_t>& groupIndex) const;

  /**
   * @brief Bind a serialized group index to a runtime group UID.
   * @param groupIndex Serialized group index.
   * @param groupUid Runtime group UID.
   * @return Existing UID when the index was already bound, otherwise `groupUid`.
   */
  uuid bindGroupIndex(std::size_t groupIndex, const uuid& groupUid);

private:
  std::unordered_map<uuid, std::size_t> m_indicesByUid; //!< Serialized indices by runtime UID
  std::unordered_map<std::size_t, uuid> m_uidsByIndex;  //!< Runtime UIDs by serialized index
};

} // namespace layout
