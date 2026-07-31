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
    [[nodiscard]] int DetermineNameToken(const char* pNameStr) const noexcept;
    [[nodiscard]] bool DetermineIfReservedWord(const char* pToken) const noexcept;
    [[nodiscard]] bool DetermineIfFunctionName(const char* pToken) const noexcept;

private:
    const char* m_pSourceBuffer{ nullptr };
    DWORD m_dwCurrentPos{ 0 };
};
