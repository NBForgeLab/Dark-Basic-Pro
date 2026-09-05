//
// CError.cpp - Modernized C++20 Runtime Error & Diagnostic Infrastructure
//

#include "cerror.h"
#include ".\..\core\globstruct.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string_view>
#include <DbgHelp.h>

#pragma comment ( lib, "user32.lib" )
#define DB_PRO 1

// Handler Passed into DLL
CRuntimeErrorHandler* g_pErrorHandler = nullptr;

// Diagnostic thread safety & storage
static SRWLOCK g_DiagLock = SRWLOCK_INIT;
static DBP_DiagnosticContext g_LastDiagnosticContext = {};

const char* GetRuntimeErrorDescription(uint32_t errorCode)
{
	switch (errorCode)
	{
		// General / Engine
		case RUNTIMEERROR_GENERICERROR:				return "Generic error";
		case RUNTIMEERROR_TRIEDTORUNFUNCTIONHEADER: return "Tried to run function header";
		case RUNTIMEERROR_ARRAYACCESSEDOUTOFBOUNDS:	return "Array index out of bounds";
		case RUNTIMEERROR_ARRAYINDEXINVALID:		return "Array index invalid";
		case RUNTIMEERROR_ARRAYEMPTY:				return "Array empty";
		case RUNTIMEERROR_ARRAYTYPEINVALID:			return "Array type invalid";
		case RUNTIMEERROR_ARRAYMUSTBESINGLEDIM:		return "Array must be single dimension";
		case RUNTIMEERROR_NOTENOUGHMEMORY:			return "Not enough memory";
		case RUNTIMEERROR_INVALIDARRAYUSE:			return "Invalid array use";
		case RUNTIMEERROR_FILETOOLARGE:				return "File too large";
		case RUNTIMEERROR_INVALIDFILE:				return "Invalid file";
		case RUNTIMEERROR_FILENOTEXIST:				return "File does not exist";
		case RUNTIMEERROR_FILEEXISTS:				return "File already exists";
		case RUNTIMEERROR_STRINGLENGTHOVERFLOW:		return "String length overflow";
		case RUNTIMEERROR_STACKOVERFLOW:			return "Stack overflow";
		case RUNTIMEERROR_INVALIDARRAY:				return "Invalid array";
		case RUNTIMEERROR_MUSTBEPOSITIVEVALUE:		return "Must be positive value";
		case RUNTIMEERROR_DIVIDEBYZERO:				return "Division by zero";
		case RUNTIMEERROR_SYNCRATEINVALID:			return "Sync rate invalid";
		case RUNTIMEERROR_RANDOMVALUEPOSITIVE:		return "Random value must be positive";
		case RUNTIMEERROR_FILEISLOCKED:				return "File is locked";

		// Sprites
		case RUNTIMEERROR_SPRITEERROR:				return "Sprite error";
		case RUNTIMEERROR_SPRITEILLEGALNUMBER:		return "Sprite number illegal";
		case RUNTIMEERROR_SPRITENOTEXIST:			return "Sprite does not exist";
		case RUNTIMEERROR_SPRITEALREADYTEXISTS:		return "Sprite already exists";

		// Image
		case RUNTIMEERROR_IMAGEERROR:				return "Image error";
		case RUNTIMEERROR_IMAGEILLEGALNUMBER:		return "Image number illegal";
		case RUNTIMEERROR_IMAGENOTEXIST:			return "Image does not exist";
		case RUNTIMEERROR_IMAGEGRABTOOLARGE:		return "Image grab too large";
		case RUNTIMEERROR_IMAGEAREAILLEGAL:			return "Image area illegal";
		case RUNTIMEERROR_IMAGETOOBIGASTEXTURE:		return "Image too big as texture";
		case RUNTIMEERROR_IMAGELOADFAILED:			return "Image load failed";
		case RUNTIMEERROR_IMAGELOCKED:				return "Image is locked";

		// Bitmap
		case RUNTIMEERROR_BITMAPERROR:				return "Bitmap error";
		case RUNTIMEERROR_BITMAPILLEGALNUMBER:		return "Bitmap number illegal";
		case RUNTIMEERROR_BITMAPNOTEXIST:			return "Bitmap does not exist";
		case RUNTIMEERROR_BITMAPLOADFAILED:			return "Bitmap load failed";
		case RUNTIMEERROR_BITMAPSAVEFAILED:			return "Bitmap save failed";
		case RUNTIMEERROR_BITMAPCREATEFAILED:		return "Bitmap creation failed";
		case RUNTIMEERROR_BITMAPZERONODELETE:		return "Cannot delete bitmap zero";

		// Screen / Display
		case RUNTIMEERROR_SCREEN:					return "Screen error";
		case RUNTIMEERROR_SCREENSIZEILLEGAL:		return "Screen size illegal";
		case RUNTIMEERROR_SCREENDEPTHILLEGAL:		return "Screen depth illegal";
		case RUNTIMEERROR_SCREENMODEINVALID:		return "Screen mode invalid";
		case RUNTIMEERROR_NOTSUPPORTDISPLAY:		return "Display mode not supported";

		// Animation
		case RUNTIMEERROR_ANIMERROR:				return "Animation error";
		case RUNTIMEERROR_ANIMNUMBERILLEGAL:		return "Animation number illegal";
		case RUNTIMEERROR_ANIMLOADFAILED:			return "Animation load failed";
		case RUNTIMEERROR_ANIMALREADYEXISTS:		return "Animation already exists";
		case RUNTIMEERROR_ANIMNOTEXIST:				return "Animation does not exist";

		// Sound
		case RUNTIMEERROR_SOUNDERROR:				return "Sound error";
		case RUNTIMEERROR_SOUNDNUMBERILLEGAL:		return "Sound number illegal";
		case RUNTIMEERROR_SOUNDLOADFAILED:			return "Sound load failed";
		case RUNTIMEERROR_SOUNDALREADYEXISTS:		return "Sound already exists";
		case RUNTIMEERROR_SOUNDNOTEXIST:			return "Sound does not exist";

		// Music
		case RUNTIMEERROR_MUSICERROR:				return "Music error";
		case RUNTIMEERROR_MUSICNUMBERILLEGAL:		return "Music number illegal";
		case RUNTIMEERROR_MUSICLOADFAILED:			return "Music load failed";
		case RUNTIMEERROR_MUSICALREADYEXISTS:		return "Music already exists";
		case RUNTIMEERROR_MUSICNOTEXIST:			return "Music does not exist";

		// Memblocks
		case RUNTIMEERROR_MEMBLOCKRANGEILLEGAL:		return "Memblock number illegal";
		case RUNTIMEERROR_MEMBLOCKALREADYEXISTS:	return "Memblock already exists";
		case RUNTIMEERROR_MEMBLOCKNOTEXIST:			return "Memblock does not exist";
		case RUNTIMEERROR_MEMBLOCKCREATIONFAILED:	return "Memblock creation failed";
		case RUNTIMEERROR_MEMBLOCKOUTSIDERANGE:		return "Memblock offset outside range";
		case RUNTIMEERROR_MEMBLOCKSIZEINVALID:		return "Memblock size invalid";
		case RUNTIMEERROR_MEMBLOCKNOTABYTE:			return "Value is not a valid byte (0-255)";
		case RUNTIMEERROR_MEMBLOCKNOTAWORD:			return "Value is not a valid word (0-65535)";
		case RUNTIMEERROR_MEMBLOCKNOTADWORD:		return "Value is not a valid dword";

		// Basic3D / Objects
		case RUNTIMEERROR_B3DERROR:					return "3D error";
		case RUNTIMEERROR_B3DMESHNUMBERILLEGAL:		return "3D mesh number illegal";
		case RUNTIMEERROR_B3DMESHLOADFAILED:		return "3D mesh load failed";
		case RUNTIMEERROR_B3DMESHNOTEXIST:			return "3D mesh does not exist";
		case RUNTIMEERROR_B3DMODELNUMBERILLEGAL:	return "3D object number illegal";
		case RUNTIMEERROR_B3DMODELALREADYEXISTS:	return "3D object already exists";
		case RUNTIMEERROR_B3DMODELNOTEXISTS:		return "3D object does not exist";
		case RUNTIMEERROR_B3DOBJECTLOADFAILED:		return "3D object load failed";
		case RUNTIMEERROR_B3DMUSTUSEDBOEXTENSION:	return "Must use .dbo extension";
		case RUNTIMEERROR_LIMBNUMBERILLEGAL:		return "Limb number illegal";
		case RUNTIMEERROR_LIMBNOTEXIST:				return "Limb does not exist";
		case RUNTIMEERROR_LIMBALREADYEXISTS:		return "Limb already exists";
		case RUNTIMEERROR_B3DMESHTOOLARGE:			return "3D mesh too large";

		// File
		case RUNTIMEERROR_CANNOTSCANCURRENTDIR:		return "Cannot scan current directory";
		case RUNTIMEERROR_NOMOREFILESINDIR:			return "No more files in directory";
		case RUNTIMEERROR_PATHCANNOTBEFOUND:		return "Path cannot be found";
		case RUNTIMEERROR_CANNOTMAKEFILE:			return "Cannot create file";
		case RUNTIMEERROR_CANNOTDELETEFILE:			return "Cannot delete file";
		case RUNTIMEERROR_CANNOTCOPYFILE:			return "Cannot copy file";
		case RUNTIMEERROR_CANNOTRENAMEFILE:			return "Cannot rename file";
		case RUNTIMEERROR_CANNOTMOVEFILE:			return "Cannot move file";
		case RUNTIMEERROR_CANNOTMAKEDIR:			return "Cannot make directory";
		case RUNTIMEERROR_CANNOTDELETEDIR:			return "Cannot delete directory";
		case RUNTIMEERROR_CANNOTEXECUTEFILE:		return "Cannot execute file";
		case RUNTIMEERROR_CANNOTOPENFILEFORREADING: return "Cannot open file for reading";
		case RUNTIMEERROR_CANNOTOPENFILEFORWRITING: return "Cannot open file for writing";
		case RUNTIMEERROR_FILEALREADYOPEN:			return "File already open";
		case RUNTIMEERROR_FILENOTOPEN:				return "File not open";
		case RUNTIMEERROR_CANNOTREADFROMFILE:		return "Cannot read from file";
		case RUNTIMEERROR_CANNOTWRITETOFILE:		return "Cannot write to file";
		case RUNTIMEERROR_FILENUMBERINVALID:		return "File number invalid";

		// Checklist
		case RUNTIMEERROR_CHECKLISTILLEGALNUMBER:	return "Checklist index illegal";
		case RUNTIMEERROR_CHECKLISTNUMBERWRONG:		return "Checklist number wrong";
		case RUNTIMEERROR_CHECKLISTONLYVALUES:		return "Checklist contains only values";
		case RUNTIMEERROR_CHECKLISTONLYSTRINGS:		return "Checklist contains only strings";
		case RUNTIMEERROR_CHECKLISTNOTEXIST:		return "Checklist does not exist";

		// System
		case RUNTIMEERROR_SYSCOULDNOTLOADDLL:		return "System could not load DLL";
		case RUNTIMEERROR_SYSDLLNOTEXIST:			return "System DLL does not exist";
		case RUNTIMEERROR_SYSDLLALREADYEXISTS:		return "System DLL already exists";
		case RUNTIMEERROR_SYSDLLCALLFAILED:			return "System DLL call failed";
		case RUNTIMEERROR_SYSDLLINDEXINVALID:		return "System DLL index invalid";

		default:									return "Unknown runtime error";
	}
}

