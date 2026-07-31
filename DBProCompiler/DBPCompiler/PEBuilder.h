#pragma once
#include <windows.h>

class CASMWriter;

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
    [[nodiscard]] bool ValidatePEHeaderRequirements(DWORD dwImageBase, DWORD dwSectionAlignment, DWORD dwFileAlignment) const noexcept;
    [[nodiscard]] bool BuildExecutable(const char* pEXEFilename) const noexcept;
    [[nodiscard]] bool BuildEXEPackage(const char* pEXEFilename, bool bParsingMainProgram, bool bGotNewCode) const noexcept;
    [[nodiscard]] bool BuildEXEPackage(CASMWriter* pASMWriter, const char* pEXEFilename, bool bParsingMainProgram, bool bGotNewCode) const;

private:
    bool m_bPrepared{ false };
    DWORD m_dwHeaderSize{ 0 };
};
