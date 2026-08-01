#pragma once

#include <windows.h>
#include "Str.h"

/**
 * @file StatementHelper.h
 * @brief Pure string utility functions for declaration and array parsing.
 *
 * The StatementHelper namespace contains stateless helper functions extracted
 * from CStatement.  They have no dependencies on global compiler state and
 * operate only on their input arguments.
 *
 * @note Memory ownership: Functions that return `LPSTR` allocate with `new[]`.
 *       The caller owns the returned pointer and must free it with `delete[]`.
 *
 * Extracted from CStatement to isolate string manipulation from statement parsing.
 *
 * @see CStatement  Original parent class that calls these helpers.
 */
namespace StatementHelper {

    /**
     * @brief Separates an initializer value from a type declaration string.
     *
     * Given a string like `"INTEGER=42"`, truncates the input at `'='` (leaving
     * `"INTEGER"`) and returns a copy of the initializer (`"42"`).
     *
     * @param[in,out] pPossibleTypeAndInit  Type string; truncated in place at the `'='`.
     * @return Heap-allocated copy of the initializer value (caller owns, free with `delete[]`).
     *         Returns nullptr if no assignment operator was found.
     */
    [[nodiscard]] LPSTR SeperateInitFromType(LPSTR pPossibleTypeAndInit);

    /**
     * @brief Separates an array dimension value from an array declaration string.
     *
     * Given a pointer to `"myArray(10)"`, extracts `"10"` into *pArrValue and
     * replaces *pArrayString with `"myArray"`.
     *
     * @param[in,out] pArrayString      Pointer to the array name+dims string; replaced with
     *                                  a heap-allocated name-only copy (caller owns, free with `delete[]`).
     *                                  The original pointer is freed internally.
     * @param[out]    pArrValue         Receives a heap-allocated copy of the dimension value
     *                                  (caller owns, free with `delete[]`).
     * @param[in]     bMustBeLiteralDim Reserved for future enforcement of literal dimensions.
     * @return true if a bracketed value was found and extracted, false otherwise.
     */
    bool SeperateValueFromArrayString(LPSTR* pArrayString, LPSTR* pArrValue, bool bMustBeLiteralDim);

    /**
     * @brief Checks whether a string contains an assignment operator with a valid L-value.
     *
     * Locates the first `'='`, extracts the left side, and verifies it passes
     * CStr::IsTextLValue (starts with a valid identifier character).
     *
     * @param[in] pString  The string to inspect.
     * @return true if a valid assignment was detected, false otherwise.
     */
    [[nodiscard]] bool ContainsAssignmentOperator(CStr* pString);

} // namespace StatementHelper
