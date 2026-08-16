// StatementHelper.cpp - Pure string utility functions extracted from CStatement
// These functions have no dependencies on global state

#include "StatementHelper.h"
#include <memory>
#include <cstring>

namespace StatementHelper {

LPSTR SeperateInitFromType(LPSTR pPossibleTypeAndInit)
{
    if(pPossibleTypeAndInit)
    {
        DWORD length = static_cast<DWORD>(strlen(pPossibleTypeAndInit));
        for(DWORD n=0; n<length; n++)
        {
            if(pPossibleTypeAndInit[n]=='=')
            {
                // Modify string so it only shows type name
                pPossibleTypeAndInit[n++]=0;

                // Extract init value (stack CStr: RAII on every exit path)
                CStr initValue("");
                for(; n<length; n++)
                {
                    initValue.AddChar(pPossibleTypeAndInit[n]);
                }
                initValue.EatEdgeSpacesandTabs(nullptr);

                // Hand a heap char[] back to the caller-owned raw contract
                auto pInitString = std::make_unique<char[]>(initValue.Length()+1);
                std::memcpy(pInitString.get(), initValue.GetStr(), initValue.Length() + 1);
                return pInitString.release();
            }
        }
    }
    return nullptr;
}

bool SeperateValueFromArrayString(LPSTR* pArrayString, LPSTR* pArrValue, [[maybe_unused]] bool bMustBeLiteralDim)
{
    if (!pArrayString || !*pArrayString || !pArrValue)
        return false;

    // Result
    bool bResult=false;

    // Create and clean string
    CStr pString(*pArrayString);
    pString.EatEdgeSpacesandTabs(nullptr);

    // Find Where bracket value starts
    DWORD dwPos = pString.FindFirstChar('(');
    if(dwPos>0)
    {
        // Skip open bracket
        dwPos++;

        // Get string length
        DWORD length = pString.Length();

        // Skip close bracket
        length--;

        // if length less than position, array (..) format incomplete
        if ( dwPos <= length )
        {
            // Extract out value (hand a heap char[] back to the caller-owned raw contract)
            auto pValueBuffer = std::make_unique<char[]>((length-dwPos)+1);
            DWORD n = 0;
            for(n=0; n<length-dwPos; n++)
                pValueBuffer[n]=pString.GetChar(dwPos+n);
            pValueBuffer[n]=0;
            *pArrValue = pValueBuffer.release();

            // Shorten array string so just name is showing
            pString.SetChar(dwPos-1, 0);
            pString.EatEdgeSpacesandTabs(nullptr);

            // Replace the incoming name buffer: adopt the old one with
            // unique_ptr<char[]> so it is released with delete[] (the legacy
            // scalar SAFE_DELETE was an array-new/scalar-delete mismatch), then
            // hand a fresh heap char[] back to the caller-owned raw contract.
            std::unique_ptr<char[]> pOldName(*pArrayString);
            auto pNewName = std::make_unique<char[]>(pString.Length()+1);
            std::memcpy(pNewName.get(), pString.GetStr(), pString.Length() + 1);
            *pArrayString = pNewName.release();

            // Success
            bResult=true;
        }
    }

    // Complete
    return bResult;
}

bool ContainsAssignmentOperator(CStr* pString)
{
    if (!pString)
        return false;

    bool bResult=false;
    DWORD dwPos = pString->FindFirstChar('=');
    if(dwPos>0)
    {
        // GetLeftOfPosition hands back a new char[]; adopt it with
        // unique_ptr<char[]> so it is released with delete[].
        std::unique_ptr<char[]> pLeft(pString->GetLeftOfPosition(dwPos));
        CStr lStr(pLeft.get());
        if(lStr.IsTextLValue()) bResult=true;
    }
    return bResult;
}

} // namespace StatementHelper
