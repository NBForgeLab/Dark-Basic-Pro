#pragma once
#include <windows.h>
#include <string>
#include <vector>

/**
 * @file Tokenizer.h
 * @brief Lexical tokenizer for Dark Basic Pro source code.
 *
 * CTokenizer performs lexical analysis on raw source buffers: skipping comments
 * and whitespace, extracting the next token delimited by separators or line
 * endings, resolving language keywords to Token enum values, and classifying
 * variable names by their type suffix (`$` string, `#` float, otherwise integer).
 *
 * Extracted from CStatement to isolate lexical scanning from statement parsing.
 *
 * @see CStatement  Original parent class that delegates tokenizing here.
 * @see Token        Enum of keyword token identifiers returned by DetermineKeywordToken.
 */
class CTokenizer
{
public:
    CTokenizer() noexcept = default;
    ~CTokenizer() = default;

    CTokenizer(const CTokenizer&) = delete;
    CTokenizer& operator=(const CTokenizer&) = delete;
    CTokenizer(CTokenizer&&) noexcept = default;
    CTokenizer& operator=(CTokenizer&&) noexcept = default;

    /** @brief Binds a source buffer and resets the scan position to zero. */
    void SetSourceBuffer(const char* pSourceBuffer) noexcept;

    /** @brief Returns the current source buffer pointer (may be null). */
    [[nodiscard]] const char* GetSourceBuffer() const noexcept { return m_pSourceBuffer; }

    /** @brief Returns the current byte offset within the source buffer. */
    [[nodiscard]] DWORD GetCurrentPosition() const noexcept { return m_dwCurrentPos; }

    /** @brief Sets the current byte offset within the source buffer. */
    void SetCurrentPosition(DWORD dwPos) noexcept { m_dwCurrentPos = dwPos; }

    /** @brief Advances past all whitespace, REM comments, backtick comments, and // comments. */
    void SkipAllComments() noexcept;

    /** @brief Advances to the next CR or LF character (skips one full line of content). */
    void SkipToCR() noexcept;

    /** @brief Advances to the next statement separator (colon, comma, or line ending). */
    void SeekToSeperator() noexcept;

    /** @brief Advances past all CR, LF, space, and tab characters. */
    void AdvancePastCRandSPACES() noexcept;

    /** @brief Extracts all characters from the current position to the end of line.
     *  @return  The extracted line content (may be empty). Advances position past the content. */
    [[nodiscard]] std::string GetStringToEndOfLine();

    /**
     * @brief Extracts the next whitespace-delimited token from the source stream.
     * @param[in,out] pString          Pointer into the source buffer; advanced past the consumed token.
     * @param[in]     bIncrementLineNumber  If true, increments the global line counter on CR/LF.
     * @param[in]     bProduceCRTK     If true, emits a CR/LF token at line boundaries.
     * @param[in]     bIncludeCommas   If true, treats commas as token separators.
     * @return Heap-allocated token string. Caller owns the memory (free with `delete[]`).
     *         Returns nullptr when no more tokens are available.
     */
    [[nodiscard]] LPSTR ProduceNextToken(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas) const;

    /**
     * @brief Extended token extraction with equate-symbol space handling.
     * @param[in]     bIgnoreSpacesAroundEquateSymbol  If true, preserves spaces adjacent to '=' as part of the token.
     * @return Heap-allocated token string. Caller owns the memory (free with `delete[]`).
     * @see ProduceNextToken  Delegates here with bIgnoreSpacesAroundEquateSymbol = false.
     */
    [[nodiscard]] LPSTR ProduceNextTokenEx(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas, bool bIgnoreSpacesAroundEquateSymbol) const;

    /**
     * @brief Extracts an array declaration token including bracket content and optional initializer.
     * @param[in,out] pOrigPointer  Pointer into source; advanced past the consumed array expression.
     * @return Heap-allocated string containing the full array token (e.g. `arr(10)=5`).
     *         Caller owns the memory (free with `delete[]`). Returns NULL if no token found.
     */
    [[nodiscard]] LPSTR ProduceNextArrayToken(LPSTR* pOrigPointer) const;

    /**
     * @brief Extracts a full statement segment up to a colon separator or line ending.
     * @param[in,out] pOrigPointer  Pointer into source; advanced past the consumed segment.
     * @return Heap-allocated segment string. Caller owns the memory (free with `delete[]`).
     *         Returns NULL if the segment is empty.
     */
    [[nodiscard]] LPSTR ProduceFullSegment(LPSTR* pOrigPointer) const;

    /**
     * @brief Determines the variable type from a name suffix.
     * @param[in] pNameStr  Null-terminated variable name.
     * @return 1 = string (suffix `$`), 2 = float (suffix `#`), 3 = integer/default, 0 = invalid input.
     */
    [[nodiscard]] int DetermineNameToken(const char* pNameStr) const noexcept;

    /**
     * @brief Maps a keyword string to its Token enum value (case-insensitive).
     * @param[in] pToken  Null-terminated keyword string.
     * @return The corresponding Token enum value cast to DWORD, or 0 if not a recognized keyword.
     */
    [[nodiscard]] DWORD DetermineKeywordToken(const char* pToken) const noexcept;

    /** @brief Returns true if the token matches a reserved word (if, then, else, etc.). */
    [[nodiscard]] bool DetermineIfReservedWord(const char* pToken) const noexcept;

    /** @brief Returns true if the token starts with an alphabetic character or underscore (valid function name). */
    [[nodiscard]] bool DetermineIfFunctionName(const char* pToken) const noexcept;

private:
    const char* m_pSourceBuffer{ nullptr };
    DWORD m_dwCurrentPos{ 0 };
};
