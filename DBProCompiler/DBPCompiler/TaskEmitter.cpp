#include "TaskEmitter.h"

DWORD CTaskEmitter::DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept
{
    // First Determine SizeCode From Type
    DWORD dwAddressSizeCode = 0;
    switch (dwTypeValue)
    {
        case 4:  dwAddressSizeCode = 0; break;  // BYTE
        case 5:  dwAddressSizeCode = 0; break;  // BYTE
        case 6:  dwAddressSizeCode = 1; break;  // WORD
        case 8:  dwAddressSizeCode = 3; break;  // DWORDx2
        case 9:  dwAddressSizeCode = 3; break;  // DWORDx2
        default: dwAddressSizeCode = 2; break;  // DWORD (default)
    }

    return dwAddressSizeCode;
}

DWORD CTaskEmitter::CalculateTaskPassOffset(DWORD dwPassNumber, DWORD dwBaseOffset) const noexcept
{
    return dwPassNumber * dwBaseOffset;
}

bool CTaskEmitter::EmitCoreTask(DWORD dwLine, DWORD dwTask) const noexcept
{
    if (dwLine == 0) return false;
    return true;
}

bool CTaskEmitter::EmitCoreTask(DWORD dwLine, DWORD dwTask, DWORD dwP1Mode, DWORD dwP2Mode, DWORD dwP3Mode) const noexcept
{
    if (dwLine == 0) return false;
    return true;
}

bool CTaskEmitter::EmitTask(CASMWriter* pASMWriter, DWORD dwLine, DWORD dwTask) const
{
    if (!pASMWriter) return false;
    return EmitCoreTask(dwLine, dwTask);
}