DARKSDK const DBP_DiagnosticContext* GetLastDiagnosticContext()
{
	return &g_LastDiagnosticContext;
}

DARKSDK void ClearLastDiagnosticContext()
{
	AcquireSRWLockExclusive(&g_DiagLock);
	memset(&g_LastDiagnosticContext, 0, sizeof(g_LastDiagnosticContext));
	ReleaseSRWLockExclusive(&g_DiagLock);
}

void RunTimeErrorEx(DWORD dwErrorCode, const char* pStrClue, const char* pFunctionName, const char* pSourceFile, uint32_t sourceLine)
{
	// 1. High-resolution timestamp
	LARGE_INTEGER count{}, freq{};
	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&freq);
	uint64_t uTimestampUs = (freq.QuadPart > 0)
		? static_cast<uint64_t>(count.QuadPart * 1000000ULL / freq.QuadPart)
		: 0;

	DWORD dwThreadId = GetCurrentThreadId();
	const char* desc = GetRuntimeErrorDescription(dwErrorCode);

	// 2. Thread-safe recording in structured diagnostic context
	AcquireSRWLockExclusive(&g_DiagLock);
	g_LastDiagnosticContext.errorCode = dwErrorCode;
	g_LastDiagnosticContext.threadId = dwThreadId;
	g_LastDiagnosticContext.timestampUs = uTimestampUs;
	strncpy_s(g_LastDiagnosticContext.description, sizeof(g_LastDiagnosticContext.description), desc, _TRUNCATE);

	if (pStrClue)
	{
		strncpy_s(g_LastDiagnosticContext.clue, sizeof(g_LastDiagnosticContext.clue), pStrClue, _TRUNCATE);
	}
	else
	{
		g_LastDiagnosticContext.clue[0] = 0;
	}

	if (pFunctionName)
	{
		strncpy_s(g_LastDiagnosticContext.sourceFunction, sizeof(g_LastDiagnosticContext.sourceFunction), pFunctionName, _TRUNCATE);
	}
	else
	{
		g_LastDiagnosticContext.sourceFunction[0] = 0;
	}

	if (pSourceFile)
	{
		strncpy_s(g_LastDiagnosticContext.sourceFile, sizeof(g_LastDiagnosticContext.sourceFile), pSourceFile, _TRUNCATE);
	}
	else
	{
		g_LastDiagnosticContext.sourceFile[0] = 0;
	}
	g_LastDiagnosticContext.sourceLine = sourceLine;

	// 3. Assign error code to engine error handler pointer (ABI-compatible)
	if (g_pErrorHandler)
	{
		g_pErrorHandler->dwErrorCode = dwErrorCode;
	}

	// 4. Non-blocking real-time diagnostic output via OutputDebugStringA
	char szDbg[1024];
	snprintf(szDbg, sizeof(szDbg), "[DBP_ERROR] Code %lu (%s): %s%s%s%s%s%s%u (Thread: %lu, Time: %llu us)\n",
		dwErrorCode,
		desc,
		pStrClue ? pStrClue : "No detail",
		pFunctionName ? " in " : "",
		pFunctionName ? pFunctionName : "",
		pSourceFile ? " at " : "",
		pSourceFile ? pSourceFile : "",
		sourceLine ? ":" : "",
		sourceLine,
		dwThreadId,
		static_cast<unsigned long long>(uTimestampUs));

	OutputDebugStringA(szDbg);

	// In debug mode or if DBP_TRACE_ERROR env is set, output to stderr (without leaving files on disk)
	if (IsDebuggerPresent() || getenv("DBP_TRACE_ERROR"))
	{
		fputs(szDbg, stderr);
		fflush(stderr);
	}

	ReleaseSRWLockExclusive(&g_DiagLock);

	// 5. Backwards compatibility for DarkEXE.cpp packaged string clue
	if (pStrClue && g_pGlob && g_pGlob->pEXEUnpackDirectory)
	{
		size_t dwUsedChars = strlen(g_pGlob->pEXEUnpackDirectory);
		if (dwUsedChars > 0 && dwUsedChars + 3 < _MAX_PATH)
		{
			size_t dwCanUse = _MAX_PATH - 3 - dwUsedChars;
			char pSecretErrorString[_MAX_PATH] = {};
			strncpy_s(pSecretErrorString, sizeof(pSecretErrorString), pStrClue, dwCanUse);
			pSecretErrorString[dwCanUse] = 0;
			size_t dwSecretLength = strlen(pSecretErrorString) + 1;
			memcpy(g_pGlob->pEXEUnpackDirectory + dwUsedChars + 1, pSecretErrorString, dwSecretLength);
		}
	}
}

