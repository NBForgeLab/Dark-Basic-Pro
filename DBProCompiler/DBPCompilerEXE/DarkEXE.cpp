//
// DarkEXE
//

// Internal Includes
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable : 4996)
#include "RuntimePackageBootstrap.h"
#include "windows.h"
#include "direct.h"
#include "../DBPCompiler/TextConvert.h"
#include "../DBPCompiler/VFSHooks.h"
#include "../DBPCompiler/CrashHandler.h"
#include "time.h"

#include <filesystem>
#include <memory>
#include <string>

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

// Internal File Handling
#include "io.h"
struct _finddata_t		filedata;
long					hInternalFile					= NULL;
int						FileReturnValue					= -1;
time_t					TodaysDay						= 0;

// Interal DarkEXE Data Variables
LPSTR								gRefCommandLineString=NULL;
char								gUnpackDirectory[_MAX_PATH];
char								gpDBPDataName[_MAX_PATH];

// EXECUTABLE Class
CEXEBlock							CEXE;

// DebugDump needs this to get at useful debug data (U5.8)
extern GlobStruct*					g_pGlob;

// Temporary Window while loading DLLs
HWND								g_hTempWindow = NULL;

// IGLOADER can send WM_SETTEXT to the temp window, DBP executables attempt to 'EMBED'
HWND								g_igLoader_HWND = NULL;

// DEMO SPLASH POPUP - ONLY ADDED FOR DEMO-BUILD

#ifdef DEMOSPLASH

int g_iTrialDialogID=0;
bool g_bTrialPeriodActive=false;
#define SPLASHPOPW 379
#define SPLASHPOPH 495

BOOL CALLBACK DialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			{
				int cx = (GetSystemMetrics(SM_CXFULLSCREEN)-SPLASHPOPW)/2;
				int cy = (GetSystemMetrics(SM_CYFULLSCREEN)-SPLASHPOPH)/2;
				SetWindowPos(hwndDlg, HWND_TOPMOST, cx, cy, 0, 0, SWP_NOSIZE);
				SetTimer(hwndDlg, 0, 2000, NULL);

				// Modify Text if Trial Period Expired
				char pReport[256]; 
				strcpy(pReport, "TRIAL VERSION EXECUTABLE - FREEWARE");
				SetDlgItemText( hwndDlg, IDC_TRIALTEXT, pReport);
			}
			return 0;

		case WM_TIMER:
			DestroyWindow(hwndDlg);
			return 1;
	}
	return DefWindowProc(hwndDlg, uMsg, wParam, lParam);
}
void PopupDemoSplashWindow(HINSTANCE hInstance)
{
	g_iTrialDialogID = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, &DialogProc);
}

BOOL CALLBACK DialogEndLinkProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			{
				int cx = (GetSystemMetrics(SM_CXFULLSCREEN)-SPLASHPOPW)/2;
				int cy = (GetSystemMetrics(SM_CYFULLSCREEN)-SPLASHPOPH)/2;
				SetWindowPos(hwndDlg, HWND_TOPMOST, cx, cy, 0, 0, SWP_NOSIZE);
			}
			return 0;

		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{ 
				case IDC_LINK:	// Link to DBP Website
								DestroyWindow(hwndDlg);
								ShellExecute(	NULL,
												"open",
												"http://www.darkbasicpro.com",
												"",
												"",
												SW_SHOWDEFAULT);
								PostQuitMessage(0);
								break;

				default:
					DestroyWindow(hwndDlg);
					break;
			} 
			return 1;

		case WM_CLOSE:
			DestroyWindow(hwndDlg);
			return 1;
	}
	return DefWindowProc(hwndDlg, uMsg, wParam, lParam);
}

void ReadStringFromRegistryEXE(char* PerfmonNamesKey, char* valuekey, char* string)
{
	HKEY hKeyNames = 0;
	DWORD Status;
	char ObjectType[256];
	DWORD Datavalue = 0;

	strcpy(string,"");
	strcpy(ObjectType,"Num");
	Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, PerfmonNamesKey, 0L, KEY_READ, &hKeyNames);
    if(Status==ERROR_SUCCESS)
	{
		DWORD Type=REG_SZ;
		DWORD Size=256;
		Status = RegQueryValueEx(hKeyNames, valuekey, NULL, &Type, NULL, &Size);
		if(Size<255)
			RegQueryValueEx(hKeyNames, valuekey, NULL, &Type, (LPBYTE)string, &Size);

		RegCloseKey(hKeyNames);
	}
}

