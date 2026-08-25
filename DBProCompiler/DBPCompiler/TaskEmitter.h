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

    /** @brief Resolves relative assembly call code from opcode and type. */
    [[nodiscard]] uint32_t DetermineASMCallForREL(uint32_t dwASMCodeAsAByte, uint32_t dwTypeValue) const noexcept;
    [[nodiscard]] uint32_t DetermineASMCallForREL(uint32_t dwASMCodeAsAByte, DBPType type) const noexcept {
        return DetermineASMCallForREL(dwASMCodeAsAByte, static_cast<uint32_t>(type));
    }

    /** @brief Evaluates parameter access mode (Mem, Rbp, Imm, etc.) for a token. */
    [[nodiscard]] uint32_t DetermineParamMode(std::string_view p, uint32_t dwPType, uint32_t dwPOffset) const noexcept;
    [[nodiscard]] uint32_t DetermineParamMode(std::string_view p, DBPType type, uint32_t dwPOffset) const noexcept {
        return DetermineParamMode(p, static_cast<uint32_t>(type), dwPOffset);
    }
    [[nodiscard]] uint32_t DetermineParamMode(const class CStr* pP, uint32_t dwPType, uint32_t dwPOffset) const noexcept;
    [[nodiscard]] uint32_t DetermineParamMode(const class CStr* pP, DBPType type, uint32_t dwPOffset) const noexcept {
        return DetermineParamMode(pP, static_cast<uint32_t>(type), dwPOffset);
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
