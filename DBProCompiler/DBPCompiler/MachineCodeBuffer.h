#pragma once

#include "ParserHeader.h"
#include <vector>

/**
 * @file MachineCodeBuffer.h
 * @brief Growable buffer for raw x86 machine code storage and low-level byte emission.
 *
 * CMachineCodeBuffer owns a contiguous block of memory that the assembler writes
 * machine instructions into.  The buffer is initialized filled with RET (0xC3)
 * opcodes and auto-expands in 100 KB increments when the write pointer comes
 * within 100 bytes of the end.  After expansion, internal raw pointers are
 * rebased to the new storage location.
 *
 * Extracted from CASMWriter to isolate memory management from code generation logic.
 *
 * @see CASMWriter           Original parent class; delegates all MC writes here.
 * @see CLeapMarkerManager   Rebases its own pointers via RebaseForBufferExpansion
 *                            after this buffer expands.
 */
class CMachineCodeBuffer {
public:
	CMachineCodeBuffer();
	~CMachineCodeBuffer();

	/**
	 * @brief Allocates the machine code buffer filled with RET (0xC3) opcodes.
	 * @param[in] dwInitialSize  Size in bytes for the initial buffer allocation.
	 * @return true on success (always succeeds with current implementation).
	 */
	bool Initialize(DWORD dwInitialSize);

	/**
	 * @brief Expands the buffer by 100 KB if the write pointer is within 100 bytes of the end.
	 *
	 * After expansion, m_pProgramStart and m_pMachineBlock are rebased to the
	 * new underlying storage.  Callers must rebase any external pointers (e.g.
	 * CLeapMarkerManager) after this returns true.
	 *
	 * @return true if expansion occurred, false if no expansion was needed.
	 */
	bool CheckAndExpandMCBMemory();

	/** @brief Returns the byte offset of the current write position from program start. */
	[[nodiscard]] DWORD GetCurrentMCPosition() const noexcept;

	/** @brief Returns the byte offset of the last emitted instruction (same as current position). */
	[[nodiscard]] DWORD GetBytePosOfLastInstruction() const noexcept;

	/** @brief Releases the buffer and resets all pointers and size to zero. */
	void FreeMachineBlock() noexcept;

	/** @brief Writes a single byte at the current position and advances the write pointer. */
	void WriteByte(int byte);

	/**
	 * @brief Writes a DWORD value at the current position and advances by @p dwSize bytes.
	 * @param[in] value   The 32-bit value to write.
	 * @param[in] dwSize  Number of bytes to advance (typically 1, 2, or 4).
	 */
	void WriteDWORD(DWORD value, DWORD dwSize);

	/**
	 * @brief Writes a 64-bit QWORD value at the current position and advances by @p dwSize bytes.
	 * @param[in] value   The 64-bit value to write.
	 * @param[in] dwSize  Number of bytes to advance (typically 1, 2, 4, or 8).
	 */
	void WriteQWORD(uint64_t value, DWORD dwSize = 8);

	/** @brief Returns the program start pointer (beginning of the MC buffer). */
	[[nodiscard]] LPSTR GetProgramStart() const noexcept { return m_pProgramStart; }

	/** @brief Returns the current machine block read pointer. */
	[[nodiscard]] LPSTR GetMachineBlock() const noexcept { return m_pMachineBlock; }

	/** @brief Returns the total allocated buffer size in bytes. */
	[[nodiscard]] DWORD GetMCBlockSize() const noexcept { return m_dwMCBlockSize; }

	/** @brief Returns the current write pointer for direct byte manipulation. */
	[[nodiscard]] LPSTR GetMachineBlockForWrite() noexcept { return m_pMachineBlock; }

	/** @brief Advances the write pointer by @p n bytes. */
	void AdvanceMachineBlock(int n) noexcept { m_pMachineBlock += n; }

	/** @brief Sets the write pointer directly (used during leap marker backpatching). */
	void SetMachineBlock(LPSTR pNew) noexcept { m_pMachineBlock = pNew; }

private:
	[[nodiscard]] bool CanWrite(size_t byteCount) const noexcept;

	/** @brief Expansion chunk size (100 KB). */
	static constexpr DWORD EXPANSION_CHUNK = 102400;

	/** @brief Safety margin in bytes before the buffer end that triggers expansion. */
	static constexpr DWORD EXPANSION_THRESHOLD = 100;

	DWORD				m_dwMCBlockSize = 0;
	std::vector<char>		m_machineCodeStorage;
	LPSTR				m_pProgramStart = nullptr;
	LPSTR				m_pMachineBlock = nullptr;
};
