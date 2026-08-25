//
// DBDLLCore Internal Functions
//

// Common Includes
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// Internal Includes
#include "DBDLLCore.h"
#include "DBDLLDisplay.h"
#include "DBDLLCoreInternal.h"

// External Data Pointer
extern void ExternalDisplaySync(int iSkipSyncDelay);
extern bool	g_bUseExternalDisplayLayer;
extern GlobStruct g_Glob;

DARKSDK void SetPrintCursor(int iX, int iY)
{
	g_Glob.iCursorX = iX;
	g_Glob.iCursorY = iY;
}

DARKSDK void PrintInteger(LONGLONG lValue, bool bIncludeCarriageReturn)
{
	char pString[256] = {};
	sprintf_s(pString, "%lld", lValue);
	PrintSomething(pString, bIncludeCarriageReturn);
}

DARKSDK void PrintFloat(double dValue, bool bIncludeCarriageReturn)
{
	// LEEFIX - 121002 - From .5g which caused the E to appear!!
	// LEEFIX - 141102 - from 20g to 16g so the garbage rounding at end does not show
	// LEEFIX - 151102 - just regular float display g
	// LEEFIX - 200603 - increased presicion .12
	// LEEFIX - 110307 - align with C++ display of float
	char pString[256] = {};
	sprintf_s(pString, "%g", static_cast<float>(dValue));
	PrintSomething(pString, bIncludeCarriageReturn);
}

DARKSDK void PrintString(LPSTR pString, bool bIncludeCarriageReturn)
{
	PrintSomething(pString, bIncludeCarriageReturn);
}

DARKSDK void PrintNothing(void)
{
	PrintSomething(const_cast<LPSTR>(""), true);
}

DARKSDK void InputSomething(LPSTR* pStr)
{
	// Clear Windows Entry Field
	InternalClearWindowsEntry();

	// Set Input Data
	int iEntryX = g_Glob.iCursorX;
	int iEntryY = g_Glob.iCursorY;
	int iLeftmostDeletableX = g_Glob.iCursorX;
	int iRightmostDeletableX = g_Glob.iCursorX;
	size_t dwLocalInputWorkSize = 32;
	char* pLocalInputWorkString = new char[dwLocalInputWorkSize]();
	size_t dwLocalInputWorkPos = 0;

	// Loop to Gather Data
	int iLastStringLen = -1;
	bool bStayInInputLoop = true;
	DWORD dwTraceInputForExitChar = 0;
	while(bStayInInputLoop)
	{
		// Check entry for exit char
		if(dwTraceInputForExitChar < g_dwWindowsTextEntryPos)
		{
			// Get current char from windows entry string
			uint8_t cThisChar = g_Glob.pWindowsTextEntry[dwTraceInputForExitChar];
			dwTraceInputForExitChar++;

			// Check char for exit value (exit on ESCAPE or RETURN)
			if(cThisChar == 13 || cThisChar == 27)
			{
				// Exit string entry
				g_iEntryCursorState = 0;
				bStayInInputLoop = false;
			}
			else
			{
				// Assign char to work string
				if(dwLocalInputWorkPos >= 4)
				{
					if(dwLocalInputWorkPos + 4 > dwLocalInputWorkSize)
					{
						size_t newSize = dwLocalInputWorkSize * 2;
						char* pNewString = new char[newSize]();
						strcpy_s(pNewString, newSize, pLocalInputWorkString);
						delete[] pLocalInputWorkString;
						pLocalInputWorkString = pNewString;
						dwLocalInputWorkSize = newSize;
					}
				}

				// Advance work string position
				if(cThisChar == 8)
				{
					if(dwLocalInputWorkPos > 0)
					{
						dwLocalInputWorkPos--;
						pLocalInputWorkString[dwLocalInputWorkPos] = 0;
					}
				}
				else
				{
					pLocalInputWorkString[dwLocalInputWorkPos] = static_cast<char>(cThisChar);
					pLocalInputWorkString[dwLocalInputWorkPos + 1] = 0;
					dwLocalInputWorkPos++;
				}
			}
		}
		else
		{
			// means we have backspaced
			if(dwTraceInputForExitChar > g_dwWindowsTextEntryPos)
			{
				// remove from local input work string
				if (dwLocalInputWorkPos > 0)
				{
					dwLocalInputWorkPos--;
					pLocalInputWorkString[dwLocalInputWorkPos] = 0;
				}
				dwTraceInputForExitChar--;
			}
		}
		// Display Input
		size_t printBufSize = dwLocalInputWorkSize + 4;
		char* pForPrint = new char[printBufSize]();
		strcpy_s(pForPrint, printBufSize, pLocalInputWorkString);
		if ( static_cast<int>(strlen(pLocalInputWorkString)) > iLastStringLen )
		{
			// leeadd - 300305 - no underscore when advancing
		}
		else
		{
			if(g_iEntryCursorState == 1)
				strcat_s(pForPrint, printBufSize, "_");
		}

		SetPrintCursor(iEntryX, iEntryY);
		PrintSomething(pForPrint, false);
		if(g_Glob.iCursorX < iLeftmostDeletableX)
		{
			// Clear text overrun
			iLeftmostDeletableX = g_Glob.iCursorX;
			ClearSomeText(iLeftmostDeletableX, iRightmostDeletableX);
		}
		iRightmostDeletableX = g_Glob.iCursorX;
		delete[] pForPrint;

		// Handle Cursor Flashing
		g_iEntryCursorState = 1 - g_iEntryCursorState;

		// May have sync on, input overrides this
		if(g_bUseExternalDisplayLayer) ExternalDisplaySync(0);

		// Process message loop while taking input
		if(InternalProcessMessages() == 1)
			break;
	}

	// Advance Cursor
	SIZE Size = GetTextSize(const_cast<LPSTR>(" "));
	g_Glob.iCursorX = 0; g_Glob.iCursorY += Size.cy;

	// Create Output String
	if(pLocalInputWorkString && pStr)
	{
		size_t length = strlen(pLocalInputWorkString);
		char* pNewStr = new char[length + 1]();
		strcpy_s(pNewStr, length + 1, pLocalInputWorkString);
		*pStr = pNewStr;
	}

	// Free usages
	delete[] pLocalInputWorkString;
}

DARKSDK void InputInteger(LONGLONG* plValue)
{
	char* pStr = nullptr;
	InputSomething(&pStr);
	if(pStr)
	{
		if (plValue) *plValue = _atoi64(pStr);
		delete[] pStr;
	}
	else
	{
		if (plValue) *plValue = 0;
	}
}

DARKSDK void InputFloat(double* pdValue)
{
	char* pStr = nullptr;
	InputSomething(&pStr);
	if(pStr)
	{
		if (pdValue) *pdValue = atof(pStr);
		delete[] pStr;
	}
	else
	{
		if (pdValue) *pdValue = 0.0;
	}
}

DARKSDK void InputString(LPSTR* ppStr)
{
	InputSomething(ppStr);
}