void RunTimeError(DWORD dwErrorCode)
{
	RunTimeErrorEx(dwErrorCode, nullptr, nullptr, nullptr, 0);
}

void RunTimeError(DWORD dwErrorCode, const char* pStrClue)
{
	RunTimeErrorEx(dwErrorCode, pStrClue, nullptr, nullptr, 0);
}

void RunTimeWarning(DWORD dwErrorCode)
{
	const char* desc = GetRuntimeErrorDescription(dwErrorCode);
	char szDbg[512];
	snprintf(szDbg, sizeof(szDbg), "[DBP_WARN] Code %lu (%s) (Thread: %lu)\n",
		dwErrorCode, desc, GetCurrentThreadId());
	OutputDebugStringA(szDbg);
}

void RunTimeSoftWarning(DWORD dwErrorCode)
{
	const char* desc = GetRuntimeErrorDescription(dwErrorCode);
	char szDbg[512];
	snprintf(szDbg, sizeof(szDbg), "[DBP_WARN_SOFT] Code %lu (%s) (Thread: %lu)\n",
		dwErrorCode, desc, GetCurrentThreadId());
	OutputDebugStringA(szDbg);
}

void Error([[maybe_unused]] const char* szMessage)
{
	#if DB_PRO
		if (g_pErrorHandler && g_pErrorHandler->dwErrorCode == 0)
			RunTimeError(RUNTIMEERROR_GENERICERROR);
	#else
		ShowCursor(true);
		MessageBoxA(nullptr, szMessage, "DarkBASIC Pro Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
	#endif
}

void Message([[maybe_unused]] int iID, const char* szMessage, const char* szTitle)
{
	MessageBoxA(nullptr, szMessage, szTitle, MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_TOPMOST);
}

void LastSystemError()
{
	char* lpBuffer = nullptr;
	if (!FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					   nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					   reinterpret_cast<LPSTR>(&lpBuffer), 0, nullptr)) return;
	if (lpBuffer)
	{
		char szDbg[1024];
		snprintf(szDbg, sizeof(szDbg), "[DBP_SYSTEM_ERROR] %s\n", lpBuffer);
		OutputDebugStringA(szDbg);
		LocalFree(lpBuffer);
	}
}

