#include "ReferenceTracker.h"
#include "EXEBlock.h"
#include <memory>
#include <cstring>

CReferenceTracker::CReferenceTracker() noexcept
{
    m_ProgramRefs.resize(m_dwRefBufferSize, 0);
    m_ProgramRefLabels.resize(m_dwRefBufferSize, 0);
}

void CReferenceTracker::Reset() noexcept
{
    m_dwProgramRefPointer = 0;
    std::fill(m_ProgramRefs.begin(), m_ProgramRefs.end(), 0u);
    std::fill(m_ProgramRefLabels.begin(), m_ProgramRefLabels.end(), 0u);
}

bool CReferenceTracker::CheckAndExpandREFMemory()
{
    if (m_dwProgramRefPointer > m_dwRefBufferSize - 100)
    {
        DWORD dwNewSize = m_dwRefBufferSize + 1024;
        m_ProgramRefs.resize(dwNewSize, 0);
        m_ProgramRefLabels.resize(dwNewSize, 0);
        m_dwRefBufferSize = dwNewSize;
        return true;
    }
    return false;
}

void CReferenceTracker::AddReference(DWORD dwRef, DWORD dwRefLabel)
{
    CheckAndExpandREFMemory();
    if (m_dwProgramRefPointer < m_ProgramRefs.size())
    {
        m_ProgramRefs[m_dwProgramRefPointer] = dwRef;
        m_ProgramRefLabels[m_dwProgramRefPointer] = dwRefLabel;
        m_dwProgramRefPointer++;
    }
}

DWORD CReferenceTracker::GetRef(size_t index) const noexcept
{
    if (index < m_dwProgramRefPointer && index < m_ProgramRefs.size())
    {
        return m_ProgramRefs[index];
    }
    return 0;
}

DWORD CReferenceTracker::GetRefLabel(size_t index) const noexcept
{
    if (index < m_dwProgramRefPointer && index < m_ProgramRefLabels.size())
    {
        return m_ProgramRefLabels[index];
    }
    return 0;
}

bool CReferenceTracker::UpdateMCBRefData(CEXEBlock* pEXE)
{
    if (pEXE == nullptr)
    {
        return false;
    }

    DWORD dwStartAt = 0;
    DWORD dwFinishAt = 0;

    if (pEXE->m_pRefArray == nullptr)
    {
        DWORD dwNewSize = m_dwProgramRefPointer;
        LPSTR pNewArray1 = reinterpret_cast<LPSTR>(pEXE->CreateArray(dwNewSize));
        LPSTR pNewArray2 = reinterpret_cast<LPSTR>(pEXE->CreateArray(dwNewSize));
        LPSTR pNewArray3 = reinterpret_cast<LPSTR>(pEXE->CreateArray(dwNewSize));

        pEXE->m_dwNumberOfReferences = dwNewSize;
        pEXE->m_pRefArray = reinterpret_cast<DWORD*>(pNewArray1);
        pEXE->m_pRefTypeArray = reinterpret_cast<DWORD*>(pNewArray2);
        pEXE->m_pRefIndexArray = reinterpret_cast<DWORD*>(pNewArray3);

        dwStartAt = 0;
        dwFinishAt = pEXE->m_dwNumberOfReferences;
    }
    else
    {
        DWORD dwOldSize = pEXE->m_dwNumberOfReferences;
        std::unique_ptr<DWORD[]> pOldArray1(pEXE->m_pRefArray);
        std::unique_ptr<DWORD[]> pOldArray2(pEXE->m_pRefTypeArray);
        std::unique_ptr<DWORD[]> pOldArray3(pEXE->m_pRefIndexArray);

        DWORD dwNewSize = dwOldSize + m_dwProgramRefPointer;
        LPSTR pNewArray1 = reinterpret_cast<LPSTR>(pEXE->CreateArray(dwNewSize));
        LPSTR pNewArray2 = reinterpret_cast<LPSTR>(pEXE->CreateArray(dwNewSize));
        LPSTR pNewArray3 = reinterpret_cast<LPSTR>(pEXE->CreateArray(dwNewSize));

        if (pOldArray1 && pNewArray1)
            std::memcpy(pNewArray1, pOldArray1.get(), dwOldSize * sizeof(DWORD));
        if (pOldArray2 && pNewArray2)
            std::memcpy(pNewArray2, pOldArray2.get(), dwOldSize * sizeof(DWORD));
        if (pOldArray3 && pNewArray3)
            std::memcpy(pNewArray3, pOldArray3.get(), dwOldSize * sizeof(DWORD));

        pEXE->m_dwNumberOfReferences = dwNewSize;
        pEXE->m_pRefArray = reinterpret_cast<DWORD*>(pNewArray1);
        pEXE->m_pRefTypeArray = reinterpret_cast<DWORD*>(pNewArray2);
        pEXE->m_pRefIndexArray = reinterpret_cast<DWORD*>(pNewArray3);

        dwStartAt = dwOldSize;
        dwFinishAt = dwNewSize;
    }

    int iTokeniseCount = static_cast<int>(dwFinishAt - dwStartAt);
    for (int ref = 0; ref < iTokeniseCount; ref++)
    {
        DWORD dwEXERefDataIndex = dwStartAt + ref;
        if (dwEXERefDataIndex < pEXE->m_dwNumberOfReferences && static_cast<size_t>(ref) < m_ProgramRefs.size())
        {
            pEXE->m_pRefArray[dwEXERefDataIndex] = m_ProgramRefs[ref];
            DWORD dwRefLabel = m_ProgramRefLabels[ref];

            DWORD dwType = (dwRefLabel & 0xFF000000) >> 24;
            DWORD dwIndex = dwRefLabel & 0x00FFFFFF;

            pEXE->m_pRefTypeArray[dwEXERefDataIndex] = dwType;
            pEXE->m_pRefIndexArray[dwEXERefDataIndex] = dwIndex;
        }
    }

    return true;
}
