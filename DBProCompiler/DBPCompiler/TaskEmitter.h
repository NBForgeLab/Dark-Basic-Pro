#pragma once
#include <windows.h>

class CTaskEmitter
{
public:
    CTaskEmitter() noexcept = default;
    ~CTaskEmitter() = default;

    CTaskEmitter(const CTaskEmitter&) = delete;
    CTaskEmitter& operator=(const CTaskEmitter&) = delete;
    CTaskEmitter(CTaskEmitter&&) noexcept = default;
    CTaskEmitter& operator=(CTaskEmitter&&) noexcept = default;

    void Reset() noexcept { m_dwTaskCount = 0; }
    void IncrementTaskCount() noexcept { m_dwTaskCount++; }
    [[nodiscard]] DWORD GetTaskCount() const noexcept { return m_dwTaskCount; }

    [[nodiscard]] DWORD DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept;

private:
    DWORD m_dwTaskCount{ 0 };
};