/*
bool FileExists(LPSTR pFilename)
{
	HANDLE hFile = CreateFile(pFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		// Close File
		SAFE_CLOSE(hFile);
		return true;
	}
	// soft fail
	return false;
}
*/

void PopupDemoEndLink(HINSTANCE hInstance)
{
	// Only popup if not got it installed..
	char InstallPath[256];
	char keyname[256];
	strcpy(keyname, "Software\\Dark Basic\\Dark Basic Pro");
	ReadStringFromRegistryEXE(keyname,"INSTALL-PATH", InstallPath);
// leefix-220703-simpler so it does not popup on new/old/diff version
//	char TestDLL[256];
//	strcat(InstallPath, "\\Compiler\\plugins\\");
//	strcpy(TestDLL,InstallPath);
//	strcat(TestDLL,"DBProCore.dll");
//	if(FileExists(TestDLL))
	if(strcmp(InstallPath,"")!=NULL)
	{
		// Carry on - compiler installed already
	}
	else
	{
		// Compiler not installed - advertise it
		g_iTrialDialogID = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG2), NULL, &DialogEndLinkProc);
	}
}

#endif

// LOAD EXE DATA AND RUN

bool RunProgram(HINSTANCE hInstance, LPSTR* pReturnError)
{
	// Result Var
	bool bResult=true;
	bool bSuccessfulDLLLinks=false;

	// Make program
	bResult=CEXE.Init(hInstance, bResult, pReturnError, gRefCommandLineString);

	// LEEADD - 221008 - U71 - SIMULATE EXTERNAL DEBUGGER (not for release code)
	//#define SIMULATEEXTERNALDEBUGGER
	#ifdef SIMULATEEXTERNALDEBUGGER
		// 1. external debugger creates mutex
		char pUniqueMutexNameForExternalDebugger [ 512 ];
		strcpy ( pUniqueMutexNameForExternalDebugger, CEXE.m_pAbsoluteAppFile );
		strcat ( pUniqueMutexNameForExternalDebugger, "(Mutex)" );
		HANDLE pExternalDebuggerCreatesMutex = CreateMutex ( NULL, FALSE, pUniqueMutexNameForExternalDebugger );
		// 2. external debugger writes DEBUGME to the shared string file map
		LPSTR pDebugMeString = "debugme";
		char pUniqueFileMapName [ 512 ];
		strcpy ( pUniqueFileMapName, CEXE.m_AbsoluteAppFile.c_str() );
		strcat ( pUniqueFileMapName, "(FileMap)" );
		DWORD dwWriteDataSize = strlen(pDebugMeString)+1;
		HANDLE hWriteFileMap = CreateFileMappingW((HANDLE)0xFFFFFFFF,NULL,PAGE_READWRITE,0,dwWriteDataSize+4,TextConvert::UTF8ToUTF16(pUniqueFileMapName).c_str());
		if(hWriteFileMap)
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
	char pUniqueMutexName [ 512 ];
	strcpy ( pUniqueMutexName, CEXE.m_AbsoluteAppFile.c_str() );
	strcat ( pUniqueMutexName, "(Mutex)" );
	for ( DWORD n=0; n<strlen(pUniqueMutexName); n++ )
	{
		if ( pUniqueMutexName[n]==':' ) pUniqueMutexName[n] = '_';
		if ( pUniqueMutexName[n]=='\\' ) pUniqueMutexName[n] = '_';
		if ( pUniqueMutexName[n]=='/' ) pUniqueMutexName[n] = '_';
	}
	HANDLE pAppMutex = OpenMutexW ( MUTEX_ALL_ACCESS, FALSE, TextConvert::UTF8ToUTF16(pUniqueMutexName).c_str() );
	if ( pAppMutex )
	{
		// it appears another process has already created an identical mutex
		// (which can happen if multiple programs with the same name are running)
		// so we then check a shared storage location to see whether debugging
		// has been requested by the 'external process' that may have created a mutex
		char pSharedStringStorage [ 512 ];
		strcpy ( pSharedStringStorage, "" );
		char pUniqueFileMapName [ 512 ];
		strcpy ( pUniqueFileMapName, CEXE.m_AbsoluteAppFile.c_str() );
		strcat ( pUniqueFileMapName, "(FileMap)" );
		for ( DWORD n=0; n<strlen(pUniqueFileMapName); n++ )
		{
			if ( pUniqueFileMapName[n]==':' ) pUniqueFileMapName[n] = '_';
			if ( pUniqueFileMapName[n]=='\\' ) pUniqueFileMapName[n] = '_';
			if ( pUniqueFileMapName[n]=='/' ) pUniqueFileMapName[n] = '_';
		}

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
					LPSTR pLoadData = (LPSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, dwDataSize+1);
					memcpy(pLoadData, (LPSTR)lpVoid+4, dwDataSize);
					strcpy ( pSharedStringStorage, pLoadData ); // takes the shared string ehre!
					GlobalFree ( pLoadData );
				}
				UnmapViewOfFile(lpVoid);
			}
			CloseHandle(hFileMap);
		}
		if ( stricmp ( pSharedStringStorage, "debugme" )==NULL )
		{
			// a mutex for this app exists and the shared storage says it wants to
			// DEBUG this application, so we pause here until we can own the mutex
			// (which is achieved by the current owner of the mutex releasing it)

			// but before we pause, we must replace the string in the shared storage
			// so some new data which tells the debugger we are here now and waiting
			// for ownership of the mutex so we can start running the program. In 
			// order to keep this section simple, we simply pass in the memory address
			// of the GLOBSTRUCT data, which includes all the information needed
			DWORD dwWriteDataSize = 4;
			HANDLE hWriteFileMap = CreateFileMappingW((HANDLE)0xFFFFFFFF,NULL,PAGE_READWRITE,0,dwWriteDataSize,TextConvert::UTF8ToUTF16(pUniqueFileMapName).c_str());
			if(hWriteFileMap)
			{
				LPVOID lpWriteVoid = MapViewOfFile(hWriteFileMap,FILE_MAP_WRITE,0,0,dwWriteDataSize);
				if(lpWriteVoid)
				{
					// Copy to Virtual File
					*(DWORD*)lpWriteVoid = (DWORD)g_pGlob;
					UnmapViewOfFile(lpWriteVoid);
				}
				CloseHandle(hWriteFileMap);
			}

			// friendly message
			MessageBoxW ( NULL, TextConvert::UTF8ToUTF16(pUniqueMutexName).c_str(), L"DBP App has deposited the glob struct in the filemap as a DWORD, and now wants to OWN this mutex...give it to me!", MB_OK );

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
	else
	{
		// could not find a MUTEX open, so no other apps with same mutex name
		// are running, which includes any external debuggers so we continue as normal
	}

	// Run the EXE Program
	bResult=CEXE.Run(bResult);

	// Report Any Runtime Errors
	DWORD dwRTError=CEXE.m_dwRuntimeErrorDWORD;
	DWORD dwRTErrorLine=CEXE.m_dwRuntimeErrorLineDWORD;
	if(dwRTError>0)
	{
		// create report string and store
		*pReturnError = new char[1024];
		LPSTR pRuntimeErrorString = NULL;
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
			if ( g_igLoader_HWND==NULL )
			{
				LPSTR pIncomingStr = (LPSTR)lParam;
				if ( pIncomingStr )
				{
					if ( strlen ( pIncomingStr ) > 7 )
					{
						// extract HWND from lParam
						char str [ 256 ];
						strcpy ( str, pIncomingStr + 7 );
						int num = atoi ( str );
						g_igLoader_HWND = (HWND)num;

						// when get igl parent handle, change window to no-size, no-border, nout
						DWORD dwWindowStyle = 0;
						SetWindowLong ( hWnd, GWL_STYLE, (LONG)dwWindowStyle );
						SetWindowPos ( hWnd, NULL, 0, 0, 640, 480, 0 );
						UpdateWindow ( hWnd );
					}
				}
			}
			return 0;
		}

		case WM_CLOSE:
			PostQuitMessage(0);
			break;
	}

	// Default Action
    return DefWindowProc(hWnd, message, wParam, lParam);
}


