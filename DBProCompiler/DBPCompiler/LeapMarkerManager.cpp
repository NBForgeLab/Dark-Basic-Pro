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

CLeapMarkerManager::~CLeapMarkerManager()
{
}

void CLeapMarkerManager::Reset()
{
	m_pRecordTopBytePosition = NULL;
	for (DWORD di = 0; di < MAX_LEAP_MARKERS; di++)
	{
		m_pRecordRefPosition[di] = 0;
		m_pRecordBytePosition[di] = 0;
	}
}

void CLeapMarkerManager::RebaseForBufferExpansion(
	LPSTR pNewProgramStart,
	DWORD dwNewMCBlockSize)
{
	// Save relative offsets
	DWORD dwByteOffset = m_pRecordTopBytePosition - pNewProgramStart;
	DWORD dwLeapRelDiff[MAX_LEAP_MARKERS];
	for (DWORD di = 0; di < MAX_LEAP_MARKERS; di++)
		dwLeapRelDiff[di] = m_pRecordBytePosition[di] - pNewProgramStart;

	// Note: The caller (CASMWriter::CheckAndExpandMCBMemory) is responsible
	// for actually resizing the buffer and updating pNewProgramStart.
	// After the resize, the caller must update our pointers using the
	// saved relative offsets via the Set methods.

	// Rebase using the new program start (caller provides the already-updated pointer)
	m_pRecordTopBytePosition = pNewProgramStart + dwByteOffset;
	for (DWORD di = 0; di < MAX_LEAP_MARKERS; di++)
		m_pRecordBytePosition[di] = pNewProgramStart + dwLeapRelDiff[di];
}

//////////////////////////////////////////////////////////////////////
// Leap Marker Methods
//////////////////////////////////////////////////////////////////////

bool CLeapMarkerManager::WriteASMLeapMarkerTop(CASMWriter* pWriter)
{
	// Record Where We Are Now
	m_pRecordTopBytePosition = pWriter->m_machineCodeBuffer.GetMachineBlock();

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLineLeapToTop(DWORD dwOp, CASMWriter* pWriter)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(pWriter->m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(pWriter->m_ASMDebugStrings[dwOp].c_str()));
	strDBMLine.AddText(" ");
	strDBMLine.AddText("LEAP TO TOP");
	if (g_pDBMWriter->OutputDBM(&strDBMLine) == false) return false;

	// Calculate Offset For This Leap To Top
	int iOffset = (m_pRecordTopBytePosition - pWriter->m_machineCodeBuffer.GetMachineBlock()) - 6;

	// ASM Code
	CStr offsetStr;
	offsetStr.SetNumericText(iOffset);
	pWriter->CreateASMMiddle(pWriter->m_iASMPreOp[dwOp], pWriter->m_iASMOp1[dwOp], pWriter->m_iASMOp2[dwOp], offsetStr.GetStr());

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLeapMarkerJumpToTop(CASMWriter* pWriter)
{
	pWriter->WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
	WriteASMLineLeapToTop(static_cast<DWORD>(ASMOp::JNE), pWriter);
	return true;
}

bool CLeapMarkerManager::WriteASMLineLeap(DWORD dwOp, DWORD di, CASMWriter* pWriter)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(pWriter->m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(pWriter->m_ASMDebugStrings[dwOp].c_str()));
	strDBMLine.AddText(" ");
	strDBMLine.AddText("LEAP");
	if (g_pDBMWriter->OutputDBM(&strDBMLine) == false) return false;

	// ASM Code
	pWriter->CreateASMMiddle(pWriter->m_iASMPreOp[dwOp], pWriter->m_iASMOp1[dwOp], pWriter->m_iASMOp2[dwOp], "0");

	// Complete
	return true;
}

bool CLeapMarkerManager::WriteASMLeapMarkerJump(DWORD dwOp, DWORD di, CASMWriter* pWriter)
{
	// Write Line As Normal
	WriteASMLineLeap(dwOp, di, pWriter);

	// Record Where JUMP Offset Must Go
	m_pRecordRefPosition[di] = 1 + pWriter->m_dwProgramRefPointer;
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
	if (m_pRecordRefPosition[di] > 0)
	{
		// Prepare for actual indexing
		m_pRecordRefPosition[di] -= 2;

		// Get old ref-string
		LPSTR pRefStr = (LPSTR)pWriter->m_ProgramRefLabels[m_pRecordRefPosition[di]];
		if (pRefStr)
		{
			delete[] pRefStr;
			pRefStr = NULL;
		}

		// Calculate Leap Offset
		DWORD dwLeapOffset = pWriter->m_machineCodeBuffer.GetMachineBlock() - m_pRecordBytePosition[di];

		// Create NEW ref-string from offset value (tempStr is stack-owned; pRefStr keeps new[] ownership)
		CStr tempStr;
		tempStr.SetNumericText(dwLeapOffset);
		pRefStr = new char[strlen(tempStr.GetStr()) + 1];
		strcpy_s(pRefStr, strlen(tempStr.GetStr()) + 1, tempStr.GetStr());
		pWriter->m_ProgramRefLabels[m_pRecordRefPosition[di]] = (uintptr_t)pRefStr;

		// Clear leap flag
		m_pRecordRefPosition[di] = 0;
		m_pRecordBytePosition[di] = 0;
	}
	else
	{
		// No marker to complete
	}

	// Complete
	return true;
}
