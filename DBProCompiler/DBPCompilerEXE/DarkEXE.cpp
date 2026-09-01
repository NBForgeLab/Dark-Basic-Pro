//
// DarkEXE
//

// Internal Includes
#include "RuntimePackageBootstrap.h"
#include "windows.h"
#include "direct.h"
#include "../DBPCompiler/TextConvert.h"
#include "../DBPCompiler/VFSHooks.h"
#include "../DBPCompiler/CrashHandler.h"
#include "../DBPCompiler/MemoryPE.h"
#include "time.h"

#include <filesystem>
#include <memory>
#include <string>
#include "../DBPCompiler/PluginRegistry.h"

// Defines and Externs
#include "DarkEXE.h"
#include "resource.h"
#include ".\..\..\Dark Basic Public Shared\Dark basic Pro SDK\Shared\Core\globstruct.h"

// Include Memory Manager & Globals
#include ".\..\..\Dark Basic Public Shared\Dark basic Pro SDK\Shared\MemoryManager\DarkMemoryManager.h"
char g_MM_DLLName [ 256 ] = { "Main" };
char g_MM_FunctionName [ 256 ]= { "<none>" };

// External Includes
#include "..\DBPCompiler\EXEBlock.h"

// Custom Includes
#include "..\DBPCompiler\macros.h"

// Interal DarkEXE Data Variables
LPSTR								gRefCommandLineString=nullptr;
char								gUnpackDirectory[_MAX_PATH];
char								gpDBPDataName[_MAX_PATH];

// EXECUTABLE Class
CEXEBlock							CEXE;

// DebugDump needs this to get at useful debug data (U5.8)
extern GlobStruct*					g_pGlob;

// Temporary Window while loading DLLs
HWND								g_hTempWindow = nullptr;

// IGLOADER can send WM_SETTEXT to the temp window, DBP executables attempt to 'EMBED'
HWND								g_igLoader_HWND = nullptr;

// LOAD EXE DATA AND RUN

