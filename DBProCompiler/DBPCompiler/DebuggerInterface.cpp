// DebuggerInterface.cpp: implementation of CDebuggerInterface
// Extracted from CASMWriter (ASMWriter.cpp)
//////////////////////////////////////////////////////////////////////

#include "DebuggerInterface.h"
#include "DebugInfo.h"

#include <cstring>

// External globals - defined elsewhere in the compiler
extern bool g_bIsInternalDebugger;
extern PROCESS_INFORMATION g_InternalDebuggerProcessInfo;
extern bool g_bExternaliseDLLS;
extern CDebugInfo g_DebugInfo;

//////////////////////////////////////////////////////////////////////
// Initialisation
//////////////////////////////////////////////////////////////////////

void CDebuggerInterface::InitDebuggerState()
{
	g_bIsInternalDebugger = false;
	memset(&g_InternalDebuggerProcessInfo, 0, sizeof(PROCESS_INFORMATION));
}

//////////////////////////////////////////////////////////////////////
// Debugger Communication
//////////////////////////////////////////////////////////////////////

LRESULT CDebuggerInterface::SendDataToDebugger(int iType, LPSTR pData, DWORD dwDataSize)
{
	LRESULT lResult = 0;

	// Create Virtual File for Transfer
	HANDLE hFileMap = CreateFileMappingW((HANDLE)0xFFFFFFFF, NULL, PAGE_READWRITE, 0, dwDataSize + 4, L"DBPRODEBUGGERMESSAGE");
	if (hFileMap)
	{
		LPVOID lpVoid = MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, dwDataSize + 4);
		if (lpVoid)
		{
			// Copy to Virtual File
			*(DWORD*)lpVoid = dwDataSize;
			memcpy((LPSTR)lpVoid + 4, pData, dwDataSize);

			// Find Debugger to send to
			HWND hWnd = FindWindowW(NULL, L"DBProDebugger");
			if (hWnd)
			{
				// Found - transmit
				lResult = SendMessage(hWnd, WM_USER + 10, iType, 0);
			}

			// Release virtual file
			UnmapViewOfFile(lpVoid);
		}
		CloseHandle(hFileMap);
	}

	// May have result
	return lResult;
}

void CDebuggerInterface::GetDataFromDebugger(int iType, LPSTR* pData, DWORD* dwDataSize)
{
	// Wait for text to arrive by message (3 second timeout)
	DWORD dwTime = timeGetTime();
	while (g_DebugInfo.MessageArrived() == false)
		if (timeGetTime() > dwTime + 3000)
			break;

	// Act on any data received
	switch (iType)
	{
		case 51 : // CLI Text Message
			if (g_DebugInfo.GetCLIText())
			{
				*pData = new char[g_DebugInfo.GetCLISize() + 2];
				ZeroMemory(*pData, g_DebugInfo.GetCLISize() + 2);
				strcpy_s(*pData, g_DebugInfo.GetCLISize() + 2, g_DebugInfo.GetCLIText());
				*dwDataSize = strlen(*pData) + 1;
			}
			else
			{
				*pData = new char[5];
				ZeroMemory(*pData, 5);
				*dwDataSize = 4;
			}
			break;

		default: // Empty Text Data
			*pData = new char[5];
			ZeroMemory(*pData, 5);
			*dwDataSize = 4;
			break;
	}

	// Message Processed
	g_DebugInfo.SetMessageArrived(false);
}

//////////////////////////////////////////////////////////////////////
// Hidden Code Processing
//////////////////////////////////////////////////////////////////////

bool CDebuggerInterface::HideAnyHiddenCode(LPSTR pData, DWORD dwSize)
{
	LPSTR pPtr = pData;
	LPSTR pPtrEnd = pData + dwSize;
	bool bReplaceOn = false;
	while (pPtr < pPtrEnd)
	{
		// Check ahead
		if (_strnicmp(pPtr, "HIDESTART", 9) == NULL) { bReplaceOn = true;  pPtr += 9; }
		if (_strnicmp(pPtr, "HIDEEND", 7) == NULL)   { bReplaceOn = false; pPtr += 7; }

		// Replace
		if (*pPtr > 32 && bReplaceOn == true) *pPtr = 'X';

		// Next char
		pPtr++;
	}

	// Complete
	return true;
}

//////////////////////////////////////////////////////////////////////
// State Accessors
//////////////////////////////////////////////////////////////////////

bool CDebuggerInterface::IsInternalDebuggerActive()
{
	return g_bIsInternalDebugger;
}

void CDebuggerInterface::SetInternalDebuggerActive(bool bActive)
{
	g_bIsInternalDebugger = bActive;
}

PROCESS_INFORMATION& CDebuggerInterface::GetDebuggerProcessInfo()
{
	return g_InternalDebuggerProcessInfo;
}

bool CDebuggerInterface::ShouldExternaliseDLLs()
{
	return g_bExternaliseDLLS;
}
