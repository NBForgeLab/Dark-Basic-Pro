#pragma once
// StatementHelper.h - Pure string utility functions extracted from CStatement
// These functions have no dependencies on global state

#include <windows.h>
#include "Str.h"

namespace StatementHelper {
    // Separates initialization value from type in declarations
    // Modifies pPossibleTypeAndInit in place (truncates at '=')
    // Returns heap-allocated copy of init value (caller owns, free with delete[])
    // Returns nullptr if no assignment found
    [[nodiscard]] LPSTR SeperateInitFromType(LPSTR pPossibleTypeAndInit);

    // Separates array value from array string
    // Extracts value inside (...) into *pArrValue (caller owns, free with delete[])
    // Replaces *pArrayString with trimmed name (caller owns, free with delete[])
    bool SeperateValueFromArrayString(LPSTR* pArrayString, LPSTR* pArrValue, bool bMustBeLiteralDim);

    // Checks if string contains assignment operator with valid L-value
    [[nodiscard]] bool ContainsAssignmentOperator(CStr* pString);
}