bool RunProgram(HINSTANCE hInstance, LPSTR* pReturnError)
{
	// Result Var
	bool bResult=true;

	// Make program
	bResult=CEXE.Init(hInstance, bResult, pReturnError, gRefCommandLineString);

	// LEEADD - 221008 - U71 - SIMULATE EXTERNAL DEBUGGER (not for release code)
	//#define SIMULATEEXTERNALDEBUGGER
	#ifdef SIMULATEEXTERNALDEBUGGER
		// 1. external debugger creates mutex
		const std::string pUniqueMutexNameForExternalDebugger = std::string(CEXE.m_pAbsoluteAppFile) + "(Mutex)";
		HANDLE pExternalDebuggerCreatesMutex = CreateMutexA( nullptr, FALSE, pUniqueMutexNameForExternalDebugger.c_str() );
		// 2. external debugger writes DEBUGME to the shared string file map
		LPSTR pDebugMeString = "debugme";
		const std::string pUniqueFileMapName = CEXE.m_AbsoluteAppFile + "(FileMap)";
		DWORD dwWriteDataSize = strlen(pDebugMeString)+1;
		HANDLE hWriteFileMap = CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,dwWriteDataSize+4,TextConvert::UTF8ToUTF16(pUniqueFileMapName).c_str());
		if(hWriteFileMap != nullptr && hWriteFileMap != INVALID_HANDLE_VALUE)
		{
			LPVOID lpWriteVoid = MapViewOfFile(hWriteFileMap,FILE_MAP_WRITE,0,0,dwWriteDataSize+4);
			if(lpWriteVoid)
			{
				// Copy to Virtual File
				*(DWORD*)lpWriteVoid = dwWriteDataSize;
				LPSTR pWriteData = pDebugMeString;
				memcpy((LPSTR)lpWriteVoid+4, pWriteData, dwWriteDataSize);
				UnmapViewOfFile(lpWriteVoid);
			}
			CloseHandle(hWriteFileMap);
		}
		// 3. external debugger executes DBP application
		//    shellex DBP APP
		// 4. external debugger waits for string in shared filemap to change
		//    str <> "debugme"
		// 5. external debugger takes new string and extracts globstruct ptr from it
		//    ptr=(DWORD*);
		// 6. external debugger can use globstruct ptr to access all other internal data
		//    globstruct=ptr
		// 7. external debugger closes the shared file map once string used
		//    CloseHandle(hWriteFileMap)
	#endif

	// LEEADD - 221008 - U71 - EXTERNAL DEBUGGER SUPPORT
	{
		// kernel object names cannot contain path separators
		std::string pUniqueMutexName = CEXE.m_AbsoluteAppFile + "(Mutex)";
		for ( char& c : pUniqueMutexName )
			if ( c == ':' || c == '@' || c == '/' ) c = '_';
		HANDLE pAppMutex = OpenMutexW ( MUTEX_ALL_ACCESS, FALSE, TextConvert::UTF8ToUTF16(pUniqueMutexName).c_str() );
		if ( pAppMutex )
		{
			// it appears another process has already created an identical mutex
			// (which can happen if multiple programs with the same name are running)
			// so we then check a shared storage location to see whether debugging
			// has been requested by the 'external process' that may have created a mutex
			std::string pSharedStringStorage;
			// kernel object names cannot contain path separators
			std::string pUniqueFileMapName = CEXE.m_AbsoluteAppFile + "(FileMap)";
			for ( char& c : pUniqueFileMapName )
				if ( c == ':' || c == '@' || c == '/' ) c = '_';

			HANDLE hFileMap = OpenFileMappingW(FILE_MAP_READ,FALSE,TextConvert::UTF8ToUTF16(pUniqueFileMapName).c_str());
			if(hFileMap)
			{
				LPVOID lpVoid = MapViewOfFile(hFileMap,FILE_MAP_READ,0,0,0);
				if(lpVoid)
				{
					DWORD dwDataSize = 0;
					dwDataSize = *((LPDWORD) lpVoid);
					if ( dwDataSize>0 )
					{
						const char* pPayload = reinterpret_cast<const char*>(lpVoid) + 4;
						pSharedStringStorage.assign(pPayload, strnlen(pPayload, dwDataSize));
					}
					UnmapViewOfFile(lpVoid);
				}
				CloseHandle(hFileMap);
			}
			if ( _stricmp(pSharedStringStorage.c_str(), "debugme") == 0 )
			{
				// a mutex for this app exists and the shared storage says it wants to
				// DEBUG this application, so we pause here until we can own the mutex
				// (which is achieved by the current owner of the mutex releasing it)

				// but before we pause, we must replace the string in the shared storage
				// so some new data which tells the debugger we are here now and waiting
				// for ownership of the mutex so we can start running the program. In 
				// order to keep this section simple, we simply pass in the memory address
				// of the GLOBSTRUCT data, which includes all the information needed
				DWORD dwWriteDataSize = sizeof(uintptr_t);
				HANDLE hWriteFileMap = CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,dwWriteDataSize,TextConvert::UTF8ToUTF16(pUniqueFileMapName).c_str());
				if(hWriteFileMap != nullptr && hWriteFileMap != INVALID_HANDLE_VALUE)
				{
					LPVOID lpWriteVoid = MapViewOfFile(hWriteFileMap,FILE_MAP_WRITE,0,0,dwWriteDataSize);
					if(lpWriteVoid)
					{
						// Copy to Virtual File
						*(uintptr_t*)lpWriteVoid = (uintptr_t)g_pGlob;
						UnmapViewOfFile(lpWriteVoid);
					}
					CloseHandle(hWriteFileMap);
				}

				// friendly message
				MessageBoxW ( nullptr, TextConvert::UTF8ToUTF16(pUniqueMutexName).c_str(), L"DBP App has deposited the glob struct in the filemap as a DWORD, and now wants to OWN this mutex...give it to me!", MB_OK );

				// now wait for the external debugger to release the mutex
				DWORD dwWaitResult = WaitForSingleObject ( pAppMutex, 5000L );
				switch ( dwWaitResult ) 
				{
					// The thread got mutex ownership.
					case WAIT_OBJECT_0:		dwWaitResult=dwWaitResult; break;
					case WAIT_TIMEOUT:		dwWaitResult=dwWaitResult; break;
					case WAIT_ABANDONED:	dwWaitResult=dwWaitResult; break;
				}
			}
			else
			{
				// for whatever reason, the shared string did NOT contain the text
				// which would trigger this application to seek ownership of the 
				// mutex and so can carry on without attempting to own this mutex
				// as it was probably created by another app with the same mutex name
			}

			// close the mutex handle as we are finished with it
			CloseHandle ( pAppMutex );
		}
	}

	// Run the EXE Program
	bResult=CEXE.Run(bResult);

	// Report Any Runtime Errors
	DWORD dwRTError=static_cast<DWORD>(CEXE.m_dwRuntimeErrorDWORD);
	DWORD dwRTErrorLine=static_cast<DWORD>(CEXE.m_dwRuntimeErrorLineDWORD);
	if(dwRTError>0)
	{
		// create report string and store
		*pReturnError = new char[1024];
		LPSTR pRuntimeErrorString = nullptr;
		if(CEXE.m_pRuntimeErrorStringsArray) pRuntimeErrorString = (LPSTR)CEXE.m_pRuntimeErrorStringsArray[dwRTError];
		sprintf_s(*pReturnError, 1024, "Runtime Error %d - %s at line %d", dwRTError, pRuntimeErrorString, dwRTErrorLine);
		bResult=false;
	}

	// Return Result
	return bResult;
}