void CreateTempWindow ( HINSTANCE hInstance, LPSTR pFullAppPath, DWORD dwWindowWidth, DWORD dwWindowHeight )
{
	// Initial Window (ahead of lengthy DLL and media load)
	char pAppName[_MAX_PATH];
	strcpy(pAppName, pFullAppPath);
	strrev(pAppName);
	for(DWORD d=0; d<strlen(pAppName); d++)
	{
		if(pAppName[d]=='\\' || pAppName[d]=='/')
			pAppName[d]=0;
	}
	strrev(pAppName);
	if(strlen(pAppName)>4)
		if(pAppName[strlen(pAppName)-4]=='.')
			pAppName[strlen(pAppName)-4]=0;

	// Register window
	WNDCLASSA wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = TempWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = pAppName;
	RegisterClassA( &wc );

	// Create Window
	g_hTempWindow = CreateWindowExA( 
										0,                      // no extended styles           
										pAppName,				// class name                   
										pAppName,				// window name                  
										0,					    // no WS_OVERLAPPEDWINDOW for pure window for igloader browser
										-50000,					// temp window hide completely! 
										-50000,					//  
										dwWindowWidth,			// default width                
										dwWindowHeight,			// default height               
										(HWND) NULL,            // no parent or owner window    
										(HMENU) NULL,           // class menu used              
										hInstance,              // instance handle              
										NULL);                  // no window creation data      
	
	// Return if not valid
	if (!g_hTempWindow) 
		return; 

	// Show the created window, and issue a WM_PAINT message
	ShowWindow(g_hTempWindow, SW_HIDE ); // 080909 - for hidden applications, this flashes taskbar icon quickly, was SW_NORMAL); 
	UpdateWindow(g_hTempWindow); 
}


