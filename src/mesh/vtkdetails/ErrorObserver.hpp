#include <vtkSmartPointer.h>
#include <vtkCommand.h>

#include <string>

class ErrorObserver : public vtkCommand
{
public:

    ErrorObserver()
        :
          m_error( false ),
          m_warning( false ),
          m_errorMessage(),
          m_warningMessage()
    {}

    static ErrorObserver* New()
    {
        return new ErrorObserver;
    }

    bool GetError() const
    {
        return m_error;
    }

    bool GetWarning() const
    {
        return m_warning;
    }

    void Clear()
    {
        m_error = false;
        m_warning = false;
        m_errorMessage = "";
        m_warningMessage = "";
    }

    virtual void Execute(
            vtkObject *vtkNotUsed(caller),
            unsigned long event,
            void *calldata)
    {
        switch ( event )
        {
        case vtkCommand::ErrorEvent:
        {
            m_errorMessage = static_cast<char*>( calldata );
            m_error = true;
            break;
        }

        case vtkCommand::WarningEvent:
        {
            m_warningMessage = static_cast<char*>( calldata );
            m_warning = true;
            break;
        }
        }
    }

    const std::string& GetErrorMessage() const
    {
        return m_errorMessage;
    }

    const std::string& GetWarningMessage() const
    {
        return m_warningMessage;
    }


private:

    bool m_error;
    bool m_warning;
    std::string m_errorMessage;
    std::string m_warningMessage;
};
