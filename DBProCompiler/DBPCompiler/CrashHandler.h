#pragma once

#include <windows.h>
#include <exception>
#include <string>

namespace db3 {

// Structured Exception to C++ exception translator
class CSEHException : public std::exception {
private:
    unsigned int m_code;
public:
    CSEHException(unsigned int code) : m_code(code) {}
    unsigned int GetCode() const { return m_code; }
    virtual const char* what() const noexcept override {
        switch (m_code) {
            case EXCEPTION_ACCESS_VIOLATION: return "SEH Exception: Access Violation (0xC0000005)";
            case EXCEPTION_INT_DIVIDE_BY_ZERO: return "SEH Exception: Integer Divide By Zero (0xC0000094)";
            case EXCEPTION_STACK_OVERFLOW: return "SEH Exception: Stack Overflow (0xC00000FD)";
            default: return "SEH Exception: Unknown structured exception";
        }
    }
};

void SetupDiagnosticHandlers();

} // namespace db3
