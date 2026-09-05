#ifndef _CERROR_H_
#define _CERROR_H_

#include <windows.h>   
#include <windowsx.h>
#include "cruntimeerrors.h"

#ifndef DARKSDK
	#if defined(DBP_TESTS_COMPILATION)
		#define DARKSDK
	#elif defined(DARKSDK_COMPILE)
		#define DARKSDK static
	#else
		#define DARKSDK __declspec ( dllexport )
	#endif
#endif

// Handler Passed into DLL
extern CRuntimeErrorHandler* g_pErrorHandler;

void Error ( const char* szMessage );
void Message ( int iID, const char* szMessage, const char* szTitle );

#define DBP_RECORD_ERROR(code, clue) RunTimeErrorEx(code, clue, __func__, __FILE__, __LINE__)

void			RunTimeError(DWORD dwErrorCode);
void			RunTimeWarning(DWORD dwErrorCode);
void			RunTimeSoftWarning ( DWORD dwErrorCode );
void			RunTimeError(DWORD dwErrorCode, const char* pStrClue);
void			RunTimeErrorEx(DWORD dwErrorCode, const char* pStrClue, const char* pFunctionName = nullptr, const char* pSourceFile = nullptr, uint32_t sourceLine = 0);

// Modern C++20 Diagnostics API
DARKSDK const DBP_DiagnosticContext* GetLastDiagnosticContext();
DARKSDK void ClearLastDiagnosticContext();
DARKSDK LONG WINAPI DBP_HandlePluginException(LPEXCEPTION_POINTERS pExInfo, const char* pContextTag);
DARKSDK void DBP_InstallCrashHandler();
DARKSDK LONG WINAPI DBP_UnhandledExceptionFilter(LPEXCEPTION_POINTERS pExInfo);

#endif // _CERROR_H_