void FFindCloseFile(void)
{
	_findclose(hInternalFile);
	hInternalFile=NULL;
}

void FFindFirstFile(void)
{
	if(hInternalFile) FFindCloseFile();
	hInternalFile = _findfirst("*.*", &filedata);
	if(hInternalFile!=-1L)
	{
		// Success!
		FileReturnValue=0;
	}
}

int FGetFileReturnValue(void)
{
	return FileReturnValue;
}

void FFindNextFile(void)
{
	FileReturnValue = _findnext(hInternalFile, &filedata);
}

int FGetActualTypeValue(int flagvalue)
{
	if(flagvalue & _A_SUBDIR)
		return 1;
	else
		return 0;
}

void DeleteContentsOfDBPDATA ( bool bOnlyIfOlderThan2DAYS )
{
	// Delete all files in current folder
	FFindFirstFile();
	int iAttempts=20;
	while(FGetFileReturnValue()!=-1L && iAttempts>0)
	{
		// only if older than 2 days
		bool bGo = false;
		if ( bOnlyIfOlderThan2DAYS==false ) bGo = true;
		if ( bOnlyIfOlderThan2DAYS==true )
		{
			time_t ThisFileDays = filedata.time_write / 60 / 60 / 24;
			time_t Difference = TodaysDay - ThisFileDays; 
			if ( Difference >= 2 )
			{
				// this file is at least 2 days old
				bGo = true;
			}
		}

		// go
		if ( bGo )
		{
			if(FGetActualTypeValue(filedata.attrib)==1)
			{
				if(stricmp(filedata.name, ".")!=NULL
				&& stricmp(filedata.name, "..")!=NULL)
				{
					char file[_MAX_PATH];
					strcpy(file, filedata.name);
					char old[_MAX_PATH];
					getcwd(old, _MAX_PATH);
					BOOL bResult = RemoveDirectoryW(TextConvert::UTF8ToUTF16(file).c_str());
					if(bResult==FALSE)
					{
						chdir(file);
						FFindCloseFile();
						DeleteContentsOfDBPDATA ( bOnlyIfOlderThan2DAYS );
						chdir(old);
						BOOL bResult = RemoveDirectoryW(TextConvert::UTF8ToUTF16(file).c_str());
						FFindFirstFile();
					}
					iAttempts--;
				}
			}
			else
			{
				DeleteFileW(TextConvert::UTF8ToUTF16(filedata.name).c_str());
			}
		}
		FFindNextFile();
	}
	FFindCloseFile();
}