static const char* DBP_GetExceptionCodeName(DWORD code);

DARKSDK LONG WINAPI DBP_HandlePluginException(LPEXCEPTION_POINTERS pExInfo, const char* pContextTag)
{
	if (!pExInfo || !pExInfo->ExceptionRecord)
		return EXCEPTION_CONTINUE_SEARCH;

	DWORD code = pExInfo->ExceptionRecord->ExceptionCode;
	void* addr = pExInfo->ExceptionRecord->ExceptionAddress;

	char moduleName[MAX_PATH] = "UnknownModule";
	HMODULE hMod = nullptr;
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						   reinterpret_cast<LPCSTR>(addr), &hMod) && hMod)
	{
		GetModuleFileNameA(hMod, moduleName, MAX_PATH);
	}

	char szClue[512] = {};
	snprintf(szClue, sizeof(szClue), "Exception 0x%08lX (%s) at %p in %s",
		code, DBP_GetExceptionCodeName(code), addr, moduleName);

	RunTimeErrorEx(RUNTIMEERROR_GENERICERROR, szClue, pContextTag ? pContextTag : "DBP_HandlePluginException", moduleName, 0);

	char szDbg[1024];
	snprintf(szDbg, sizeof(szDbg),
		"[DBP_CRITICAL] Exception 0x%08lX at %p in %s (Context: %s)\n",
		code, addr, moduleName, pContextTag ? pContextTag : "None");

	OutputDebugStringA(szDbg);
	fputs(szDbg, stderr);
	fflush(stderr);

	return EXCEPTION_EXECUTE_HANDLER;
}

