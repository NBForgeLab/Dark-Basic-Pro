#pragma once

// LeapMarkerManager.h - Leap marker / backpatching subsystem extracted from CASMWriter.
// Handles forward references and backpatching in x86 machine code emission.

#include <windows.h>

// Forward declarations to avoid circular dependency with ASMWriter.h
class CASMWriter;

class CLeapMarkerManager {
public:
	CLeapMarkerManager();
	~CLeapMarkerManager() = default;

	// Reset all leap marker state
	void Reset() noexcept;

	// Rebase internal pointers after machine code buffer expansion.
	// Preserves relative offsets when the underlying storage is relocated.
	void RebaseForBufferExpansion(
		LPSTR pNewProgramStart,
		DWORD dwNewMCBlockSize);

	// Leap marker methods (moved from CASMWriter)
	bool WriteASMLeapMarkerTop(CASMWriter* pWriter);
	bool WriteASMLineLeapToTop(DWORD dwOp, CASMWriter* pWriter);
	bool WriteASMLeapMarkerJumpToTop(CASMWriter* pWriter);

	bool WriteASMLeapForwardMarker(CASMWriter* pWriter);
	bool WriteASMLineLeap(DWORD dwOp, DWORD di, CASMWriter* pWriter);
	bool WriteASMLeapMarkerJump(DWORD dwOp, DWORD di, CASMWriter* pWriter);
	bool WriteASMLeapMarkerJumpNotEqual(DWORD di, CASMWriter* pWriter);
	bool WriteASMLeapMarkerEnd(DWORD di, CASMWriter* pWriter);

	// Access to leap state for CheckAndExpandMCBMemory
	[[nodiscard]] LPSTR GetRecordTopBytePosition() const noexcept { return m_pRecordTopBytePosition; }
	[[nodiscard]] LPSTR GetRecordBytePosition(DWORD index) const noexcept { return index < MAX_LEAP_MARKERS ? m_pRecordBytePosition[index] : nullptr; }
	void SetRecordTopBytePosition(LPSTR pos) noexcept { m_pRecordTopBytePosition = pos; }
	void SetRecordBytePosition(DWORD index, LPSTR pos) noexcept { if (index < MAX_LEAP_MARKERS) m_pRecordBytePosition[index] = pos; }

private:
	static constexpr DWORD MAX_LEAP_MARKERS = 9;

	// Leap Marker State
	LPSTR  m_pRecordTopBytePosition = nullptr;
	DWORD  m_pRecordRefPosition[MAX_LEAP_MARKERS] = {0};
	LPSTR  m_pRecordBytePosition[MAX_LEAP_MARKERS] = {nullptr};
};