void MakeOrEnterUniqueDBPDATA(void)
{
	// Default DBPDATA string
	strcpy(gpDBPDataName,"dbpdata");

	// Prepare DBPDATA string builder
	DWORD dwBuildID=2;
	char pBuildStart[_MAX_PATH];
	strcpy(pBuildStart, gpDBPDataName);

	//05032015 - 015 - To help the virus checkers that are flagging the sound dll we will keep the folder the same every time
	// Go for a million attempts
	/*while(dwBuildID<1000000)
	{
		// Check if DBPDATA folder exists
		if(chdir(gpDBPDataName)==-1)
		{*/
			// Make it as unique
			mkdir(gpDBPDataName);

			// Leave now
			/*break;
		}
		else
		{
			// Does exist, so back out of it..
			chdir("..");

			// Create new folder name from build data
			wsprintf(gpDBPDataName, "%s%d", pBuildStart, dwBuildID);
		}

		// Will change foldername slightly
		dwBuildID++;
	}*/

	// get todays day when made directory
	FFindFirstFile();
	TodaysDay = filedata.time_write / 60 / 60 / 24;
	FFindCloseFile();
}

void DeleteAllOldDBPDATAFolders(void)
{
	// Default DBPDATA string
	strcpy(gpDBPDataName,"dbpdata");

	// Prepare DBPDATA string builder
	DWORD dwBuildID=2;
	char pBuildStart[_MAX_PATH];
	strcpy(pBuildStart, gpDBPDataName);

	// Go for a million attempts
	while(dwBuildID<1000000)
	{
		// Check if DBPDATA folder exists
		if(chdir(gpDBPDataName)!=-1 || dwBuildID==2)
		{
			// Delete contents of directory
			DeleteContentsOfDBPDATA(true);

			// Delete directory (if empty)
			chdir("..");
			rmdir(gpDBPDataName);

			// Create new folder name from build data
			sprintf_s(gpDBPDataName, _MAX_PATH, "%s%d", pBuildStart, dwBuildID);
		}
		else
		{
			// Done scanning
			break;
		}

		// Will change foldername slightly
		dwBuildID++;
	}
}

// DUMP DEBUG REPORT

void DumpDebugReport ( void )
{
	// Setup Report (by date and time)
	char pReportDate [ _MAX_PATH ];
	_strdate ( pReportDate );
	for ( DWORD i=0; i<strlen(pReportDate); i++ )
		if ( pReportDate[i]=='/' )
			pReportDate[i]='_';

	// Current location
	char currentdir [ _MAX_PATH ];
	_getcwd ( currentdir, _MAX_PATH );

	// Construct local directory
	char pReportFile [ _MAX_PATH ];
	strcpy ( pReportFile, currentdir );
	strcat ( pReportFile, "\\CrashOn_" );
	strcat ( pReportFile, pReportDate );
	strcat ( pReportFile, ".txt" );

	// Create Report File (by date and time)
	char pLineToReport [ _MAX_PATH ];
	sprintf_s ( pLineToReport, _MAX_PATH, "%s", pReportFile );
	WritePrivateProfileStringA ( "COMMON", "PathToEXE", pReportFile, pReportFile );
	if ( CEXE.m_dwRuntimeErrorDWORD==0 )
	{
		// crashed out inside a function - hard crash (ie access NULL ptr)
		if ( g_pGlob )
			sprintf_s ( pLineToReport, _MAX_PATH, "Internal Code:%d", g_pGlob->dwInternalFunctionCode );
		else
			strcpy_s ( pLineToReport, _MAX_PATH, "Unknown Internal Location - email this file and TEMP\\FullSourceDump.dba to bugs@thegamecreators.com" );
	}
	else
	{
		// regular runtime error
		sprintf_s ( pLineToReport, _MAX_PATH, "%d", CEXE.m_dwRuntimeErrorDWORD );
	}
	WritePrivateProfileStringA ( "CEXE", "m_dwRuntimeErrorDWORD", pLineToReport, pReportFile );
	sprintf_s ( pLineToReport, _MAX_PATH, "%d", CEXE.m_dwRuntimeErrorLineDWORD );
	WritePrivateProfileStringA ( "CEXE", "m_dwRuntimeErrorLineDWORD", pLineToReport, pReportFile );			
}

bool FileExists(LPSTR pFilename)
{
	HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
		hFile=NULL;
		return true;
	}
	return false;
}

