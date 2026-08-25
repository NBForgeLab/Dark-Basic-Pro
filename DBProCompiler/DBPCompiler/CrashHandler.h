#pragma once

#include <exception>
#include <string>

namespace db3 {

// Win32 SEH exception codes. Defined locally as stable ABI constants so this
// header does not have to pull in the heavy <windows.h> just for three integer
// literals. Values are fixed by the Windows ABI and never change.
inline constexpr unsigned int SEH_ACCESS_VIOLATION   = 0xC0000005u;
inline constexpr unsigned int SEH_INT_DIVIDE_BY_ZERO = 0xC0000094u;
inline constexpr unsigned int SEH_STACK_OVERFLOW     = 0xC00000FDu;

// Structured Exception to C++ exception translator
class CSEHException : public std::exception {
private:
    unsigned int m_code;
public:
    CSEHException(unsigned int code) : m_code(code) {}
    unsigned int GetCode() const { return m_code; }
    virtual const char* what() const noexcept override {
        switch (m_code) {
            case SEH_ACCESS_VIOLATION:   return "SEH Exception: Access Violation (0xC0000005)";
            case SEH_INT_DIVIDE_BY_ZERO: return "SEH Exception: Integer Divide By Zero (0xC0000094)";
            case SEH_STACK_OVERFLOW:     return "SEH Exception: Stack Overflow (0xC00000FD)";
            default: return "SEH Exception: Unknown structured exception";
        }
    }
};

void SetupDiagnosticHandlers();

} // namespace db3
