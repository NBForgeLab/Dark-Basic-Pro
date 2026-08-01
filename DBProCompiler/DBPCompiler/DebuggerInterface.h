#pragma once

#include <windows.h>

/**
 * @file DebuggerInterface.h
 * @brief Communication layer between the compiler and the internal/external debugger process.
 *
 * CDebuggerInterface provides static methods for sending and receiving data
 * via a Win32 shared memory file mapping (`DBPRODEBUGGERMESSAGE`), managing
 * the global debugger state flags, and obfuscating protected source regions
 * between `HIDESTART` / `HIDEEND` markers.
 *
 * All methods are static because the debugger state is process-global
 * (backed by extern globals defined in the compiler core).
 *
 * Extracted from CASMWriter to isolate debugger IPC from code generation.
 *
 * @see CASMWriter   Original parent class that called these routines directly.
 * @see CDebugInfo   Provides message arrival signaling and CLI text retrieval.
 */
class CDebuggerInterface
{
public:
	/** @brief Zeroes the global debugger state (process info and internal-debugger flag). */
	static void InitDebuggerState() noexcept;

	/**
	 * @brief Sends data to the debugger process via a named shared memory mapping.
	 *
	 * Creates a temporary file mapping named `DBPRODEBUGGERMESSAGE`, copies the
	 * data into it, then sends a WM_USER+10 message to the `DBProDebugger` window.
	 *
	 * @param[in] iType       Message type identifier forwarded as WPARAM.
	 * @param[in] pData       Pointer to the data payload.
	 * @param[in] dwDataSize  Size of the data payload in bytes.
	 * @return The LRESULT from SendMessage, or 0 if no debugger window was found.
	 */
	static LRESULT SendDataToDebugger(int iType, LPSTR pData, DWORD dwDataSize);

	/**
	 * @brief Receives data from the debugger process (blocks up to 3 seconds).
	 *
	 * Waits for CDebugInfo::MessageArrived to become true, then copies the CLI
	 * text into a newly allocated buffer.
	 *
	 * @param[in]  iType       Message type (currently only type 51 = CLI text is handled).
	 * @param[out] pData       Receives a heap-allocated string. Caller owns the memory (free with `delete[]`).
	 * @param[out] dwDataSize  Receives the byte length of the returned data (including null terminator).
	 */
	static void GetDataFromDebugger(int iType, LPSTR* pData, DWORD* dwDataSize);

	/**
	 * @brief Replaces printable characters between HIDESTART/HIDEEND markers with 'X'.
	 * @param[in,out] pData   Source buffer to process (modified in place).
	 * @param[in]     dwSize  Length of the data buffer in bytes.
	 * @return true on success, false if pData is null.
	 */
	static bool HideAnyHiddenCode(LPSTR pData, DWORD dwSize);

	/** @brief Returns true if the internal (in-process) debugger is active. */
	[[nodiscard]] static bool IsInternalDebuggerActive() noexcept;

	/** @brief Sets the internal debugger active flag. */
	static void SetInternalDebuggerActive(bool bActive) noexcept;

	/** @brief Returns a reference to the global debugger PROCESS_INFORMATION structure. */
	[[nodiscard]] static PROCESS_INFORMATION& GetDebuggerProcessInfo() noexcept;

	/** @brief Returns true if DLLs should be externalised for debugger access. */
	[[nodiscard]] static bool ShouldExternaliseDLLs() noexcept;
};
