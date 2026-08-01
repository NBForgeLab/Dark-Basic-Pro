#pragma once
#include <windows.h>

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
    [[nodiscard]] DWORD GetTaskCount() const noexcept { return m_dwTaskCount; }

    /** @brief Resolves assembly call code from opcode and type. */
    [[nodiscard]] DWORD DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept;

    /** @brief Resolves relative assembly call code from opcode and type. */
    [[nodiscard]] DWORD DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept;

    /** @brief Evaluates parameter access mode (Mem, Ebp, Imm, etc.) for a token. */
    [[nodiscard]] DWORD DetermineParamMode(class CStr* pP, DWORD dwPType, DWORD dwPOffset) const noexcept;

    /** @brief Calculates pass offset for multi-pass task emission. */
    [[nodiscard]] DWORD CalculateTaskPassOffset(DWORD dwPassNumber, DWORD dwBaseOffset) const noexcept;

    /** @brief Validates core task parameters. */
    [[nodiscard]] bool EmitCoreTask(DWORD dwLine, DWORD dwTask) const noexcept;

    /** @brief Validates core task with parameter access modes. */
    [[nodiscard]] bool EmitCoreTask(DWORD dwLine, DWORD dwTask, DWORD dwP1Mode, DWORD dwP2Mode, DWORD dwP3Mode) const noexcept;

    /** @brief Dispatches assembly task execution. */
    [[nodiscard]] bool EmitTask(CASMWriter* pASMWriter, DWORD dwLine, DWORD dwTask) const;

    /** @brief Emits assembly code to load array element into EAX. */
    void WriteASMARRtoEAX(CASMWriter* pASMWriter, DWORD dwMode, class CStr* pP, class CStr* pOffset, DWORD dwPType, DWORD dwPOffset) const;

    /** @brief Emits assembly code to load variable/SIB element into EAX. */
    void WriteASMXtoEAX(CASMWriter* pASMWriter, DWORD dwMode, class CStr* pP, class CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) const;

    /** @brief Emits assembly code to store EAX into array element. */
    void WriteASMEAXtoARR(CASMWriter* pASMWriter, DWORD dwMode, class CStr* pP, class CStr* pOffset, DWORD dwPType, DWORD dwPOffset) const;

    /** @brief Emits assembly code to store EAX into variable/SIB element. */
    void WriteASMEAXtoX(CASMWriter* pASMWriter, DWORD dwMode, class CStr* pP, class CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) const;

private:
    DWORD m_dwTaskCount{ 0 };
};
