#pragma once
#include <windows.h>
#include <string>
#include <vector>

class CTokenizer
{
public:
    CTokenizer() noexcept = default;
    ~CTokenizer() = default;

    CTokenizer(const CTokenizer&) = delete;
    CTokenizer& operator=(const CTokenizer&) = delete;
    CTokenizer(CTokenizer&&) noexcept = default;
    CTokenizer& operator=(CTokenizer&&) noexcept = default;

    void SetSourceBuffer(const char* pSourceBuffer) noexcept;

    [[nodiscard]] const char* GetSourceBuffer() const noexcept { return m_pSourceBuffer; }
    [[nodiscard]] DWORD GetCurrentPosition() const noexcept { return m_dwCurrentPos; }
    void SetCurrentPosition(DWORD dwPos) noexcept { m_dwCurrentPos = dwPos; }

    void SkipAllComments() noexcept;
    void SkipToCR() noexcept;
    void SeekToSeperator() noexcept;
    void AdvancePastCRandSPACES() noexcept;

    [[nodiscard]] std::string GetStringToEndOfLine();
    [[nodiscard]] LPSTR ProduceNextToken(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas) const;
    [[nodiscard]] LPSTR ProduceNextTokenEx(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas, bool bIgnoreSpacesAroundEquateSymbol) const;
    [[nodiscard]] LPSTR ProduceNextArrayToken(LPSTR* pOrigPointer) const;
    [[nodiscard]] LPSTR ProduceFullSegment(LPSTR* pOrigPointer) const;
    [[nodiscard]] int DetermineNameToken(const char* pNameStr) const noexcept;
    [[nodiscard]] DWORD DetermineKeywordToken(const char* pToken) const noexcept;
    [[nodiscard]] bool DetermineIfReservedWord(const char* pToken) const noexcept;
    [[nodiscard]] bool DetermineIfFunctionName(const char* pToken) const noexcept;

private:
    const char* m_pSourceBuffer{ nullptr };
    DWORD m_dwCurrentPos{ 0 };
};
