#pragma once
#include <windows.h>

class CASMWriter;

class CTaskEmitter
{
public:
    CTaskEmitter() noexcept = default;
    ~CTaskEmitter() = default;

    CTaskEmitter(const CTaskEmitter&) = delete;
    CTaskEmitter& operator=(const CTaskEmitter&) = delete;
    CTaskEmitter(CTaskEmitter&&) noexcept = default;
    CTaskEmitter& operator=(CTaskEmitter&&) noexcept = default;

    void Reset() noexcept { m_dwTaskCount = 0; }
    void IncrementTaskCount() noexcept { m_dwTaskCount++; }
    [[nodiscard]] DWORD GetTaskCount() const noexcept { return m_dwTaskCount; }

    [[nodiscard]] DWORD DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept;
    [[nodiscard]] DWORD CalculateTaskPassOffset(DWORD dwPassNumber, DWORD dwBaseOffset) const noexcept;
    [[nodiscard]] bool EmitCoreTask(DWORD dwLine, DWORD dwTask) const noexcept;
    [[nodiscard]] bool EmitCoreTask(DWORD dwLine, DWORD dwTask, DWORD dwP1Mode, DWORD dwP2Mode, DWORD dwP3Mode) const noexcept;
    [[nodiscard]] bool EmitTask(CASMWriter* pASMWriter, DWORD dwLine, DWORD dwTask) const;

private:
    DWORD m_dwTaskCount{ 0 };
};