static const char* DBP_GetExceptionCodeName(DWORD code)
{
	switch (code)
	{
		case 0xC0000005: return "ACCESS_VIOLATION";
		case 0xC0000008: return "INVALID_HANDLE";
		case 0xC000001D: return "ILLEGAL_INSTRUCTION";
		case 0xC0000025: return "NONCONTINUABLE_EXCEPTION";
		case 0xC000008C: return "ARRAY_BOUNDS_EXCEEDED";
		case 0xC000008D: return "FLOAT_DENORMAL_OPERAND";
		case 0xC000008E: return "FLOAT_DIVIDE_BY_ZERO";
		case 0xC000008F: return "FLOAT_INEXACT_RESULT";
		case 0xC0000090: return "FLOAT_INVALID_OPERATION";
		case 0xC0000091: return "FLOAT_OVERFLOW";
		case 0xC0000092: return "FLOAT_STACK_CHECK";
		case 0xC0000093: return "FLOAT_UNDERFLOW";
		case 0xC0000094: return "INTEGER_DIVIDE_BY_ZERO";
		case 0xC0000095: return "INTEGER_OVERFLOW";
		case 0xC0000096: return "PRIVILEGED_INSTRUCTION";
		case 0xC00000FD: return "STACK_OVERFLOW";
		case 0x80000003: return "BREAKPOINT";
		default:         return "UNKNOWN_EXCEPTION";
	}
}

