#include "Tokenizer.h"
#include <cctype>
#include <cstring>

void CTokenizer::SetSourceBuffer(const char* pSourceBuffer) noexcept
{
    m_pSourceBuffer = pSourceBuffer;
    m_dwCurrentPos = 0;
}

void CTokenizer::SkipAllComments() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0')
    {
        char c = m_pSourceBuffer[m_dwCurrentPos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            m_dwCurrentPos++;
            continue;
        }

        // Check for REM or `//` comments
        if ((c == 'r' || c == 'R') &&
            (m_pSourceBuffer[m_dwCurrentPos + 1] == 'e' || m_pSourceBuffer[m_dwCurrentPos + 1] == 'E') &&
            (m_pSourceBuffer[m_dwCurrentPos + 2] == 'm' || m_pSourceBuffer[m_dwCurrentPos + 2] == 'M'))
        {
            char nextChar = m_pSourceBuffer[m_dwCurrentPos + 3];
            if (nextChar == ' ' || nextChar == '\t' || nextChar == ':' || nextChar == '\r' || nextChar == '\n' || nextChar == '\0')
            {
                SkipToCR();
                continue;
            }
        }

        if (c == '`' || (c == '/' && m_pSourceBuffer[m_dwCurrentPos + 1] == '/'))
        {
            SkipToCR();
            continue;
        }

        break;
    }
}

void CTokenizer::SkipToCR() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\r' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\n')
    {
        m_dwCurrentPos++;
    }
}

void CTokenizer::SeekToSeperator() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0')
    {
        char c = m_pSourceBuffer[m_dwCurrentPos];
        if (c == ':' || c == ',' || c == '\r' || c == '\n')
        {
            break;
        }
        m_dwCurrentPos++;
    }
}

void CTokenizer::AdvancePastCRandSPACES() noexcept
{
    if (m_pSourceBuffer == nullptr) return;

    while (m_pSourceBuffer[m_dwCurrentPos] != '\0')
    {
        char c = m_pSourceBuffer[m_dwCurrentPos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            m_dwCurrentPos++;
        }
        else
        {
            break;
        }
    }
}

std::string CTokenizer::GetStringToEndOfLine()
{
    if (m_pSourceBuffer == nullptr) return "";

    std::string line;
    while (m_pSourceBuffer[m_dwCurrentPos] != '\0' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\r' &&
           m_pSourceBuffer[m_dwCurrentPos] != '\n')
    {
        line += m_pSourceBuffer[m_dwCurrentPos];
        m_dwCurrentPos++;
    }
    return line;
}

int CTokenizer::DetermineNameToken(const char* pNameStr) const noexcept
{
    if (pNameStr == nullptr || pNameStr[0] == '\0') return 0;

    size_t len = std::strlen(pNameStr);
    char lastChar = pNameStr[len - 1];

    if (lastChar == '$') return 1; // String
    if (lastChar == '#') return 2; // Float
    return 3; // Integer or Default
}

bool CTokenizer::DetermineIfReservedWord(const char* pToken) const noexcept
{
    if (pToken == nullptr) return false;

    static const char* reservedKeywords[] = {
        "if", "then", "else", "endif", "do", "loop", "for", "next",
        "while", "wend", "repeat", "until", "function", "endfunction",
        "type", "endtype", "dim", "global", "local", "goto", "gosub", "return"
    };

    for (const char* kw : reservedKeywords)
    {
        if (_stricmp(pToken, kw) == 0)
        {
            return true;
        }
    }
    return false;
}

bool CTokenizer::DetermineIfFunctionName(const char* pToken) const noexcept
{
    if (pToken == nullptr || pToken[0] == '\0') return false;
    return (std::isalpha(static_cast<unsigned char>(pToken[0])) || pToken[0] == '_');
}
