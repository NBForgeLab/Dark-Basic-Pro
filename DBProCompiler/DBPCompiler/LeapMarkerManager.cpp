// LeapMarkerManager.cpp: implementation of the CLeapMarkerManager class.
// Handles forward references and backpatching in x86 machine code emission.
//////////////////////////////////////////////////////////////////////

#include "LeapMarkerManager.h"
#include "ASMWriter.h"
#include "DBMWriter.h"
#include "ParserHeader.h"
#include "Str.h"

// External global DBM writer
extern CDBMWriter* g_pDBMWriter;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLeapMarkerManager::CLeapMarkerManager()
{
	Reset();
}

void CLeapMarkerManager::Reset() noexcept
{
	m_pRecordTopBytePosition = nullptr;
	for (DWORD di = 0; di < MAX_LEAP_MARKERS; di++)
	{
		m_pRecordRefPosition[di] = 0;
		m_pRecordBytePosition[di] = nullptr;
	}
}

void CLeapMarkerManager::RebaseForBufferExpansion(
	LPSTR pNewProgramStart,
	DWORD dwNewMCBlockSize)
{
	if (!pNewProgramStart)
		return;

	// Save relative offsets
	DWORD dwByteOffset = static_cast<DWORD>(m_pRecordTopBytePosition - pNewProgramStart);
	DWORD dwLeapRelDiff[MAX_LEAP_MARKERS];
	for (DWORD di = 0; di < MAX_LEAP_MARKERS; di++)
		dwLeapRelDiff[di] = static_cast<DWORD>(m_pRecordBytePosition[di] - pNewProgramStart);

	// Rebase using the new program start
	m_pRecordTopBytePosition = pNewProgramStart + dwByteOffset;
	for (DWORD di = 0; di < MAX_LEAP_MARKERS; di++)
		m_pRecordBytePosition[di] = pNewProgramStart + dwLeapRelDiff[di];
}

//////////////////////////////////////////////////////////////////////
// Leap Marker Methods
//////////////////////////////////////////////////////////////////////

bool CLeapMarkerManager::WriteASMLeapMarkerTop(CASMWriter* pWriter)
{
	if (!pWriter) return false;

	// Record Where We Are Now
	m_pRecordTopBytePosition = pWriter->m_machineCodeBuffer.GetMachineBlock();

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLineLeapToTop(DWORD dwOp, CASMWriter* pWriter)
{
	if (!pWriter) return false;

	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(pWriter->m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(pWriter->GetASMOpcodeDef(dwOp).name ? pWriter->GetASMOpcodeDef(dwOp).name : ""));
	strDBMLine.AddText(" ");
	strDBMLine.AddText("LEAP TO TOP");
	if (g_pDBMWriter && g_pDBMWriter->OutputDBM(&strDBMLine) == false) return false;

	// Calculate Offset For This Leap To Top
	int iOffset = static_cast<int>((m_pRecordTopBytePosition - pWriter->m_machineCodeBuffer.GetMachineBlock()) - 6);

	// ASM Code
	CStr offsetStr;
	offsetStr.SetNumericText(iOffset);
	pWriter->CreateASMMiddle(pWriter->GetASMOpcodeDef(dwOp).preOp, pWriter->GetASMOpcodeDef(dwOp).op1, pWriter->GetASMOpcodeDef(dwOp).op2, offsetStr.GetStr());

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLeapMarkerJumpToTop(CASMWriter* pWriter)
{
	if (!pWriter) return false;

	pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
	WriteASMLineLeapToTop(static_cast<DWORD>(ASMOp::JNE), pWriter);
	return true;
}

bool CLeapMarkerManager::WriteASMLineLeap(DWORD dwOp, DWORD di, CASMWriter* pWriter)
{
	if (!pWriter) return false;

	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(pWriter->m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(pWriter->GetASMOpcodeDef(dwOp).name ? pWriter->GetASMOpcodeDef(dwOp).name : ""));
	strDBMLine.AddText(" ");
	strDBMLine.AddText("LEAP");
	if (g_pDBMWriter && g_pDBMWriter->OutputDBM(&strDBMLine) == false) return false;

	// ASM Code
	pWriter->CreateASMMiddle(pWriter->GetASMOpcodeDef(dwOp).preOp, pWriter->GetASMOpcodeDef(dwOp).op1, pWriter->GetASMOpcodeDef(dwOp).op2, "0");

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLeapMarkerJump(DWORD dwOp, DWORD di, CASMWriter* pWriter)
{
	if (!pWriter || di >= MAX_LEAP_MARKERS) return false;

	// Write Line As Normal
	WriteASMLineLeap(dwOp, di, pWriter);

	// Record Where JUMP Offset Must Go
	m_pRecordRefPosition[di] = 1 + pWriter->GetReferenceTracker().GetRefPointer();
	m_pRecordBytePosition[di] = pWriter->m_machineCodeBuffer.GetMachineBlock();

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLeapMarkerJumpNotEqual(DWORD di, CASMWriter* pWriter)
{
	return WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JNE), di, pWriter);
}

bool CLeapMarkerManager::WriteASMLeapForwardMarker(CASMWriter* pWriter)
{
	if (!pWriter) return false;

	// Check if escape value is zero
	pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_ESC_");
	pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");

	// LEAP-FORWARDS Marker OpCode (different from leap-back)
	WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 0, pWriter);

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLeapMarkerEnd(DWORD di, CASMWriter* pWriter)
{
	if (!pWriter || di >= MAX_LEAP_MARKERS) return false;

	if (m_pRecordRefPosition[di] > 0)
	{
		// Prepare for actual indexing
		m_pRecordRefPosition[di] -= 2;

		// Calculate Leap Offset
		DWORD dwLeapOffset = static_cast<DWORD>(pWriter->m_machineCodeBuffer.GetMachineBlock() - m_pRecordBytePosition[di]);

		// Replace the placeholder with the resolved relative offset. The
		// reference tracker owns the string and does not expose raw allocation.
		CStr tempStr;
		tempStr.SetNumericText(dwLeapOffset);
		if (!pWriter->GetReferenceTracker().SetRefLabel(
				m_pRecordRefPosition[di], tempStr.GetStr()))
		{
			return false;
		}

		// Clear leap flag
		m_pRecordRefPosition[di] = 0;
		m_pRecordBytePosition[di] = nullptr;
	}

	// Complete
	return true;
}
