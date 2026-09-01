// include definition
#include "cerror.h"
#include ".\..\core\globstruct.h"
#include <cstdio>
#include <cstdlib>
#pragma comment ( lib, "user32.lib" )
#define DB_PRO 1

// Handler Passed into DLL
CRuntimeErrorHandler* g_pErrorHandler = nullptr;

// Diagnostic: when DBP_TRACE_ERROR env is set, append error info to error_trace.txt
static void TraceRuntimeError(DWORD dwErrorCode, const char* tag)
{
	if (!getenv("DBP_TRACE_ERROR")) return;
	FILE* f = nullptr;
	fopen_s(&f, "error_trace.txt", "a");
	if (f)
	{
		fprintf(f, "err=%u tag=%s\n", dwErrorCode, tag ? tag : "?");
		fclose(f);
	}
}

void Error ([[maybe_unused]] const char* szMessage)
{
	#if DB_PRO
		if(g_pErrorHandler)
			if(g_pErrorHandler->dwErrorCode == 0)
				RunTimeError(RUNTIMEERROR_GENERICERROR);
	#else
		HWND mHwnd = GetForegroundWindow();
		ShowCursor(true);
		MessageBoxA(nullptr, szMessage, "DarkBASIC Pro Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
	#endif
}

void Message ([[maybe_unused]] int iID, const char* szMessage, const char* szTitle )
{
	MessageBoxA(nullptr, szMessage, szTitle, MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_TOPMOST);
}

void RunTimeError ( DWORD dwErrorCode )
{
	// Diagnostic trace
	TraceRuntimeError(dwErrorCode, "RunTimeError");
	// Assign Run Time Error To Global Error Handler
	if(g_pErrorHandler) g_pErrorHandler->dwErrorCode = dwErrorCode;
}

void RunTimeWarning ([[maybe_unused]] DWORD dwErrorCode)
{
}

void RunTimeSoftWarning ([[maybe_unused]] DWORD dwErrorCode)
{
}

void RunTimeError(DWORD dwErrorCode, const char* pExtraErrorString)
{
	// Diagnostic trace
	TraceRuntimeError(dwErrorCode, pExtraErrorString ? pExtraErrorString : "RunTimeError+str");
	// lee - 180407 - now would it not be a good idea to know what the
	// data was that caused the runtime error?
	if ( pExtraErrorString )
	{
		size_t dwUsedChars = 0;
		if ( g_pGlob && g_pGlob->pEXEUnpackDirectory )
			dwUsedChars = strlen(g_pGlob->pEXEUnpackDirectory);

		if ( dwUsedChars > 0 && dwUsedChars + 3 < _MAX_PATH )
		{
			// hack the error string into the large pEXEUnpackDirectory string path
			size_t dwCanUse = _MAX_PATH - 3 - dwUsedChars;
			char pSecretErrorString [ _MAX_PATH ] = {};
			strcpy_s(pSecretErrorString, sizeof(pSecretErrorString), pExtraErrorString);
			pSecretErrorString [ dwCanUse ] = 0; // cut short in case too long
			size_t dwSecretLength = strlen(pSecretErrorString) + 1;
			memcpy(g_pGlob->pEXEUnpackDirectory + dwUsedChars + 1, pSecretErrorString, dwSecretLength);
		}
	}
	RunTimeError ( dwErrorCode );
}

void LastSystemError()
{
	char* lpBuffer = nullptr;
	if (!FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					   nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					   reinterpret_cast<LPSTR>(&lpBuffer), 0, nullptr)) return;
	if (lpBuffer)
	{
		MessageBoxA(nullptr, lpBuffer, "System Error", MB_OK);
		LocalFree(lpBuffer);
	}
}
