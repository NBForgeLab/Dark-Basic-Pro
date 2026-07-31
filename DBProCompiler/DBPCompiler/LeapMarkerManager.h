#pragma once

// LeapMarkerManager.h - Leap marker / backpatching subsystem extracted from CASMWriter.
// Handles forward references and backpatching in x86 machine code emission.

#include "windows.h"

// Forward declarations to avoid circular dependency with ASMWriter.h
class CASMWriter;

class CLeapMarkerManager {
public:
	CLeapMarkerManager();
	~CLeapMarkerManager();

	// Reset all leap marker state
	void Reset();

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
	LPSTR GetRecordTopBytePosition() const { return m_pRecordTopBytePosition; }
	LPSTR GetRecordBytePosition(DWORD index) const { return m_pRecordBytePosition[index]; }
	void SetRecordTopBytePosition(LPSTR pos) { m_pRecordTopBytePosition = pos; }
	void SetRecordBytePosition(DWORD index, LPSTR pos) { m_pRecordBytePosition[index] = pos; }

private:
	static constexpr DWORD MAX_LEAP_MARKERS = 9;

	// Leap Marker State
	LPSTR  m_pRecordTopBytePosition;
	DWORD  m_pRecordRefPosition[MAX_LEAP_MARKERS];
	LPSTR  m_pRecordBytePosition[MAX_LEAP_MARKERS];
};