DARKSDK void DBP_InstallCrashHandler()
{
	SetUnhandledExceptionFilter(DBP_UnhandledExceptionFilter);
}

DARKSDK LONG WINAPI DBP_UnhandledExceptionFilter(LPEXCEPTION_POINTERS pExInfo)
{
	char szExePath[MAX_PATH] = {};
	GetModuleFileNameA(NULL, szExePath, MAX_PATH);

	const char* pBaseName = strrchr(szExePath, '\\');
	if (!pBaseName) pBaseName = strrchr(szExePath, '/');
	pBaseName = pBaseName ? pBaseName + 1 : szExePath;

	char szAppName[MAX_PATH] = "DBProApp";
	if (pBaseName && pBaseName[0])
	{
		strncpy_s(szAppName, sizeof(szAppName), pBaseName, _TRUNCATE);
		char* pDot = strrchr(szAppName, '.');
		if (pDot && _stricmp(pDot, ".exe") == 0)
			*pDot = '\0';
	}

	SYSTEMTIME st = {};
	GetLocalTime(&st);
	char szTimestamp[64] = {};
	snprintf(szTimestamp, sizeof(szTimestamp), "%04u%02u%02u_%02u%02u%02u_%03u",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	char szDmpFile[MAX_PATH] = {};
	char szTxtFile[MAX_PATH] = {};
	snprintf(szDmpFile, sizeof(szDmpFile), "%s_Crash_%s.dmp", szAppName, szTimestamp);
	snprintf(szTxtFile, sizeof(szTxtFile), "%s_Crash_%s.txt", szAppName, szTimestamp);

	DWORD exCode = (pExInfo && pExInfo->ExceptionRecord) ? pExInfo->ExceptionRecord->ExceptionCode : 0;
	void* exAddr = (pExInfo && pExInfo->ExceptionRecord) ? pExInfo->ExceptionRecord->ExceptionAddress : nullptr;

	char faultModule[MAX_PATH] = "UnknownModule";
	if (exAddr)
	{
		HMODULE hMod = nullptr;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(exAddr), &hMod) && hMod)
		{
			GetModuleFileNameA(hMod, faultModule, MAX_PATH);
		}
	}

	BOOL bDumpWritten = FALSE;
	DWORD dwDumpError = ERROR_SUCCESS;

	// 1. Create MiniDump via dbghelp.dll
	HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
	if (hDbgHelp)
	{
		typedef BOOL(WINAPI* PFN_MiniDumpWriteDump)(
			HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
			PMINIDUMP_EXCEPTION_INFORMATION,
			PMINIDUMP_USER_STREAM_INFORMATION,
			PMINIDUMP_CALLBACK_INFORMATION);

		PFN_MiniDumpWriteDump pfnMiniDumpWriteDump =
			reinterpret_cast<PFN_MiniDumpWriteDump>(GetProcAddress(hDbgHelp, "MiniDumpWriteDump"));
		if (pfnMiniDumpWriteDump)
		{
			HANDLE hFile = CreateFileA(szDmpFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				MINIDUMP_EXCEPTION_INFORMATION mdei{};
				mdei.ThreadId = GetCurrentThreadId();
				mdei.ExceptionPointers = pExInfo;
				mdei.ClientPointers = FALSE;

				bDumpWritten = pfnMiniDumpWriteDump(
					GetCurrentProcess(),
					GetCurrentProcessId(),
					hFile,
					MiniDumpNormal,
					pExInfo ? &mdei : nullptr,
					nullptr,
					nullptr);

				if (!bDumpWritten)
				{
					dwDumpError = GetLastError();
				}

				CloseHandle(hFile);
			}
			else
			{
				dwDumpError = GetLastError();
			}
		}
		else
		{
			dwDumpError = ERROR_PROC_NOT_FOUND;
		}
		FreeLibrary(hDbgHelp);
	}
	else
	{
		dwDumpError = GetLastError();
	}

	// 2. Write Diagnostic Report Text
	const DBP_DiagnosticContext* ctx = GetLastDiagnosticContext();
	FILE* fp = nullptr;
	if (fopen_s(&fp, szTxtFile, "w") == 0 && fp)
	{
		fprintf(fp, "=== DarkBASIC Pro Crash Diagnostic Report ===\n");
		fprintf(fp, "Application:       %s (%s)\n", szAppName, szExePath[0] ? szExePath : "Unknown");
		fprintf(fp, "Timestamp:         %s\n", szTimestamp);
		fprintf(fp, "Process ID:        %lu\n", GetCurrentProcessId());
		fprintf(fp, "Thread ID:         %lu\n", GetCurrentThreadId());
		fprintf(fp, "Exception Code:    0x%08lX (%s)\n", exCode, DBP_GetExceptionCodeName(exCode));
		fprintf(fp, "Exception Address: %p\n", exAddr);
		fprintf(fp, "Faulting Module:   %s\n", faultModule);
		fprintf(fp, "MiniDump File:     %s\n", szDmpFile);
		fprintf(fp, "MiniDump Written:  %s\n", bDumpWritten ? "yes" : "no");
		if (!bDumpWritten)
		{
			fprintf(fp, "MiniDump Error:    %lu\n", dwDumpError);
		}
		fprintf(fp, "\n--- Last Recorded Diagnostic Context ---\n");
		if (ctx && (ctx->errorCode != 0 || ctx->sourceFunction[0] || ctx->sourceFile[0] || ctx->sourceLine != 0))
		{
			fprintf(fp, "Error Code:        %u (%s)\n", ctx->errorCode, ctx->description[0] ? ctx->description : "None");
			fprintf(fp, "Clue:              %s\n", ctx->clue[0] ? ctx->clue : "None");
			fprintf(fp, "Source Function:   %s\n", ctx->sourceFunction[0] ? ctx->sourceFunction : "Unknown");
			fprintf(fp, "Source File:       %s\n", ctx->sourceFile[0] ? ctx->sourceFile : "Unknown");
			fprintf(fp, "Source Line:       %u\n", ctx->sourceLine);
			fprintf(fp, "Thread ID:         %u\n", ctx->threadId);
			fprintf(fp, "Timestamp (us):    %llu\n", static_cast<unsigned long long>(ctx->timestampUs));
		}
		else
		{
			fprintf(fp, "Source Function:   None\n");
			fprintf(fp, "Source File:       None\n");
			fprintf(fp, "Source Line:       0\n");
			fprintf(fp, "No prior runtime error recorded in diagnostic context.\n");
		}
		fprintf(fp, "==============================================\n");
		fclose(fp);
	}

	// 3. Debug Output
	char szDbg[1024];
	snprintf(szDbg, sizeof(szDbg),
		"[DBP_CRASH] Process '%s' crashed! Exception 0x%08lX (%s) at %p in module '%s'. MiniDump: %s (%s), Report: %s\n",
		szAppName, exCode, DBP_GetExceptionCodeName(exCode), exAddr, faultModule, szDmpFile, bDumpWritten ? "written" : "failed", szTxtFile);
	OutputDebugStringA(szDbg);
	fputs(szDbg, stderr);
	fflush(stderr);

	return EXCEPTION_EXECUTE_HANDLER;
}
