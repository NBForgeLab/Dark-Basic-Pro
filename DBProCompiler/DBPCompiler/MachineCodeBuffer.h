#pragma once

// MachineCodeBuffer.h - Machine code buffer management extracted from CASMWriter.
// Handles raw x86 machine code storage, low-level emission, and buffer expansion.

#include <windows.h>
#include <vector>

class CMachineCodeBuffer {
public:
	CMachineCodeBuffer();
	~CMachineCodeBuffer();

	// Initialize the buffer with the given size, filled with RET (0xC3).
	bool Initialize(DWORD dwInitialSize);

	// Expand the machine code buffer if within 100 bytes of the end.
	// Returns true if expansion occurred, false otherwise.
	bool CheckAndExpandMCBMemory();

	// Returns the current write position as offset from program start.
	[[nodiscard]] DWORD GetCurrentMCPosition() const noexcept;

	// Returns the byte position of the last instruction (same as current position).
	[[nodiscard]] DWORD GetBytePosOfLastInstruction() const noexcept;

	// Free the machine code buffer and reset all pointers.
	void FreeMachineBlock() noexcept;

	// Write a single byte to the machine code buffer and advance.
	void WriteByte(int byte);

	// Write a DWORD value to the machine code buffer, advancing by dwSize bytes.
	void WriteDWORD(DWORD value, DWORD dwSize);

	// Accessors for internal pointers (used by CASMWriter and CLeapMarkerManager).
	[[nodiscard]] LPSTR GetProgramStart() const noexcept { return m_pProgramStart; }
	[[nodiscard]] LPSTR GetMachineBlock() const noexcept { return m_pMachineBlock; }
	[[nodiscard]] DWORD GetMCBlockSize() const noexcept { return m_dwMCBlockSize; }

	// Returns the current write pointer (mutable) for direct writes.
	[[nodiscard]] LPSTR GetMachineBlockForWrite() noexcept { return m_pMachineBlock; }

	// Advance the machine block pointer by n bytes.
	void AdvanceMachineBlock(int n) noexcept { m_pMachineBlock += n; }

	// Set the machine block pointer (used during leap marker operations).
	void SetMachineBlock(LPSTR pNew) noexcept { m_pMachineBlock = pNew; }

private:
	// Expansion chunk size (100K)
	static constexpr DWORD EXPANSION_CHUNK = 102400;
	// Safety margin before end of buffer that triggers expansion
	static constexpr DWORD EXPANSION_THRESHOLD = 100;

	DWORD					m_dwMCBlockSize = 0;
	std::vector<char>		m_machineCodeStorage;
	LPSTR					m_pProgramStart = nullptr;
	LPSTR					m_pMachineBlock = nullptr;
};