// WINDOWS MAIN FUNCTION

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	db3::SetupDiagnosticHandlers();

	// Initialize Virtual File System hooks
	if(!VFSHooks::Initialize())
		return 1;

	// Memory Manager Initial Snapshot
	strcpy ( g_MM_FunctionName, "WinMain" );
	#ifdef  __USE_MEMORY_MANAGER__
	mm_SnapShot();
	#endif

	// Very first thing is a possible demo splash dialogue
	#ifdef DEMOSPLASH
	PopupDemoSplashWindow(hInstance);
	#endif

	// Store reference to CL$()
	gRefCommandLineString=lpCmdLine;

	// Resolve and authenticate the package belonging to this executable.
	char ActualEXEFilename[_MAX_PATH];
	std::filesystem::path ActualExecutablePath;
	{
		wchar_t wPath[_MAX_PATH];
		GetModuleFileNameW(hInstance, wPath, _MAX_PATH);
		ActualExecutablePath = std::filesystem::path(wPath);
		std::string utf8Path = TextConvert::UTF16ToUTF8(wPath);
		strncpy(ActualEXEFilename, utf8Path.c_str(), _MAX_PATH - 1);
		ActualEXEFilename[_MAX_PATH - 1] = '\0';
	}
	auto PackageStartup =
		RuntimePackageBootstrap::Start(ActualExecutablePath);
	if(!PackageStartup)
	{
		const auto message = TextConvert::UTF8ToUTF16(
			"Dark Basic Professional could not authenticate its runtime package.\n\n" +
			PackageStartup.error().message);
		MessageBoxW(
			NULL,
			message.c_str(),
			L"Runtime package error",
			MB_OK | MB_ICONERROR | MB_TOPMOST);
		VFSHooks::Shutdown();
		return 1;
	}
	auto RuntimePackage = std::move(PackageStartup.value());

	// Get current working directory (for temp folder compare)
	char CurrentDirectory[_MAX_PATH];
	getcwd(CurrentDirectory, _MAX_PATH);

