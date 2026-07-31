#include "PEBuilder.h"

void CPEBuilder::Reset() noexcept
{
    m_bPrepared = false;
    m_dwHeaderSize = 0;
}

DWORD CPEBuilder::CalculateAlignedSize(DWORD dwUnalignedSize, DWORD dwAlignment) const noexcept
{
    if (dwAlignment == 0) return dwUnalignedSize;
    DWORD dwRemainder = dwUnalignedSize % dwAlignment;
    if (dwRemainder == 0) return dwUnalignedSize;
    return dwUnalignedSize + (dwAlignment - dwRemainder);
}
