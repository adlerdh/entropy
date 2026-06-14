#pragma once

#include <cstddef>

/**
 * @brief Template class for counting the number of objects of type T created and
 * currently allocated (a.k.a. the number of objects that are "alive").
 */
template<class T>
class ObjectCounter
{
public:
  ObjectCounter()
  {
    ++m_numCreated;
    ++m_numAlive;
  }

  ObjectCounter(const ObjectCounter&)
  {
    ++m_numCreated;
    ++m_numAlive;
  }

  /// Return the number of objects of type @c T ever constructed.
  std::size_t numCreated() const noexcept
  {
    return m_numCreated;
  }

  /// Return the number of objects of type @c T currently alive.
  std::size_t numAlive() const noexcept
  {
    return m_numAlive;
  }

protected:
  /// @note Objects should never be removed through pointers of this type
  ~ObjectCounter()
  {
    --m_numAlive;
  }

private:
  static std::size_t m_numCreated;
  static std::size_t m_numAlive;
};

template<class T>
std::size_t ObjectCounter<T>::m_numCreated = 0;
template<class T>
std::size_t ObjectCounter<T>::m_numAlive = 0;
