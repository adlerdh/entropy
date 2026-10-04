#pragma once

#include <array>
#include <cstddef>
#include <mutex>

namespace rendering::deformation
{
enum class FieldBudgetUse : std::size_t
{
  AcceptedMaps,
  Candidate,
  Scratch,
  ImageTextures,
  Metrics,
  Readback,
  Count
};

/** @brief Shared byte budget for live GPU fields and host checkpoint transfer. */
class FieldWorkspace final
{
public:
  explicit FieldWorkspace(std::size_t capacityBytes);
  FieldWorkspace(const FieldWorkspace&) = delete;
  FieldWorkspace& operator=(const FieldWorkspace&) = delete;

  class Reservation final
  {
  public:
    Reservation() = default;
    ~Reservation();
    Reservation(Reservation&& other) noexcept;
    Reservation& operator=(Reservation&& other) noexcept;
    Reservation(const Reservation&) = delete;
    Reservation& operator=(const Reservation&) = delete;

  private:
    friend class FieldWorkspace;
    Reservation(FieldWorkspace& owner, FieldBudgetUse use, std::size_t bytes) noexcept;
    void release() noexcept;
    FieldWorkspace* m_owner = nullptr;
    FieldBudgetUse m_use = FieldBudgetUse::Scratch;
    std::size_t m_bytes = 0;
  };

  /** @brief Reserve a category atomically; throws bad_alloc without changing usage. */
  [[nodiscard]] Reservation reserve(FieldBudgetUse use, std::size_t bytes);
  [[nodiscard]] std::size_t capacityBytes() const noexcept;
  [[nodiscard]] std::size_t usedBytes() const;
  [[nodiscard]] std::size_t usedBytes(FieldBudgetUse use) const;

private:
  void release(FieldBudgetUse use, std::size_t bytes) noexcept;
  const std::size_t m_capacity;
  mutable std::mutex m_mutex;
  std::size_t m_used = 0;
  std::array<std::size_t, static_cast<std::size_t>(FieldBudgetUse::Count)> m_byUse{};
};
} // namespace rendering::deformation
