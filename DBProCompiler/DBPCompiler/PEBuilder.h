#pragma once
#include <windows.h>
#include "RuntimeDllTable.h"

/**
 * @file PEBuilder.h
 * @brief Subsystem for executable packaging, PE header validation, and data section updates.
 *
 * CPEBuilder manages PE header checks, memory alignment calculations, and
 * updates for compiler table data (DLLs, commands, strings, dynamic arrays, structures).
 */
class CPEBuilder
{
public:
    static constexpr DWORD RuntimeDllCapacity = dbp::runtime::DllCapacity;

    CPEBuilder() noexcept = default;
    ~CPEBuilder() = default;

    CPEBuilder(const CPEBuilder&) = delete;
    CPEBuilder& operator=(const CPEBuilder&) = delete;
    CPEBuilder(CPEBuilder&&) noexcept = default;
    CPEBuilder& operator=(CPEBuilder&&) noexcept = default;

    /** @brief Resets PE builder state. */
    void Reset() noexcept;

    /** @brief Sets prepared status flag. */
    void SetPrepared(bool bPrepared) noexcept { m_bPrepared = bPrepared; }

    /** @brief Returns true if PE headers are prepared. */
    [[nodiscard]] bool IsPrepared() const noexcept { return m_bPrepared; }

    /** @brief Sets PE header size. */
    void SetHeaderSize(DWORD dwSize) noexcept { m_dwHeaderSize = dwSize; }

    /** @brief Returns PE header size. */
    [[nodiscard]] DWORD GetHeaderSize() const noexcept { return m_dwHeaderSize; }

    /** @brief Calculates memory size aligned to target section/file alignment. */
    [[nodiscard]] DWORD CalculateAlignedSize(DWORD dwUnalignedSize, DWORD dwAlignment) const noexcept;

    /** @brief Validates PE image base, section alignment, and file alignment constraints. */
    [[nodiscard]] bool ValidatePEHeaderRequirements(DWORD dwImageBase, DWORD dwSectionAlignment, DWORD dwFileAlignment) const noexcept;

    /** @brief Validates PE32+ (64-bit) image base, section alignment, and file alignment constraints. */
    [[nodiscard]] bool ValidatePE64HeaderRequirements(uint64_t dwImageBase, DWORD dwSectionAlignment, DWORD dwFileAlignment) const noexcept;

    /** @brief Returns PE header magic value for 32-bit (0x010B) or 64-bit (0x020B). */
    [[nodiscard]] static constexpr WORD GetPeMagic(bool is64Bit) noexcept
    {
        return is64Bit ? IMAGE_NT_OPTIONAL_HDR64_MAGIC : IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    }

    /** @brief Returns whether an encoded DLL id can index the legacy runtime dispatch tables. */
    [[nodiscard]] static constexpr bool IsRuntimeDllIndex(DWORD index) noexcept
    {
        return dbp::runtime::IsDllIndex(index);
    }

    /** @brief Updates DLL index and filename tables for executable. */
    [[nodiscard]] bool UpdateDLLData() const;

    /** @brief Updates command DLL ID and call string tables for executable. */
    [[nodiscard]] bool UpdateCommandData() const;

    /** @brief Updates string table data for executable. */
    [[nodiscard]] bool UpdateStringData() const;

    /** @brief Updates DATA section data items for executable. */
    [[nodiscard]] bool UpdateDataData() const;

    /** @brief Updates dynamic variable arrays for executable. */
    [[nodiscard]] bool UpdateDynamicData() const;

    /** @brief Updates user-defined structure pattern string array for executable. */
    [[nodiscard]] bool UpdateStructurePatternData() const;

private:
    bool m_bPrepared{ false };
    DWORD m_dwHeaderSize{ 0 };
};