LRESULT CALLBACK TempWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_SETTEXT:
		{
			// handle HWND message
			if ( g_igLoader_HWND==nullptr )
			{
				LPSTR pIncomingStr = (LPSTR)lParam;
				if ( pIncomingStr )
				{
					if ( strlen ( pIncomingStr ) > 7 )
					{
						// extract HWND from lParam ("[iglwnd]1234567" style text)
						g_igLoader_HWND = reinterpret_cast<HWND>(static_cast<uintptr_t>(_atoi64(pIncomingStr + 7)));

						// when get igl parent handle, change window to no-size, no-border, nout
						DWORD dwWindowStyle = 0;
						SetWindowLongPtr ( hWnd, GWL_STYLE, static_cast<LONG_PTR>(dwWindowStyle) );
						SetWindowPos ( hWnd, nullptr, 0, 0, 640, 480, 0 );
						UpdateWindow ( hWnd );
					}
				}
			}
			return 0;
		}

		case WM_CLOSE:
			OutputDebugStringA("[DarkEXE] TempWindowProc WM_CLOSE received (ignored)\n");
			break;
	}

	// Default Action
    return DefWindowProc(hWnd, message, wParam, lParam);
}


void CreateTempWindow ( HINSTANCE hInstance, LPSTR pFullAppPath, DWORD dwWindowWidth, DWORD dwWindowHeight )
{
	// Initial Window (ahead of lengthy DLL and media load)
	const std::string pAppName = std::filesystem::path(pFullAppPath).stem().string();

	// Register window
	WNDCLASSA wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = TempWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = nullptr;
	wc.hCursor = nullptr;
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = pAppName.c_str();
	RegisterClassA( &wc );

	// Create Window
	g_hTempWindow = CreateWindowExA( 
										0,                      // no extended styles           
										pAppName.c_str(),		// class name                   
										pAppName.c_str(),		// window name                  
										0,					    // no WS_OVERLAPPEDWINDOW for pure window for igloader browser
										-50000,					// temp window hide completely! 
										-50000,					//  
										dwWindowWidth,			// default width                
										dwWindowHeight,			// default height               
										(HWND) nullptr,            // no parent or owner window    
										(HMENU) nullptr,           // class menu used              
										hInstance,              // instance handle              
										nullptr);                  // no window creation data      
	
	// Return if not valid
	if (!g_hTempWindow) 
		return; 

	// Show the created window, and issue a WM_PAINT message
	ShowWindow(g_hTempWindow, SW_HIDE ); // 080909 - for hidden applications, this flashes taskbar icon quickly, was SW_NORMAL); 
	UpdateWindow(g_hTempWindow); 
}