//	// test redirection when testing DarkEXE Project
//	strcpy(ActualEXEFilename, "Application.exe");
//	strcpy(ActualEXEFilename, "test.exe");
//	strcpy(ActualEXEFilename, "FPSC-Game.exe");
//	strcpy(ActualEXEFilename, "bigrocks.exe");
//	strcpy(ActualEXEFilename, "TerrainProto.exe");
//	strcpy(ActualEXEFilename, "FPSC-Proto-Game.exe");
//	strcpy(ActualEXEFilename, "two-compounds.exe");
//	strcpy(ActualEXEFilename, "Bullet Physics Demo.exe");
//	strcpy(ActualEXEFilename, "FPSC-Lightmapper.exe");
//	strcpy(ActualEXEFilename, "FPSC-MapEditor_.exe");

	// lee - 050406 - u6rc6 - new define for an EXE which uses local exe location for extraction TEMP
	char WindowsTempDirectory[_MAX_PATH];
	#ifdef LOCALLYEXTRACTINGEXE
	// The TEMP folder is the EXE folder
	strcpy ( WindowsTempDirectory, CurrentDirectory );
	MakeOrEnterUniqueDBPDATA();
	strcat(WindowsTempDirectory, "\\");
	strcat(WindowsTempDirectory, gpDBPDataName);
	strcpy(gUnpackDirectory, WindowsTempDirectory);
	chdir(gpDBPDataName);
	DeleteContentsOfDBPDATA(false);
	chdir(CurrentDirectory);
	#else
	// Find temporary directory (C:\WINDOWS\Temp)
	GetTempPathA(_MAX_PATH, WindowsTempDirectory);
	if(stricmp(WindowsTempDirectory, CurrentDirectory)!=NULL)
	{
		// XP Temp Folder
		chdir(WindowsTempDirectory);
		MakeOrEnterUniqueDBPDATA();
		strcat(WindowsTempDirectory, "\\");
		strcat(WindowsTempDirectory, gpDBPDataName);
		strcpy(gUnpackDirectory, WindowsTempDirectory);
		chdir(gpDBPDataName);
		DeleteContentsOfDBPDATA(false);
		chdir(CurrentDirectory);
	}
	else
	{
		// Pre-XP Temp Folder
		GetWindowsDirectoryA(WindowsTempDirectory, _MAX_PATH);
		_chdir(WindowsTempDirectory);
		mkdir("temp");
		chdir("temp");
		MakeOrEnterUniqueDBPDATA();
		strcat(WindowsTempDirectory, "\\temp\\");
		strcat(WindowsTempDirectory, gpDBPDataName);
		strcpy(gUnpackDirectory, WindowsTempDirectory);
		chdir(gpDBPDataName);
		DeleteContentsOfDBPDATA(false);
		chdir(CurrentDirectory);
	}
	#endif

	// The executable block is mounted under its canonical package path.
	char LoadWithFilename[_MAX_PATH];
	strcpy(LoadWithFilename, "_virtual.dat");
	int RuntimeExitCode = 0;

	// If EXE is an installer, create files then quit
	if(RuntimePackage->mode()==dbp::package::RuntimeMode::Installer)
	{
		const auto installed = RuntimePackage->MaterializeInstaller(
			std::filesystem::path(
				TextConvert::UTF8ToUTF16(CurrentDirectory)));
		if(!installed)
		{
			RuntimeExitCode = 1;
			const auto message = TextConvert::UTF8ToUTF16(
				"Dark Basic Professional could not publish the installed application.\n\n" +
				installed.error().message);
			MessageBoxW(
				NULL,
				message.c_str(),
				L"Installer package error",
				MB_OK | MB_ICONERROR | MB_TOPMOST);
		}
	}
	else
	{
		// Send critical start info to CEXE
		CEXE.StartInfo(gUnpackDirectory, 0);

		// Place absolute EXE filename in CEXE structure
		CEXE.m_AbsoluteAppFile = ActualEXEFilename;

		// In case of error
		LPSTR pErrorString = NULL;

		// Load EXE Block
		if(CEXE.Load(LoadWithFilename))
		{
			// creating window HERE will allow me to create the right size, type, etc
			CreateTempWindow ( hInstance, ActualEXEFilename, CEXE.m_dwInitialDisplayWidth, CEXE.m_dwInitialDisplayHeight );

			// try to run the ASM program
			try
			{
				// Run EXE Block
				RunProgram(hInstance, &pErrorString);
			}
			catch(...)
			{
				// If crash bomb, dump a text report for crash
				DumpDebugReport ( );
			}

			// Free Display First
			CEXE.FreeUptoDisplay();

			// Report any errors
			if(pErrorString)
			{
				// Report Failure to Run
				ShowCursor(TRUE);
				SetCursor(LoadCursor(NULL, IDC_ARROW));
				char pFullError[_MAX_PATH];
				strcpy ( pFullError, pErrorString );

				// is there any additional error info after EXEPATH string?
				DWORD dwEXEPathLength = strlen ( g_pGlob->pEXEUnpackDirectory );
				LPSTR pSecretErrorMessage = g_pGlob->pEXEUnpackDirectory + dwEXEPathLength + 1;
				if ( strlen ( pSecretErrorMessage ) > 0 && strlen ( pSecretErrorMessage ) < _MAX_PATH )
				{
					// addition error info
					strcat ( pFullError, ".\n" );
					strcat ( pFullError, pSecretErrorMessage );
				}
				//MessageBox(NULL, pFullError, "Error", MB_TOPMOST | MB_OK);
				char err[512];
				sprintf_s ( err, 512, "GameGuru has detected an unexpected issue and needs to restart your session. If this problem persists, please contact support with error code (%s)", pErrorString );
				MessageBoxW(NULL, TextConvert::UTF8ToUTF16(err).c_str(), L"GameGuru Problem Detected", MB_TOPMOST | MB_OK);
				SAFE_DELETE(pErrorString);
			}

			// Demo Prompts EndUser to download DBPro Demo :)
			#ifdef DEMOSPLASH
			PopupDemoEndLink(hInstance);
			#endif

			// Free EXE Block
			CEXE.Free();
		}
		else
		{
			RuntimeExitCode = 1;
		}

	}

	// Unmount before shutting down the VFS hook layer.
	RuntimePackage.reset();

	// leeadd - 230604 - u54 - use current date to remove
	chdir(WindowsTempDirectory);
	if ( chdir("..")!=-1 ) DeleteAllOldDBPDATAFolders();
	chdir(CurrentDirectory);

	// Delete temporary unpack directory
	rmdir(gUnpackDirectory);

	// Shutdown Virtual File System hooks and free resources
	VFSHooks::Shutdown();

	return RuntimeExitCode;
}
