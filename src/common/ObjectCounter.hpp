#ifndef OBJECT_COUNTER_H
#define OBJECT_COUNTER_H

#include <cstddef>


/**
 * @brief Template class for couynting the number of objects of type T created and
 * currently allocated (a.k.a. the number of objects that are "alive").
 */
template< class T >
class ObjectCounter
{
public:

    ObjectCounter()
    {
        ++m_numCreated;
        ++m_numAlive;
    }

    ObjectCounter( const ObjectCounter& )
    {
        ++m_numCreated;
        ++m_numAlive;
    }

    std::size_t numCreated() const noexcept
    {
        return m_numCreated;
    }

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


template< class T > std::size_t ObjectCounter<T>::m_numCreated = 0;
template< class T > std::size_t ObjectCounter<T>::m_numAlive = 0;

#endif // OBJECT_COUNTER_H
