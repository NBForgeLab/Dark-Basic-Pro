#pragma once

#include <windows.h>

// CDebuggerInterface - extracted from CASMWriter
// Handles communication with internal/external debugger processes and
// provides controlled access to the global debugger state variables.
class CDebuggerInterface
{
public:
	// Initialise global debugger state (called once from CASMWriter constructor)
	static void InitDebuggerState() noexcept;

	// Send data to the debugger process via shared memory mapping.
	// Returns the LRESULT from SendMessage, or 0 if no debugger window found.
	static LRESULT SendDataToDebugger(int iType, LPSTR pData, DWORD dwDataSize);

	// Receive data from the debugger process.
	// Allocates *pData with new[]; caller owns the memory.
	static void GetDataFromDebugger(int iType, LPSTR* pData, DWORD* dwDataSize);

	// Replace printable characters between HIDESTART/HIDEEND markers with 'X'.
	static bool HideAnyHiddenCode(LPSTR pData, DWORD dwSize);

	// Static accessors for global debugger state
	[[nodiscard]] static bool IsInternalDebuggerActive() noexcept;
	static void SetInternalDebuggerActive(bool bActive) noexcept;
	[[nodiscard]] static PROCESS_INFORMATION& GetDebuggerProcessInfo() noexcept;
	[[nodiscard]] static bool ShouldExternaliseDLLs() noexcept;
};
