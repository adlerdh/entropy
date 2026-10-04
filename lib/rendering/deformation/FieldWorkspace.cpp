#include "rendering/deformation/FieldWorkspace.h"

#include <new>
#include <stdexcept>
#include <utility>

namespace rendering::deformation
{
FieldWorkspace::FieldWorkspace(const std::size_t capacityBytes) : m_capacity(capacityBytes) {}

FieldWorkspace::Reservation::Reservation(FieldWorkspace& owner, FieldBudgetUse use, std::size_t bytes) noexcept
  : m_owner(&owner), m_use(use), m_bytes(bytes)
{
}
FieldWorkspace::Reservation::~Reservation()
{
  release();
}
FieldWorkspace::Reservation::Reservation(Reservation&& other) noexcept
  : m_owner(std::exchange(other.m_owner, nullptr)), m_use(other.m_use), m_bytes(std::exchange(other.m_bytes, 0))
{
}
FieldWorkspace::Reservation& FieldWorkspace::Reservation::operator=(Reservation&& other) noexcept
{
  if (this != &other) {
    release();
    m_owner = std::exchange(other.m_owner, nullptr);
    m_use = other.m_use;
    m_bytes = std::exchange(other.m_bytes, 0);
  }
  return *this;
}
void FieldWorkspace::Reservation::release() noexcept
{
  if (m_owner) m_owner->release(m_use, m_bytes);
  m_owner = nullptr;
  m_bytes = 0;
}

FieldWorkspace::Reservation FieldWorkspace::reserve(FieldBudgetUse use, const std::size_t bytes)
{
  const auto index = static_cast<std::size_t>(use);
  if (index >= m_byUse.size()) throw std::invalid_argument("Unknown field budget category");
  std::lock_guard lock(m_mutex);
  if (bytes > m_capacity - m_used) throw std::bad_alloc();
  m_used += bytes;
  m_byUse[index] += bytes;
  return Reservation(*this, use, bytes);
}
std::size_t FieldWorkspace::capacityBytes() const noexcept
{
  return m_capacity;
}
std::size_t FieldWorkspace::usedBytes() const
{
  std::lock_guard lock(m_mutex);
  return m_used;
}
std::size_t FieldWorkspace::usedBytes(FieldBudgetUse use) const
{
  const auto index = static_cast<std::size_t>(use);
  if (index >= m_byUse.size()) return 0;
  std::lock_guard lock(m_mutex);
  return m_byUse[index];
}
void FieldWorkspace::release(FieldBudgetUse use, const std::size_t bytes) noexcept
{
  std::lock_guard lock(m_mutex);
  m_used -= bytes;
  m_byUse[static_cast<std::size_t>(use)] -= bytes;
}
} // namespace rendering::deformation
