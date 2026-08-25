#pragma once

#include "ParserHeader.h"

class CASMWriter;

/**
 * @file LeapMarkerManager.h
 * @brief Forward-jump backpatching subsystem for x86 machine code emission.
 *
 * CLeapMarkerManager implements the "leap marker" pattern for control flow
 * constructs (DO/LOOP, WHILE/ENDWHILE, IF/ELSE/ENDIF, FOR/NEXT).  When the
 * compiler encounters a forward jump whose target is not yet known, it emits
 * a placeholder offset and records the byte position.  When the target is
 * later reached, WriteASMLeapMarkerEnd backpatches the real offset into the
 * reference tracker.
 *
 * Supports up to 9 nested forward-leap markers plus one "top" marker for
 * backward jumps (loop-back).  After a machine code buffer expansion,
 * RebaseForBufferExpansion must be called to update all internal raw pointers.
 *
 * Extracted from CASMWriter to isolate backpatching from general code emission.
 *
 * @see CASMWriter          Owns this manager and calls its methods during compilation.
 * @see CMachineCodeBuffer  Provides the underlying buffer that may be rebased.
 * @see CReferenceTracker   Stores the reference entries that get backpatched.
 */
class CLeapMarkerManager {
public:
	CLeapMarkerManager();
	~CLeapMarkerManager() = default;

	/** @brief Resets all leap marker state (top position and all indexed markers). */
	void Reset() noexcept;

	/**
	 * @brief Rebases internal byte pointers after a machine code buffer expansion.
	 *
	 * Preserves relative offsets from program start when the underlying
	 * std::vector storage is relocated by CMachineCodeBuffer::CheckAndExpandMCBMemory.
	 *
	 * @param[in] pNewProgramStart  New base address of the machine code buffer.
	 * @param[in] dwNewMCBlockSize  New total size of the buffer (reserved for future use).
	 */
	void RebaseForBufferExpansion(
		LPSTR pNewProgramStart,
		DWORD dwNewMCBlockSize);

	/** @brief Records the current MC position as the backward-jump target ("top" of a loop). */
	bool WriteASMLeapMarkerTop(CASMWriter* pWriter);

	/** @brief Emits a backward jump to the previously recorded top marker. */
	bool WriteASMLineLeapToTop(DWORD dwOp, CASMWriter* pWriter);

	/** @brief Emits a compare-and-jump-not-equal back to the top marker (loop conditional). */
	bool WriteASMLeapMarkerJumpToTop(CASMWriter* pWriter);

	/** @brief Emits a forward-jump placeholder and records its position at index 0 (escape check). */
	bool WriteASMLeapForwardMarker(CASMWriter* pWriter);

	/** @brief Emits a forward-jump placeholder with debug output for leap index @p di. */
	bool WriteASMLineLeap(DWORD dwOp, DWORD di, CASMWriter* pWriter);

	/** @brief Emits a forward-jump placeholder and records byte/ref positions at index @p di. */
	bool WriteASMLeapMarkerJump(DWORD dwOp, DWORD di, CASMWriter* pWriter);

	/** @brief Convenience: emits a JNE forward-jump placeholder at leap index @p di. */
	bool WriteASMLeapMarkerJumpNotEqual(DWORD di, CASMWriter* pWriter);

	/**
	 * @brief Backpatches a forward-jump at index @p di with the actual offset.
	 *
	 * Calculates the byte distance from the recorded jump position to the
	 * current MC write position, then overwrites the reference tracker entry
	 * with the real offset string.
	 *
	 * @param[in] di       Leap marker index (0 – MAX_LEAP_MARKERS-1).
	 * @param[in] pWriter  CASMWriter providing MC buffer and reference tracker access.
	 * @return true on success, false if di is out of range or pWriter is null.
	 */
	bool WriteASMLeapMarkerEnd(DWORD di, CASMWriter* pWriter);

	/** @brief Returns the recorded top-of-loop byte position. */
	[[nodiscard]] LPSTR GetRecordTopBytePosition() const noexcept { return m_pRecordTopBytePosition; }

	/** @brief Returns the recorded byte position at the given leap index, or nullptr if out of range. */
	[[nodiscard]] LPSTR GetRecordBytePosition(DWORD index) const noexcept { return index < MAX_LEAP_MARKERS ? m_pRecordBytePosition[index] : nullptr; }

	/** @brief Sets the top-of-loop byte position (used during buffer rebasing). */
	void SetRecordTopBytePosition(LPSTR pos) noexcept { m_pRecordTopBytePosition = pos; }

	/** @brief Sets the byte position at a specific leap index (used during buffer rebasing). */
	void SetRecordBytePosition(DWORD index, LPSTR pos) noexcept { if (index < MAX_LEAP_MARKERS) m_pRecordBytePosition[index] = pos; }

private:
	/** @brief Maximum number of nested forward-leap markers supported. */
	static constexpr DWORD MAX_LEAP_MARKERS = 9;

	LPSTR  m_pRecordTopBytePosition = nullptr;
	DWORD  m_pRecordRefPosition[MAX_LEAP_MARKERS] = {0};
	LPSTR  m_pRecordBytePosition[MAX_LEAP_MARKERS] = {nullptr};
};
