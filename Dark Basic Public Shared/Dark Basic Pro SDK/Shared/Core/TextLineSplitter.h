//
// TextLineSplitter - line-ending tolerant text line scanning
//
// Extracted from the CRLF-only scan that lived inside LoadArrayCore so the
// behaviour is unit-testable and shared. Recognises CRLF, lone LF and lone
// CR as line breaks; a trailing line without a terminator is still emitted.
//
// Header-only, allocation-free and C++03 compatible: this must compile both
// in the legacy DBProCore.dll project (v100 toolset) and in dbp_tests.
//

#ifndef DBP_SDK_CORE_TEXT_LINE_SPLITTER_H
#define DBP_SDK_CORE_TEXT_LINE_SPLITTER_H

struct TextLineCursor
{
    const char*   pNext;      // first unread character
    unsigned long ulRemaining; // bytes left from pNext
};

inline void TextLineCursorInit(TextLineCursor* pCursor, const char* pData, unsigned long ulSize)
{
    pCursor->pNext = pData;
    pCursor->ulRemaining = pData ? ulSize : 0;
}

// Emits the next line (without its terminator). Returns 0 when exhausted.
inline int TextLineCursorNext(TextLineCursor* pCursor, const char** ppLineStart, unsigned long* pulLineLength)
{
    if (pCursor->ulRemaining == 0)
        return 0;

    const char* pStart = pCursor->pNext;
    const char* pScan = pStart;
    const char* pEnd = pStart + pCursor->ulRemaining;

    while (pScan < pEnd && *pScan != '\r' && *pScan != '\n')
        pScan++;

    *ppLineStart = pStart;
    *pulLineLength = (unsigned long)(pScan - pStart);

    // Swallow one line break: CRLF counts as a single break.
    if (pScan < pEnd)
    {
        if (*pScan == '\r' && (pScan + 1) < pEnd && *(pScan + 1) == '\n')
            pScan += 2;
        else
            pScan += 1;
    }

    pCursor->pNext = pScan;
    pCursor->ulRemaining = (unsigned long)(pEnd - pScan);
    return 1;
}

#endif // DBP_SDK_CORE_TEXT_LINE_SPLITTER_H
