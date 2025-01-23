#ifndef ENTROPY_EXCEPTION
#define ENTROPY_EXCEPTION

#include <sstream>
#include <stdexcept>

/**
 * @brief A friendly wrapper around \c std::runtime_error that prints the file name,
 * function name, and line number on which the exception occurred to stdout.
 * The C-style definition \c throw_debug is to be used by clients of this class.
 */
class Exception : public std::runtime_error
{
public:

    Exception(const char* msg, const char* file, const char* function, int line)
        : std::runtime_error( msg )
    {
        std::ostringstream ss;
        ss << "[in function '" << function << "'; file '" << file << "' : line " << line << "] " << msg;
        m_msg = ss.str();
    }

    Exception(const std::string& msg, const char* file, const char* function, int line)
        : Exception( msg.c_str(), file, function, line )
    {}

    virtual ~Exception() = default;

    const char* what() const noexcept
    {
        return m_msg.c_str();
    }

private:

    std::string m_msg;
};

/// @todo use https://en.cppreference.com/w/cpp/utility/source_location
#define throw_debug(msg) throw Exception(msg, __FILE__, __FUNCTION__, __LINE__);

#endif // ENTROPY_EXCEPTION
