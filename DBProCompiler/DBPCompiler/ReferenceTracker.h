#pragma once
#include <windows.h>
#include <vector>

class CEXEBlock;

/**
 * @file ReferenceTracker.h
 * @brief Forward-reference and label-to-offset tracker for x86 code generation.
 *
 * CReferenceTracker maintains parallel arrays of program references (byte
 * offsets into the machine code buffer) and their associated labels.  Labels
 * encode a type in the high byte and an index in the low 24 bits, which
 * UpdateMCBRefData unpacks into the CEXEBlock ref/type/index arrays for the
 * final executable.
 *
 * The storage auto-expands in 1024-entry increments when the write pointer
 * approaches the current capacity.
 *
 * Extracted from CASMWriter to isolate reference bookkeeping from code emission.
 *
 * @see CASMWriter   Original parent class that owns this tracker.
 * @see CEXEBlock     Final executable block that receives reference data.
 */
class CReferenceTracker
{
public:
    CReferenceTracker() noexcept;
    ~CReferenceTracker() = default;

    CReferenceTracker(const CReferenceTracker&) = delete;
    CReferenceTracker& operator=(const CReferenceTracker&) = delete;
    CReferenceTracker(CReferenceTracker&&) noexcept = default;
    CReferenceTracker& operator=(CReferenceTracker&&) noexcept = default;

    /** @brief Zeros all reference entries and resets the write pointer to the start. */
    void Reset() noexcept;

    /** @brief Expands the reference buffers by 1024 entries if within 100 of capacity.
     *  @return true if expansion occurred, false otherwise. */
    bool CheckAndExpandREFMemory();

    /** @brief Records a reference offset and its encoded label, expanding storage if needed.
     *  @param[in] dwRef       Byte offset into the machine code buffer.
     *  @param[in] dwRefLabel  Encoded label (high byte = type, low 24 bits = index). */
    void AddReference(DWORD dwRef, DWORD dwRefLabel);

    /** @brief Returns the current write pointer (number of references stored). */
    [[nodiscard]] DWORD GetRefPointer() const noexcept { return m_dwProgramRefPointer; }

    /** @brief Returns the current buffer capacity in entries. */
    [[nodiscard]] DWORD GetRefBufferSize() const noexcept { return m_dwRefBufferSize; }

    /** @brief Returns the reference offset at the given index, or 0 if out of range. */
    [[nodiscard]] DWORD GetRef(size_t index) const noexcept;

    /** @brief Returns the encoded reference label at the given index, or 0 if out of range. */
    [[nodiscard]] DWORD GetRefLabel(size_t index) const noexcept;

    /** @brief Returns an immutable view of the program reference offsets. */
    [[nodiscard]] const std::vector<DWORD>& GetProgramRefs() const noexcept { return m_ProgramRefs; }

    /** @brief Returns an immutable view of the program reference labels. */
    [[nodiscard]] const std::vector<DWORD>& GetProgramRefLabels() const noexcept { return m_ProgramRefLabels; }

    /** @brief Returns a mutable view of the program reference offsets. */
    [[nodiscard]] std::vector<DWORD>& GetProgramRefs() noexcept { return m_ProgramRefs; }

    /** @brief Returns a mutable view of the program reference labels. */
    [[nodiscard]] std::vector<DWORD>& GetProgramRefLabels() noexcept { return m_ProgramRefLabels; }

    /**
     * @brief Transfers accumulated references into the CEXEBlock arrays.
     *
     * Allocates or extends pEXE->m_pRefArray / m_pRefTypeArray / m_pRefIndexArray,
     * then unpacks each label into a type (high byte) and index (low 24 bits).
     *
     * @param[in,out] pEXE  Target executable block; arrays are (re)allocated on the EXE heap.
     * @return true on success, false if pEXE is null.
     */
    bool UpdateMCBRefData(CEXEBlock* pEXE);

private:
    DWORD m_dwRefBufferSize{ 1024 };
    DWORD m_dwProgramRefPointer{ 0 };
    std::vector<DWORD> m_ProgramRefs;
    std::vector<DWORD> m_ProgramRefLabels;
};
