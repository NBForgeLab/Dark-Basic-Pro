#pragma once
#include <windows.h>
#include <vector>

class CEXEBlock;

class CReferenceTracker
{
public:
    CReferenceTracker() noexcept;
    ~CReferenceTracker() = default;

    CReferenceTracker(const CReferenceTracker&) = delete;
    CReferenceTracker& operator=(const CReferenceTracker&) = delete;
    CReferenceTracker(CReferenceTracker&&) noexcept = default;
    CReferenceTracker& operator=(CReferenceTracker&&) noexcept = default;

    void Reset() noexcept;
    bool CheckAndExpandREFMemory();
    void AddReference(DWORD dwRef, DWORD dwRefLabel);

    [[nodiscard]] DWORD GetRefPointer() const noexcept { return m_dwProgramRefPointer; }
    [[nodiscard]] DWORD GetRefBufferSize() const noexcept { return m_dwRefBufferSize; }

    [[nodiscard]] DWORD GetRef(size_t index) const noexcept;
    [[nodiscard]] DWORD GetRefLabel(size_t index) const noexcept;

    [[nodiscard]] const std::vector<DWORD>& GetProgramRefs() const noexcept { return m_ProgramRefs; }
    [[nodiscard]] const std::vector<DWORD>& GetProgramRefLabels() const noexcept { return m_ProgramRefLabels; }
    [[nodiscard]] std::vector<DWORD>& GetProgramRefs() noexcept { return m_ProgramRefs; }
    [[nodiscard]] std::vector<DWORD>& GetProgramRefLabels() noexcept { return m_ProgramRefLabels; }

    bool UpdateMCBRefData(CEXEBlock* pEXE);

private:
    DWORD m_dwRefBufferSize{ 1024 };
    DWORD m_dwProgramRefPointer{ 0 };
    std::vector<DWORD> m_ProgramRefs;
    std::vector<DWORD> m_ProgramRefLabels;
};
