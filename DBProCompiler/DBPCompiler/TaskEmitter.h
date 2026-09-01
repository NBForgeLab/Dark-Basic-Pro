#pragma once
#include <cstdint>
#include <string_view>
#include "DataType.h"

class CASMWriter;

/**
 * @file TaskEmitter.h
 * @brief Subsystem for assembly task dispatch, SIB emission, and parameter mode resolution.
 *
 * CTaskEmitter encapsulates task counting, parameter access mode calculations,
 * and assembly instruction emission for array and variable access.
 */
class CTaskEmitter
{
public:
    CTaskEmitter() noexcept = default;
    ~CTaskEmitter() = default;

    CTaskEmitter(const CTaskEmitter&) = delete;
    CTaskEmitter& operator=(const CTaskEmitter&) = delete;
    CTaskEmitter(CTaskEmitter&&) noexcept = default;
    CTaskEmitter& operator=(CTaskEmitter&&) noexcept = default;

    /** @brief Resets task emitter counter. */
    void Reset() noexcept { m_dwTaskCount = 0; }

    /** @brief Increments task count by one. */
    void IncrementTaskCount() noexcept { m_dwTaskCount++; }

    /** @brief Returns current total task count. */
    [[nodiscard]] uint32_t GetTaskCount() const noexcept { return m_dwTaskCount; }

    /** @brief Resolves assembly call code from opcode and type. */
    [[nodiscard]] uint32_t DetermineASMCall(uint32_t dwASMCodeAsAByte, uint32_t dwTypeValue) const noexcept;
    [[nodiscard]] uint32_t DetermineASMCall(uint32_t dwASMCodeAsAByte, DBPType type) const noexcept {
        return DetermineASMCall(dwASMCodeAsAByte, static_cast<uint32_t>(type));
    }

    /** @brief Like DetermineASMCall, but routes pointer/handle types (strings,
     * UDT pointers, array handles) to the 8-byte mov variant so 64-bit values
     * are not truncated on x64. */
    [[nodiscard]] uint32_t DetermineASMCallWide(uint32_t dwASMCodeAsAByte, uint32_t dwTypeValue) const noexcept;

    /** @brief Resolves relative assembly call code from opcode and type. */
    [[nodiscard]] uint32_t DetermineASMCallForREL(uint32_t dwASMCodeAsAByte, uint32_t dwTypeValue) const noexcept;
    [[nodiscard]] uint32_t DetermineASMCallForREL(uint32_t dwASMCodeAsAByte, DBPType type) const noexcept {
        return DetermineASMCallForREL(dwASMCodeAsAByte, static_cast<uint32_t>(type));
    }

    /** @brief Evaluates parameter access mode (Mem, Rbp, Imm, etc.) for a token.
     * pPIndex carries the array element index token (additional offset) when the
     * access targets an element; nullptr means the whole variable/handle. */
    [[nodiscard]] uint32_t DetermineParamMode(std::string_view p, uint32_t dwPType, uint32_t dwPOffset, const class CStr* pPIndex) const noexcept;
    [[nodiscard]] uint32_t DetermineParamMode(std::string_view p, DBPType type, uint32_t dwPOffset, const class CStr* pPIndex) const noexcept {
        return DetermineParamMode(p, static_cast<uint32_t>(type), dwPOffset, pPIndex);
    }
    [[nodiscard]] uint32_t DetermineParamMode(const class CStr* pP, uint32_t dwPType, uint32_t dwPOffset, const class CStr* pPIndex) const noexcept;
    [[nodiscard]] uint32_t DetermineParamMode(const class CStr* pP, DBPType type, uint32_t dwPOffset, const class CStr* pPIndex) const noexcept {
        return DetermineParamMode(pP, static_cast<uint32_t>(type), dwPOffset, pPIndex);
    }

    /** @brief Calculates pass offset for multi-pass task emission. */
    [[nodiscard]] uint32_t CalculateTaskPassOffset(uint32_t dwPassNumber, uint32_t dwBaseOffset) const noexcept;

    /** @brief Emits assembly code to load array element into RAX. */
    void WriteASMARRtoRAX(CASMWriter* pASMWriter, uint32_t dwMode, class CStr* pP, class CStr* pOffset, uint32_t dwPType, uint32_t dwPOffset) const;

    /** @brief Emits assembly code to load variable/SIB element into RAX. */
    void WriteASMXtoRAX(CASMWriter* pASMWriter, uint32_t dwMode, class CStr* pP, class CStr* pPIndex, uint32_t dwPType, uint32_t dwPOffset) const;

    /** @brief Emits assembly code to store RAX into array element. */
    void WriteASMRAXtoARR(CASMWriter* pASMWriter, uint32_t dwMode, class CStr* pP, class CStr* pOffset, uint32_t dwPType, uint32_t dwPOffset) const;

    /** @brief Emits assembly code to store RAX into variable/SIB element. */
    void WriteASMRAXtoX(CASMWriter* pASMWriter, uint32_t dwMode, class CStr* pP, class CStr* pPIndex, uint32_t dwPType, uint32_t dwPOffset) const;

private:
    uint32_t m_dwTaskCount{ 0 };
};