void DeleteContentsOfDBPDATA(bool bOnlyIfOlderThan2DAYS)
{
	std::error_code ec;
	const auto currentDir = std::filesystem::current_path(ec);
	if (ec || !std::filesystem::exists(currentDir, ec))
		return;

	const auto now = std::filesystem::file_time_type::clock::now();
	for (const auto& entry : std::filesystem::directory_iterator(currentDir, ec))
	{
		if (ec) break;
		if (bOnlyIfOlderThan2DAYS)
		{
			auto writeTime = entry.last_write_time(ec);
			if (ec) continue;
			auto ageDuration = std::chrono::duration_cast<std::chrono::hours>(now - writeTime);
			if (ageDuration.count() < 48) // 2 days
				continue;
		}

		std::error_code rmEc;
		std::filesystem::remove_all(entry.path(), rmEc);
	}
}

void MakeOrEnterUniqueDBPDATA(void)
{
	strcpy_s(gpDBPDataName, "dbpdata");
	std::error_code ec;
	std::filesystem::create_directories(gpDBPDataName, ec);
}

void DeleteAllOldDBPDATAFolders(void)
{
	strcpy_s(gpDBPDataName, "dbpdata");
	std::error_code ec;
	for (DWORD dwBuildID = 2; dwBuildID < 1000000; ++dwBuildID)
	{
		char folderName[_MAX_PATH];
		sprintf_s(folderName, _MAX_PATH, "dbpdata%lu", dwBuildID);
		if (!std::filesystem::exists(folderName, ec))
			break;

		std::filesystem::remove_all(folderName, ec);
	}
}

// DUMP DEBUG REPORT

struct RuntimeExecutionFailure
{
	DWORD exceptionCode = 0U;
	const void* exceptionAddress = nullptr;
	ULONG_PTR accessType = 0U;
	const void* accessedAddress = nullptr;
};

