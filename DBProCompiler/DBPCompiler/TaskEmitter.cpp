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
