#pragma once
#include <windows.h>

class CPEBuilder
{
public:
    CPEBuilder() noexcept = default;
    ~CPEBuilder() = default;

    CPEBuilder(const CPEBuilder&) = delete;
    CPEBuilder& operator=(const CPEBuilder&) = delete;
    CPEBuilder(CPEBuilder&&) noexcept = default;
    CPEBuilder& operator=(CPEBuilder&&) noexcept = default;

    void Reset() noexcept;
    void SetPrepared(bool bPrepared) noexcept { m_bPrepared = bPrepared; }
    [[nodiscard]] bool IsPrepared() const noexcept { return m_bPrepared; }

    void SetHeaderSize(DWORD dwSize) noexcept { m_dwHeaderSize = dwSize; }
    [[nodiscard]] DWORD GetHeaderSize() const noexcept { return m_dwHeaderSize; }

    [[nodiscard]] DWORD CalculateAlignedSize(DWORD dwUnalignedSize, DWORD dwAlignment) const noexcept;

private:
    bool m_bPrepared{ false };
    DWORD m_dwHeaderSize{ 0 };
};