int CaptureRuntimeExecutionFailure(
	EXCEPTION_POINTERS* const exception,
	RuntimeExecutionFailure* const failure) noexcept
{
	if (exception != nullptr && exception->ExceptionRecord != nullptr &&
		failure != nullptr)
	{
		failure->exceptionCode = exception->ExceptionRecord->ExceptionCode;
		failure->exceptionAddress =
			exception->ExceptionRecord->ExceptionAddress;
		if (exception->ExceptionRecord->NumberParameters >= 2U)
		{
			failure->accessType =
				exception->ExceptionRecord->ExceptionInformation[0];
			failure->accessedAddress = reinterpret_cast<const void*>(
				exception->ExceptionRecord->ExceptionInformation[1]);
		}
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

bool TryRunProgram(
	HINSTANCE hInstance,
	LPSTR* const returnError,
	bool* const runResult,
	RuntimeExecutionFailure* const failure)
{
	__try
	{
		*runResult = RunProgram(hInstance, returnError);
		return true;
	}
	__except(CaptureRuntimeExecutionFailure(
		GetExceptionInformation(), failure))
	{
		return false;
	}
}

void DumpDebugReport (
	const RuntimeExecutionFailure& failure,
	const char* const runtimeMessage )
{
	// Setup Report (by date and time)
	char pReportDate [ _MAX_PATH ];
	_strdate ( pReportDate );
	for ( DWORD i=0; i<strlen(pReportDate); i++ )
		if ( pReportDate[i]=='/' )
			pReportDate[i]='_';

	// Always place the report beside the application. CEXE::Init temporarily
	// changes the process working directory while loading packaged assets, so a
	// cwd-relative report can otherwise be deleted with the unpack directory.
	const std::filesystem::path applicationPath{CEXE.m_AbsoluteAppFile};
	const auto reportPath = applicationPath.parent_path() /
		(std::string{"CrashOn_"} + pReportDate + ".txt");
	const auto reportPathText = reportPath.string();
	char pReportFile [ _MAX_PATH ];
	strcpy_s(pReportFile, _MAX_PATH, reportPathText.c_str());

	// One report represents one failure. Do not retain exception fields from an
	// earlier failure that happened on the same calendar day.
	const HANDLE reportFile = CreateFileA(
		pReportFile,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (reportFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(reportFile);
	}

	// Create Report File (by date and time)
	char pLineToReport [ _MAX_PATH ];
	sprintf_s ( pLineToReport, _MAX_PATH, "%s", pReportFile );
	WritePrivateProfileStringA ( "COMMON", "PathToEXE", pReportFile, pReportFile );
	if ( CEXE.m_dwRuntimeErrorDWORD==0 )
	{
		// crashed out inside a function - hard crash (ie access nullptr ptr)
		if ( g_pGlob )
			sprintf_s ( pLineToReport, _MAX_PATH, "Internal Code:%d", g_pGlob->dwInternalFunctionCode );
		else
			strcpy_s ( pLineToReport, _MAX_PATH, "Unknown Internal Location - email this file and TEMP\\FullSourceDump.dba to bugs@thegamecreators.com" );
	}
	else
	{
		// regular runtime error
		sprintf_s ( pLineToReport, _MAX_PATH, "%d", static_cast<DWORD>(CEXE.m_dwRuntimeErrorDWORD) );
	}
	WritePrivateProfileStringA ( "CEXE", "m_dwRuntimeErrorDWORD", pLineToReport, pReportFile );
	sprintf_s ( pLineToReport, _MAX_PATH, "%d", static_cast<DWORD>(CEXE.m_dwRuntimeErrorLineDWORD) );
	WritePrivateProfileStringA ( "CEXE", "m_dwRuntimeErrorLineDWORD", pLineToReport, pReportFile );			
	WritePrivateProfileStringA(
		"RUNTIME",
		"Message",
		runtimeMessage != nullptr ? runtimeMessage : "RunProgram returned false.",
		pReportFile);
	if (failure.exceptionCode != 0U)
	{
		sprintf_s(
			pLineToReport,
			_MAX_PATH,
			"0x%08lX",
			failure.exceptionCode);
		WritePrivateProfileStringA(
			"EXCEPTION", "Code", pLineToReport, pReportFile);
		sprintf_s(
			pLineToReport,
			_MAX_PATH,
			"0x%p",
			failure.exceptionAddress);
		WritePrivateProfileStringA(
			"EXCEPTION", "Instruction", pLineToReport, pReportFile);
		sprintf_s(
			pLineToReport,
			_MAX_PATH,
			"%llu",
			static_cast<unsigned long long>(failure.accessType));
		WritePrivateProfileStringA(
			"EXCEPTION", "AccessType", pLineToReport, pReportFile);
		sprintf_s(
			pLineToReport,
			_MAX_PATH,
			"0x%p",
			failure.accessedAddress);
		WritePrivateProfileStringA(
			"EXCEPTION", "AccessedAddress", pLineToReport, pReportFile);
		const auto memoryModule =
			MemoryPE::InspectAddress(failure.exceptionAddress);
		if (memoryModule)
		{
			WritePrivateProfileStringA(
				"EXCEPTION",
				"MemoryModule",
				memoryModule->moduleName.c_str(),
				pReportFile);
			WritePrivateProfileStringA(
				"EXCEPTION",
				"MemorySection",
				memoryModule->sectionName.c_str(),
				pReportFile);
			sprintf_s(
				pLineToReport,
				_MAX_PATH,
				"0x%lX",
				memoryModule->relativeVirtualAddress);
			WritePrivateProfileStringA(
				"EXCEPTION", "MemoryRVA", pLineToReport, pReportFile);
		}
		MEMORY_BASIC_INFORMATION memoryInfo{};
		if (failure.exceptionAddress != nullptr &&
			VirtualQuery(
				failure.exceptionAddress,
				&memoryInfo,
				sizeof(memoryInfo)) == sizeof(memoryInfo))
		{
			sprintf_s(
				pLineToReport,
				_MAX_PATH,
				"0x%p",
				memoryInfo.AllocationBase);
			WritePrivateProfileStringA(
				"EXCEPTION", "AllocationBase", pLineToReport, pReportFile);
			sprintf_s(
				pLineToReport,
				_MAX_PATH,
				"0x%lX",
				memoryInfo.Protect);
			WritePrivateProfileStringA(
				"EXCEPTION", "PageProtection", pLineToReport, pReportFile);
		}

		const auto instruction =
			reinterpret_cast<uintptr_t>(failure.exceptionAddress);
		const auto machineCode =
			reinterpret_cast<uintptr_t>(CEXE.m_pMachineCodeBlock);
		sprintf_s(
			pLineToReport,
			_MAX_PATH,
			"0x%llX",
			static_cast<unsigned long long>(machineCode));
		WritePrivateProfileStringA(
			"EXCEPTION", "MachineCodeBase", pLineToReport, pReportFile);
		sprintf_s(
			pLineToReport,
			_MAX_PATH,
			"0x%lX",
			CEXE.m_dwSizeOfMCB);
		WritePrivateProfileStringA(
			"EXCEPTION", "MachineCodeSize", pLineToReport, pReportFile);
		if (instruction >= machineCode &&
			instruction - machineCode < CEXE.m_dwSizeOfMCB)
		{
			sprintf_s(
				pLineToReport,
				_MAX_PATH,
				"0x%llX",
				static_cast<unsigned long long>(
					instruction - machineCode));
			WritePrivateProfileStringA(
				"EXCEPTION",
				"MachineCodeOffset",
				pLineToReport,
				pReportFile);
		}

		HMODULE module = nullptr;
		if (failure.exceptionAddress != nullptr &&
			GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
					GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(failure.exceptionAddress),
				&module))
		{
			char modulePath[_MAX_PATH]{};
			GetModuleFileNameA(module, modulePath, _MAX_PATH);
			WritePrivateProfileStringA(
				"EXCEPTION", "Module", modulePath, pReportFile);
			const auto offset =
				reinterpret_cast<uintptr_t>(failure.exceptionAddress) -
				reinterpret_cast<uintptr_t>(module);
			sprintf_s(
				pLineToReport,
				_MAX_PATH,
				"0x%llX",
				static_cast<unsigned long long>(offset));
			WritePrivateProfileStringA(
				"EXCEPTION", "ModuleOffset", pLineToReport, pReportFile);
		}
	}
}

bool FileExists(LPSTR pFilename)
{
	HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
		hFile=nullptr;
		return true;
	}
	return false;
}

// WINDOWS MAIN FUNCTION

int WINAPI WinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, LPSTR lpCmdLine, [[maybe_unused]] int nCmdShow)
{
	// Install diagnostic handlers before any packaged asset is touched
	db3::SetupDiagnosticHandlers();

	// Initialize Virtual File System hooks
	if(!VFSHooks::Initialize())
		return 1;

	// Memory Manager Initial Snapshot
	strcpy_s(g_MM_FunctionName, "WinMain");
	#ifdef  __USE_MEMORY_MANAGER__
	mm_SnapShot();
	#endif

	// Store reference to CL$()
	gRefCommandLineString=lpCmdLine;

	// Resolve the executable path and open a startup breadcrumb trace beside
	// it. Several startup paths used to fail with no observable output; the
	// trace records the last milestone reached and is flushed after every
	// write, so it survives crashes. Overwritten on each start.
	char ActualEXEFilename[_MAX_PATH];
	std::filesystem::path ActualExecutablePath;
	wchar_t wPath[_MAX_PATH];
	GetModuleFileNameW(hInstance, wPath, _MAX_PATH);
	ActualExecutablePath = std::filesystem::path(wPath);
	std::string utf8Path = TextConvert::UTF16ToUTF8(wPath);
	strcpy_s(ActualEXEFilename, _MAX_PATH, utf8Path.c_str());

	FILE* pStartupTraceFile = nullptr;
	fopen_s(&pStartupTraceFile, (ActualExecutablePath.parent_path() / "dbp_startup.log").string().c_str(), "w");
	const auto trace = [&pStartupTraceFile](const std::string& message) {
		if (pStartupTraceFile)
		{
			fputs(message.c_str(), pStartupTraceFile);
			fputc('\n', pStartupTraceFile);
			fflush(pStartupTraceFile);
		}
	};
	trace("WinMain entry");

	// Authenticate the package belonging to this executable.
	auto PackageStartup =
		RuntimePackageBootstrap::Start(ActualExecutablePath);
	if(!PackageStartup)
	{
		trace("package authentication FAILED: " + PackageStartup.error().message);
		const auto message = TextConvert::UTF8ToUTF16(
			"Dark Basic Professional could not authenticate its runtime package.\n\n" +
			PackageStartup.error().message);
		MessageBoxW(
			nullptr,
			message.c_str(),
			L"Runtime package error",
			MB_OK | MB_ICONERROR | MB_TOPMOST);
		VFSHooks::Shutdown();
		return 1;
	}
	trace("package authentication OK");
	auto RuntimePackage = std::move(PackageStartup.value());

	// Get current working directory (for temp folder compare)
	std::error_code ec;
	const auto currentPath = std::filesystem::current_path(ec);
	char CurrentDirectory[_MAX_PATH];
	strcpy_s(CurrentDirectory, currentPath.string().c_str());

	// Find temporary directory (C:\WINDOWS\Temp)
	char WindowsTempDirectory[_MAX_PATH];
	GetTempPathA(_MAX_PATH, WindowsTempDirectory);
	if(_stricmp(WindowsTempDirectory, CurrentDirectory)!=0)
	{
		std::filesystem::path unpackPath = std::filesystem::path(WindowsTempDirectory) / "dbpdata";
		strcpy_s(gUnpackDirectory, _MAX_PATH, unpackPath.string().c_str());
		std::filesystem::create_directories(unpackPath, ec);
		std::filesystem::current_path(unpackPath, ec);
		DeleteContentsOfDBPDATA(false);
		std::filesystem::current_path(currentPath, ec);
	}
	else
	{
		std::filesystem::path unpackPath = std::filesystem::path(CurrentDirectory) / "dbpdata";
		strcpy_s(gUnpackDirectory, _MAX_PATH, unpackPath.string().c_str());
		std::filesystem::create_directories(unpackPath, ec);
		std::filesystem::current_path(unpackPath, ec);
		DeleteContentsOfDBPDATA(false);
		std::filesystem::current_path(currentPath, ec);
	}

	// The executable block is mounted under its canonical package path.
	char LoadWithFilename[] = "_virtual.dat";
	int RuntimeExitCode = 0;

	// If EXE is an installer, create files then quit
	if(RuntimePackage->mode()==dbp::package::RuntimeMode::Installer)
	{
		trace("runtime mode: installer");
		const auto installed = RuntimePackage->MaterializeInstaller(
			std::filesystem::path(
				TextConvert::UTF8ToUTF16(CurrentDirectory)));
		if(!installed)
		{
			trace("installer materialization FAILED: " + installed.error().message);
			RuntimeExitCode = 1;
			const auto message = TextConvert::UTF8ToUTF16(
				"Dark Basic Professional could not publish the installed application.\n\n" +
				installed.error().message);
			MessageBoxW(
				nullptr,
				message.c_str(),
				L"Installer package error",
				MB_OK | MB_ICONERROR | MB_TOPMOST);
		}
		else
		{
			trace("installer materialization OK");
		}
	}
	else
	{
		trace("runtime mode: application");

		// Send critical start info to CEXE
		CEXE.StartInfo(gUnpackDirectory, 0);

		// Place absolute EXE filename in CEXE structure
		CEXE.m_AbsoluteAppFile = ActualEXEFilename;

		// In case of error
		LPSTR pErrorString = nullptr;

		// Load EXE Block
		trace("loading EXE block");
		if(CEXE.Load(LoadWithFilename))
		{
			trace("EXE block loaded");
			// creating window HERE will allow me to create the right size, type, etc
			trace("creating temp window");
			CreateTempWindow ( hInstance, ActualEXEFilename, CEXE.m_dwInitialDisplayWidth, CEXE.m_dwInitialDisplayHeight );
			trace("temp window created");

			// Execute behind an SEH boundary so a generated-code or plug-in
			// fault is reported with a useful module offset and a failing exit.
			RuntimeExecutionFailure executionFailure;
			bool runResult = false;
			trace("running program");
			if (!TryRunProgram(
					hInstance,
					&pErrorString,
					&runResult,
					&executionFailure))
			{
				trace("program execution faulted");
				trace(pErrorString
					? std::string("run error: ") + pErrorString
					: std::string("run error: (no error string)"));
				DumpDebugReport(executionFailure, pErrorString);
				RuntimeExitCode = 1;
			}
			else if (!runResult)
			{
				trace("program run returned failure");
				trace(pErrorString
					? std::string("run error: ") + pErrorString
					: std::string("run error: (no error string)"));
				DumpDebugReport(executionFailure, pErrorString);
				RuntimeExitCode = 1;
			}
			else
			{
				trace("program finished");
			}

			// Free Display First
			CEXE.FreeUptoDisplay();

			// Report any errors
			if(pErrorString)
			{
				// Report Failure to Run
				ShowCursor(TRUE);
				SetCursor(LoadCursor(nullptr, IDC_ARROW));
				// surface any trailing packaged error detail after the unpack path
				std::string fullError(pErrorString);
				if ( g_pGlob && g_pGlob->pEXEUnpackDirectory )
				{
					const size_t dwEXEPathLength = strlen( g_pGlob->pEXEUnpackDirectory );
					LPSTR pSecretErrorMessage = g_pGlob->pEXEUnpackDirectory + dwEXEPathLength + 1;
					if ( pSecretErrorMessage && pSecretErrorMessage[0] != 0 )
						fullError += std::string(".\n") + pSecretErrorMessage;
				}
				// Persist the failure instead of blocking on a modal dialog: the
				// conformance/headless runtime cannot dismiss a message box, and a
				// dialog would hang every automated run. Write dbp_error.txt in the
				// working directory (next to output.txt) for the harness to read.
				const std::string errorFilePath = "dbp_error.txt";
				FILE* pErrorFile = nullptr;
				if ( fopen_s ( &pErrorFile, errorFilePath.c_str(), "w" ) == 0 && pErrorFile )
				{
					fputs(fullError.c_str(), pErrorFile);
					fputc('\n', pErrorFile);
					fclose(pErrorFile);
				}
				else
				{
					// Only fall back to a dialog when the error could not be persisted.
					const std::wstring message = TextConvert::UTF8ToUTF16(
						"An issue was detected and the session needs to restart: " + fullError);
					MessageBoxW(nullptr, message.c_str(), L"Runtime Problem Detected", MB_TOPMOST | MB_OK);
				}
				SAFE_DELETE_ARRAY(pErrorString);
			}

			// Free EXE Block
			CEXE.Free();
		}
		else
		{
			// Persist the load failure instead of exiting silently: the
			// conformance/headless runtime cannot surface a dialog.
			std::string loadError = CEXE.GetLoadError();
			if (loadError.empty())
				loadError = "The packaged EXE block could not be loaded.";
			trace("EXE block load FAILED: " + loadError);
			FILE* pErrorFile = nullptr;
			if (fopen_s(&pErrorFile, "dbp_error.txt", "w") == 0 && pErrorFile)
			{
				fputs(loadError.c_str(), pErrorFile);
				fputc('\n', pErrorFile);
				fclose(pErrorFile);
			}
			RuntimeExitCode = 1;
		}

	}

	// Unmount before shutting down the VFS hook layer.
	RuntimePackage.reset();

	// Shutdown Virtual File System hooks and free resources before filesystem cleanup
	VFSHooks::Shutdown();

	// Clean up old DBP data folders in temp directory
	if (std::filesystem::exists(WindowsTempDirectory, ec))
	{
		std::filesystem::current_path(WindowsTempDirectory, ec);
		DeleteAllOldDBPDATAFolders();
		std::filesystem::current_path(currentPath, ec);
	}

	// Delete temporary unpack directory
	std::filesystem::remove_all(gUnpackDirectory, ec);

	trace("exit code " + std::to_string(RuntimeExitCode));
	return RuntimeExitCode;
}
