#pragma once

// MachineCodeBuffer.h - Machine code buffer management extracted from CASMWriter.
// Handles raw x86 machine code storage, low-level emission, and buffer expansion.

#include "windows.h"
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
	DWORD GetCurrentMCPosition() const;

	// Returns the byte position of the last instruction (same as current position).
	DWORD GetBytePosOfLastInstruction() const;

	// Free the machine code buffer and reset all pointers.
	void FreeMachineBlock();

	// Write a single byte to the machine code buffer and advance.
	void WriteByte(int byte);

	// Write a DWORD value to the machine code buffer, advancing by dwSize bytes.
	void WriteDWORD(DWORD value, DWORD dwSize);

	// Accessors for internal pointers (used by CASMWriter and CLeapMarkerManager).
	LPSTR GetProgramStart() const { return m_pProgramStart; }
	LPSTR GetMachineBlock() const { return m_pMachineBlock; }
	DWORD GetMCBlockSize() const { return m_dwMCBlockSize; }

	// Returns the current write pointer (mutable) for direct writes.
	LPSTR GetMachineBlockForWrite() { return m_pMachineBlock; }

	// Advance the machine block pointer by n bytes.
	void AdvanceMachineBlock(int n) { m_pMachineBlock += n; }

	// Set the machine block pointer (used during leap marker operations).
	void SetMachineBlock(LPSTR pNew) { m_pMachineBlock = pNew; }

private:
	// Expansion chunk size (100K)
	static constexpr DWORD EXPANSION_CHUNK = 102400;
	// Safety margin before end of buffer that triggers expansion
	static constexpr DWORD EXPANSION_THRESHOLD = 100;

	DWORD					m_dwMCBlockSize;
	std::vector<char>		m_machineCodeStorage;
	LPSTR					m_pProgramStart;
	LPSTR					m_pMachineBlock;
};
