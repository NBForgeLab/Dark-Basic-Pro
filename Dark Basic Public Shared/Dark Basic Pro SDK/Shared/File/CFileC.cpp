
//////////////////////////////////////////////////////////////////////////////////
// INCLUDES / LIBS ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#include <windows.h> 
#include <windowsx.h>
#include "mmsystem.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include "cfilec.h"
#include <direct.h>
#include <io.h>
#include ".\..\error\cerror.h"
#include ".\..\core\globstruct.h"
#include "winioctl.h"
#include "shlobj.h"
#include ".\..\Core\EncryptedFile.h"

// 20091129 v75 - IRM - Provide automatically expanding buffer when reading strings
#include <vector>
#include <filesystem>
#include <string_view>
#include <string>

#ifdef DARKSDK_COMPILE
	#include ".\..\..\..\DarkGDK\Code\Include\DarkSDKMemblocks.h"
#endif

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////
// GLOBALS ///////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

typedef LPSTR			( *RetVoidMakeMemblock )		( int, DWORD );
typedef void			( *RetVoidFreeMemblock )		( int );
typedef DWORD			( *RetVoidGetMemblockSize )		( int );
typedef void			( *RetVoidSetMemblockSize )		( int, DWORD);
typedef LPSTR			( *RetVoidGetMemblockPtr )		( int );

struct sHardDrive
{
	LARGE_INTEGER	liCylinderCount;
	DWORD			dwTracksPerCylinder; 
	DWORD			dwSectorsPerTrack; 
	DWORD			dwBytesPerSector; 

	ULONGLONG		ulTotalBytes;
	ULONGLONG		ulTotalMB;
	ULONGLONG		ulTotalGB;
};

#define MAX_HARD_DRIVE 24

DBPRO_GLOBAL sHardDrive					g_HardDrives      [ MAX_HARD_DRIVE ];
DBPRO_GLOBAL char						g_HardDiskLetters [ MAX_HARD_DRIVE ] [ 4 ] = 
																					{
																						"c:\\",	"d:\\",	"e:\\",	"f:\\",	"g:\\",	"h:\\",
																						"i:\\",	"j:\\",	"k:\\",	"l:\\",	"m:\\",	"n:\\",
																						"o:\\",	"p:\\",	"q:\\",	"r:\\",	"s:\\",	"t:\\",
																						"u:\\",	"v:\\",	"w:\\",	"x:\\",	"y:\\",	"z:\\"
																					};
DBPRO_GLOBAL int						g_iHardDriveCount = 0;
DBPRO_GLOBAL HANDLE						ghExecuteFileProcess			= nullptr;
#define					MAX_FILES	64
DBPRO_GLOBAL HANDLE						File[MAX_FILES] = {};
DBPRO_GLOBAL BOOL						FileEOF[MAX_FILES] = {};
DBPRO_GLOBAL char*						pVirtFileEncrypted[MAX_FILES] = {};
DBPRO_GLOBAL char						filetext[_MAX_PATH] = {};
DBPRO_GLOBAL struct _finddata_t			filedata = {};
DBPRO_GLOBAL intptr_t					hInternalFile					= -1;
DBPRO_GLOBAL int						FileReturnValue					= -1;
DBPRO_GLOBAL bool						g_bCreateChecklistNow           = false;
DBPRO_GLOBAL DWORD						g_dwMaxStringSizeInEnum         = 0;
DBPRO_GLOBAL char						m_pWorkString[1024]             = {};
DBPRO_GLOBAL GlobStruct*				g_pGlob							= nullptr;
DBPRO_GLOBAL PTR_FuncCreateStr			g_pCreateDeleteStringFunction	= nullptr;

DBPRO_GLOBAL RetVoidMakeMemblock		g_MEM_Make						= nullptr;
DBPRO_GLOBAL RetVoidFreeMemblock		g_MEM_Free						= nullptr;
DBPRO_GLOBAL RetVoidGetMemblockSize		g_MEM_GetSize					= nullptr;
DBPRO_GLOBAL RetVoidSetMemblockSize		g_MEM_SetSize					= nullptr;
DBPRO_GLOBAL RetVoidGetMemblockPtr		g_MEM_GetPtr					= nullptr;

// Memblock Array Strcutre
//#define					MEMBLOCKSIZE 258
//DWORD					gpMemblockSize[MEMBLOCKSIZE];
//char*					gpMemblock[MEMBLOCKSIZE];

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////
// INTERNAL FUNCTIONS ////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

static inline bool IsReadablePointer(DWORD_PTR ptr)
{
	if (ptr <= 0x10000 || ptr >= 0x00007FFFFFFFFFFFULL)
		return false;
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(reinterpret_cast<const void*>(ptr), &mbi, sizeof(mbi)) != sizeof(mbi))
		return false;
	if (mbi.State != MEM_COMMIT)
		return false;
	if ((mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0)
		return false;
	if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
		return false;
	return true;
}

static inline size_t SafeStrLen(DWORD_PTR ptr)
{
	if (!IsReadablePointer(ptr))
		return 0;
	__try {
		const char* s = reinterpret_cast<const char*>(ptr);
		return strlen(s);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

static inline void SafeStrCopy(char* dest, DWORD_PTR src, size_t maxLen)
{
	if (!dest || maxLen == 0) return;
	dest[0] = '\0';
	if (!IsReadablePointer(src))
		return;
	__try {
		const char* s = reinterpret_cast<const char*>(src);
		size_t len = strlen(s);
		if (len >= maxLen) len = maxLen - 1;
		memcpy(dest, s, len);
		dest[len] = '\0';
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		dest[0] = '\0';
	}
}

DARKSDK void Constructor ( void )
{
	ZeroMemory(File, sizeof(File));
	ZeroMemory(FileEOF, sizeof(FileEOF));
	ZeroMemory(filetext, sizeof(filetext));
	ZeroMemory(&filedata, sizeof(filedata));
	ZeroMemory(&pVirtFileEncrypted, sizeof(pVirtFileEncrypted));

	{
		// local variables
		int				iIndex          = 0;		// drive index
		HANDLE			hDevice         = NULL;		// handle to device
		DWORD			dwBytesReturned = 0;		// bytes returned from io control

		DISK_GEOMETRY	drive;						// drive structure
		char			szDrive [ 256 ];			// to store drive name

		// get a handle to the device
		hDevice = CreateFile ( "\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );

		// check handle is valid
		if ( hDevice == INVALID_HANDLE_VALUE )
			return;

		// loop round for all drives
		while ( hDevice != INVALID_HANDLE_VALUE )
		{
			// get information
			if ( !DeviceIoControl (
										hDevice,						// handle to device
										IOCTL_DISK_GET_DRIVE_GEOMETRY,	// control type
										NULL,							// no input data
										0,								// use 0 because we have no input data
										&drive,							// pointer to drive structure
										sizeof ( DISK_GEOMETRY ),		// size of data
										&dwBytesReturned,				// number of bytes returned
										( LPOVERLAPPED ) NULL			// ignored
								  ) )
										return;

			// calculate the disk size
			ULONGLONG TotalBytes = drive.Cylinders.QuadPart * ( ULONG ) drive.TracksPerCylinder * ( ULONG ) drive.SectorsPerTrack * ( ULONG ) drive.BytesPerSector;
			
			g_HardDrives [ g_iHardDriveCount ].liCylinderCount     = drive.Cylinders;
			g_HardDrives [ g_iHardDriveCount ].dwTracksPerCylinder = drive.TracksPerCylinder;
			g_HardDrives [ g_iHardDriveCount ].dwSectorsPerTrack   = drive.SectorsPerTrack;
			g_HardDrives [ g_iHardDriveCount ].dwBytesPerSector    = drive.BytesPerSector;
			g_HardDrives [ g_iHardDriveCount ].ulTotalBytes        = TotalBytes;
			g_HardDrives [ g_iHardDriveCount ].ulTotalMB           = TotalBytes / 1024 / 1024;
			g_HardDrives [ g_iHardDriveCount ].ulTotalGB           = TotalBytes / 1024 / 1024 / 1024;

			g_iHardDriveCount++;

			// close this handle
			CloseHandle ( hDevice );

			// set up string for next drive
			sprintf ( szDrive, "\\\\.\\PhysicalDrive%d", ++iIndex );

			// access the next drive
			hDevice = CreateFile ( szDrive, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
		}
	}

}

DARKSDK void Destructor ( void )
{
	if (hInternalFile != -1 && hInternalFile != 0)
	{
		_findclose(hInternalFile);
		hInternalFile = -1;
	}
	for(DWORD f=0; f<MAX_FILES; f++)
	{
		if(File[f])
		{
			CloseHandle(File[f]);
			File[f]=NULL;
		}
		if(pVirtFileEncrypted[f])
		{
			delete pVirtFileEncrypted[f];
			pVirtFileEncrypted[f]=NULL;
		}
	}
}

DARKSDK void SetErrorHandler ( LPVOID pErrorHandlerPtr )
{
	// Update error handler pointer
	g_pErrorHandler = (CRuntimeErrorHandler*)pErrorHandlerPtr;
}

DARKSDK void PassCoreData( LPVOID pGlobPtr )
{
	// Held in Core, used here..
	g_pGlob = (GlobStruct*)pGlobPtr;
	if (!g_pGlob)
	{
		g_pCreateDeleteStringFunction = nullptr;
		return;
	}
	g_pCreateDeleteStringFunction = g_pGlob->CreateDeleteString;

	// Construct links to memblock access functions
	#ifndef DARKSDK_COMPILE
	if(g_pGlob->g_Memblocks)
	{
		g_MEM_Make = ( RetVoidMakeMemblock )		GetProcAddress ( g_pGlob->g_Memblocks, "?ExtMakeMemblock@@YAPEADHK@Z" );
		g_MEM_Free = ( RetVoidFreeMemblock )		GetProcAddress ( g_pGlob->g_Memblocks, "?ExtFreeMemblock@@YAXH@Z" );
		g_MEM_GetSize = ( RetVoidGetMemblockSize )	GetProcAddress ( g_pGlob->g_Memblocks, "?ExtGetMemblockSize@@YAKH@Z" );
		g_MEM_SetSize = ( RetVoidSetMemblockSize )	GetProcAddress ( g_pGlob->g_Memblocks, "?ExtSetMemblockSize@@YAXHK@Z" );
		g_MEM_GetPtr = ( RetVoidGetMemblockPtr )	GetProcAddress ( g_pGlob->g_Memblocks, "?ExtGetMemblockPtr@@YAPEADH@Z" );
	}
	#else
		g_MEM_Make		= dbExtMakeMemblock;
		g_MEM_Free		= dbExtFreeMemblock;
		g_MEM_GetSize	= dbExtGetMemblockSize;
		g_MEM_SetSize	= dbExtSetMemblockSize;
		g_MEM_GetPtr	= dbExtGetMemblockPtr;
	#endif
}

DARKSDK void FFindCloseFile(void)
{
	if (hInternalFile != -1 && hInternalFile != 0)
	{
		_findclose(hInternalFile);
	}
	hInternalFile = -1;
	FileReturnValue = -1;
}

DARKSDK void FFindFirstFile(void)
{
	if (hInternalFile != -1 && hInternalFile != 0) FFindCloseFile();
	hInternalFile = _findfirst("*.*", &filedata);
	if (hInternalFile != -1L)
	{
		// Success!
		FileReturnValue = 0;
	}
	else
	{
		FileReturnValue = -1;
		RunTimeWarning(RUNTIMEERROR_CANNOTSCANCURRENTDIR);
	}
}

DARKSDK int FGetFileReturnValue(void)
{
	return FileReturnValue;
}

DARKSDK void FFindNextFile(void)
{
	if (hInternalFile != -1 && hInternalFile != 0)
	{
		FileReturnValue = _findnext(hInternalFile, &filedata);
		if (FileReturnValue == -1)
		{
			FFindCloseFile();
		}
	}
	else
	{
		FileReturnValue = -1;
	}
}

DARKSDK int FGetActualTypeValue(int flagvalue)
{
	if(flagvalue & _A_SUBDIR)
		return 1;
	else
		return 0;
}

DARKSDK BOOL DB_FileExist(char* Filename)
{
	// If no string, no file
	if (Filename == NULL || !IsReadablePointer((DWORD_PTR)Filename) || Filename[0] == '\0')
		return FALSE;

	// Uses actual or virtual file..
	char VirtualFilename[_MAX_PATH];
	strcpy_s(VirtualFilename, sizeof(VirtualFilename), Filename);
	if (g_pGlob && g_pGlob->UpdateFilenameFromVirtualTable)
		g_pGlob->UpdateFilenameFromVirtualTable(VirtualFilename);

	CheckForWorkshopFile(VirtualFilename);

	DWORD dwAttrib = GetFileAttributesA(VirtualFilename);
	if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
		return TRUE;

	// Also check raw filename if VirtualFilename was modified
	if (strcmp(VirtualFilename, Filename) != 0)
	{
		dwAttrib = GetFileAttributesA(Filename);
		if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
			return TRUE;
	}

	// Open BASIC Script fallback
	HANDLE hfile = CreateFileA(VirtualFilename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hfile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hfile);
		return TRUE;
	}

	return FALSE;
}

DARKSDK DWORD DB_FileSize(char* Filename)
{
	if (Filename == NULL || !IsReadablePointer((DWORD_PTR)Filename) || Filename[0] == '\0')
		return 0;

	// Uses actual or virtual file..
	char VirtualFilename[_MAX_PATH];
	strcpy_s(VirtualFilename, sizeof(VirtualFilename), Filename);
	if (g_pGlob && g_pGlob->UpdateFilenameFromVirtualTable)
		g_pGlob->UpdateFilenameFromVirtualTable(VirtualFilename);

	CheckForWorkshopFile(VirtualFilename);

	WIN32_FILE_ATTRIBUTE_DATA fad{};
	if (GetFileAttributesExA(VirtualFilename, GetFileExInfoStandard, &fad))
	{
		if (!(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			return fad.nFileSizeLow;
	}

	if (strcmp(VirtualFilename, Filename) != 0)
	{
		if (GetFileAttributesExA(Filename, GetFileExInfoStandard, &fad))
		{
			if (!(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				return fad.nFileSizeLow;
		}
	}

	// Open BASIC Script fallback
	HANDLE hfile = CreateFileA(VirtualFilename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hfile == INVALID_HANDLE_VALUE)
		return 0;

	DWORD size = GetFileSize(hfile, NULL);
	CloseHandle(hfile);
	return size;
}

DARKSDK BOOL DB_FileWriteProtected(char* Filename)
{
	// If no string, no file
	if (Filename == NULL || !IsReadablePointer((DWORD_PTR)Filename) || Filename[0] == '\0')
		return FALSE;

	// Uses actual or virtual file..
	char VirtualFilename[_MAX_PATH];
	strcpy_s(VirtualFilename, sizeof(VirtualFilename), Filename);
	if (g_pGlob && g_pGlob->UpdateFilenameFromVirtualTable)
		g_pGlob->UpdateFilenameFromVirtualTable( VirtualFilename);

	CheckForWorkshopFile ( VirtualFilename );

	DWORD flags = GetFileAttributesA(VirtualFilename);
	if(flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_READONLY))
		return TRUE;
	else
		return FALSE;
}

DARKSDK BOOL DB_DeleteFile(char* Filename)
{
	if (!Filename || !IsReadablePointer((DWORD_PTR)Filename) || Filename[0] == '\0')
		return FALSE;

	std::error_code ec;
	return std::filesystem::remove(Filename, ec) ? TRUE : FALSE;
}

DARKSDK bool rFindFileInSub(char* currentpath, char* searchfile, char* returnpath)
{
	if (!currentpath || !searchfile || !returnpath)
		return false;

	std::error_code ec;
	std::filesystem::path root(currentpath);
	if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec))
		return false;

	std::string target(searchfile);
	for (char& c : target) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

	for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (ec) break;
		if (entry.is_regular_file(ec))
		{
			std::string entryFilename = entry.path().filename().string();
			for (char& c : entryFilename) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
			if (entryFilename == target)
			{
				std::string matchedPath = entry.path().string();
				strcpy_s(returnpath, 256, matchedPath.c_str());
				return true;
			}
		}
	}

	return false;
}

DARKSDK BOOL DB_FindFileInSubPath(char* filename, char* returnpath)
{
	if (!filename || !returnpath || !IsReadablePointer((DWORD_PTR)filename) || filename[0] == '\0')
		return FALSE;

	char path[MAX_PATH];
	if (!_getcwd(path, sizeof(path)))
		return FALSE;

	char searchfile[MAX_PATH];
	strcpy_s(searchfile, sizeof(searchfile), filename);
	_strlwr_s(searchfile, sizeof(searchfile));

	return rFindFileInSub(path, searchfile, returnpath) ? TRUE : FALSE;
}

DARKSDK BOOL DB_CanMakeFile(char* Filename)
{
	// Create Empty File
	HANDLE hfile = CreateFile(Filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hfile!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(hfile);
		DeleteFile(Filename);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

DARKSDK BOOL DB_MakeFile(char* Filename)
{
	// Create Empty File
	HANDLE hfile = CreateFile(Filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hfile==INVALID_HANDLE_VALUE)
		return FALSE;

	CloseHandle(hfile);
	return TRUE;
}

DARKSDK BOOL DB_CopyFile(char* From, char* To)
{
	if(DB_FileExist(From))
	{
		if(!CopyFile(From, To, TRUE))
			return FALSE;
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);

	return TRUE;
}

DARKSDK BOOL DB_MoveFile(char* From, char* To)
{
	if(DB_FileExist(From))
	{
		if(!MoveFile(From, To))
			return FALSE;
	}
	else
		RunTimeError(RUNTIMEERROR_FILEEXISTS);

	return TRUE;
}

DARKSDK BOOL DB_RenameFile(char* From, char* To)
{
	if(!MoveFile(From, To))
		return FALSE;

	return TRUE;
}

DARKSDK BOOL DB_PathExist(char* OriginalPathname)
{
	if (!OriginalPathname || !IsReadablePointer((DWORD_PTR)OriginalPathname) || OriginalPathname[0] == '\0')
		return FALSE;

	// Strip trailing backslashes/slashes for GetFileAttributes
	char szCleanPath[_MAX_PATH];
	strcpy_s(szCleanPath, sizeof(szCleanPath), OriginalPathname);
	size_t len = strlen(szCleanPath);
	while (len > 1 && (szCleanPath[len - 1] == '\\' || szCleanPath[len - 1] == '/'))
	{
		if (len == 3 && szCleanPath[1] == ':') break;
		szCleanPath[len - 1] = '\0';
		len--;
	}

	DWORD Attribs = GetFileAttributesA(szCleanPath);
	if (Attribs != INVALID_FILE_ATTRIBUTES && (Attribs & FILE_ATTRIBUTE_DIRECTORY))
		return TRUE;

	Attribs = GetFileAttributesA(OriginalPathname);
	if (Attribs != INVALID_FILE_ATTRIBUTES && (Attribs & FILE_ATTRIBUTE_DIRECTORY))
		return TRUE;

	return FALSE;
}

DARKSDK BOOL DB_MakeDir(char* Dirname)
{
	if (!Dirname || !IsReadablePointer((DWORD_PTR)Dirname) || Dirname[0] == '\0')
		return FALSE;

	std::error_code ec;
	return (std::filesystem::create_directories(Dirname, ec) || std::filesystem::exists(Dirname, ec)) ? TRUE : FALSE;
}

DARKSDK BOOL DB_DeleteDir(char* Dirname)
{
	if (!Dirname || !IsReadablePointer((DWORD_PTR)Dirname) || Dirname[0] == '\0')
		return FALSE;

	std::error_code ec;
	return std::filesystem::remove(Dirname, ec) ? TRUE : FALSE;
}

DARKSDK BOOL DB_DeleteDirRecursively(char* Dirname)
{
	if (!Dirname || !IsReadablePointer((DWORD_PTR)Dirname) || Dirname[0] == '\0')
		return FALSE;

	std::error_code ec;
	std::filesystem::path p(Dirname);
	if (std::filesystem::exists(p, ec))
	{
		std::filesystem::remove_all(p, ec);
		return !ec ? TRUE : FALSE;
	}
	return FALSE;
}

DARKSDK BOOL DB_ExecuteFile(HANDLE* phExecuteFileProcess, char* Operation, char* Filename, char* String, char* Path, bool bWaitForTermination )
{
	if(*phExecuteFileProcess)
	{
		CloseHandle(*phExecuteFileProcess);
		*phExecuteFileProcess=NULL;
	}
	if(bWaitForTermination==true)
	{
		SHELLEXECUTEINFO seinfo;
		ZeroMemory(&seinfo, sizeof(seinfo));
		seinfo.cbSize = sizeof(seinfo);
		seinfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
		seinfo.hwnd = NULL;
		seinfo.lpVerb = "open";
		seinfo.lpFile = Filename;
		seinfo.lpParameters = String;
		seinfo.lpDirectory = Path;
		seinfo.nShow = SW_SHOWDEFAULT;
		if(ShellExecuteEx(&seinfo)==TRUE)
		{
			*phExecuteFileProcess=seinfo.hProcess;
			return TRUE;
		}
		else
		{
			*phExecuteFileProcess=NULL;
			return FALSE;
		}
	}
	else
	{
		HINSTANCE hinstance = ShellExecuteA(	nullptr,
											"open",
											Filename,
											String,
											Path,
											SW_SHOWDEFAULT);
		if(reinterpret_cast<uintptr_t>(hinstance) <= 32)
			return FALSE;
		else
			return TRUE;
	}
}

DARKSDK BOOL DB_ExecuteFileIndi ( DWORD* dwExecuteFileProcess, char* Operation, char* Filename, char* String, char* Path, int iPriorityOfProcess )
{
	// create process data
	STARTUPINFOA si{};
	PROCESS_INFORMATION pi{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWDEFAULT;

	// directory must be absolute
	char szDirectory[_MAX_PATH] = {};
	char* pDirectory = nullptr;
	if ( Path && strlen ( Path ) > 0 )
	{
		if ( Path[1] == ':' )
		{
			// absolute
			strcpy_s ( szDirectory, sizeof(szDirectory), Path );
		}
		else
		{
			// relative
			_getcwd ( szDirectory, _MAX_PATH );
			strcat_s ( szDirectory, sizeof(szDirectory), "\\" );
			strcat_s ( szDirectory, sizeof(szDirectory), Path );
		}
		pDirectory = szDirectory;
	}

	// Concat Filename and Commandline String
	char pConcat[512] = {};
	strcpy_s ( pConcat, sizeof(pConcat), Filename ? Filename : "" );
	strcat_s ( pConcat, sizeof(pConcat), " " );
	strcat_s ( pConcat, sizeof(pConcat), String ? String : "" );

	// Process priority
	DWORD dwPriority = NORMAL_PRIORITY_CLASS;
	switch ( iPriorityOfProcess )
	{
		case 1 : dwPriority = HIGH_PRIORITY_CLASS;	break;
	}

	// Start the process. 
	if( CreateProcessA(	nullptr,
						pConcat,	
						nullptr, 
						nullptr, 
						FALSE,
						dwPriority,
						nullptr,
						pDirectory,
						&si, 
						&pi ) ) 
	{
		CloseHandle ( pi.hThread );
		CloseHandle ( pi.hProcess );
		if ( dwExecuteFileProcess ) *dwExecuteFileProcess = pi.dwProcessId;
		return TRUE;
	}
	else
	{
		if ( dwExecuteFileProcess ) *dwExecuteFileProcess = 0;
		return FALSE;
	}
}

DARKSDK BOOL DB_OpenToReadCore(int FileIndex, char* Filename)
{
	// Create file READ
	File[FileIndex] = CreateFile(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(File[FileIndex]==INVALID_HANDLE_VALUE)
	{
		File[FileIndex]=NULL;
		FileEOF[FileIndex]=FALSE;
		return FALSE;
	}
	return TRUE;
}

DARKSDK BOOL DB_OpenToRead(int FileIndex, char* Filename)
{
	BOOL bRes = FALSE;
	if(pVirtFileEncrypted[FileIndex]==NULL)
	{
		// Uses actual or virtual file..
		char VirtualFilename[_MAX_PATH];
		strcpy_s(VirtualFilename, sizeof(VirtualFilename), Filename);
		if (g_pGlob && g_pGlob->UpdateFilenameFromVirtualTable)
			g_pGlob->UpdateFilenameFromVirtualTable( VirtualFilename);

		CheckForWorkshopFile ( VirtualFilename );

		// Decrypt and use media
		if (g_pGlob && g_pGlob->Decrypt)
			g_pGlob->Decrypt( VirtualFilename );
		pVirtFileEncrypted[FileIndex] = new char[strlen(VirtualFilename)+1];
		strcpy_s(pVirtFileEncrypted[FileIndex], strlen(VirtualFilename)+1, VirtualFilename);
		bRes = DB_OpenToReadCore( FileIndex, VirtualFilename );
	}
	return bRes;
}

DARKSDK BOOL DB_OpenToWrite(int FileIndex, char* Filename)
{
	// Create file WRITE
	File[FileIndex] = CreateFileA(Filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(File[FileIndex]==INVALID_HANDLE_VALUE)
	{
		File[FileIndex]=NULL;
		FileEOF[FileIndex]=FALSE;
		return FALSE;
	}
	return TRUE;
}

DARKSDK BOOL DB_CloseFile(int FileIndex)
{
	if(File[FileIndex])
	{
		CloseHandle(File[FileIndex]);
		File[FileIndex]=NULL;
		FileEOF[FileIndex]=FALSE;

		// Re-encrypt
		if(pVirtFileEncrypted[FileIndex])
		{
			if (g_pGlob && g_pGlob->Encrypt)
				g_pGlob->Encrypt( pVirtFileEncrypted[FileIndex] );
			delete[] pVirtFileEncrypted[FileIndex];
			pVirtFileEncrypted[FileIndex]=NULL;
		}
	}
	return TRUE;
}


DARKSDK LPSTR GetReturnStringFromWorkString(char* WorkString = m_pWorkString)
{
	LPSTR pReturnString = nullptr;
	if (WorkString && g_pCreateDeleteStringFunction)
	{
		size_t dwSize = strlen(WorkString);
		g_pCreateDeleteStringFunction((DWORD_PTR*)&pReturnString, static_cast<DWORD>(dwSize + 1));
		if (pReturnString)
		{
			memcpy(pReturnString, WorkString, dwSize);
			pReturnString[dwSize] = '\0';
		}
	}
	return pReturnString;
}

//
// Command Functions
//

DARKSDK void SetDir( DWORD_PTR pString )
{
	if (IsReadablePointer(pString))
	{
		const char* path = reinterpret_cast<const char*>(pString);
		if (path[0] != '\0' && _chdir(path) != -1)
		{
			return;
		}
	}
	RunTimeError(RUNTIMEERROR_PATHCANNOTBEFOUND);
}

DARKSDK void Dir(void)
{
	// Show CWD..
	getcwd(filetext, _MAX_PATH);
	strcat(filetext, ":");
	g_pGlob->PrintStringFunction(filetext, true);

	// List Files..
	FFindFirstFile();
	while(FGetFileReturnValue()!=-1L)
	{
		if(filedata.attrib & _A_SUBDIR)	
			wsprintf(filetext, "<dir>%s", filedata.name);
		else
			wsprintf(filetext, "%s", filedata.name);
		
		g_pGlob->PrintStringFunction(filetext, true);
		FFindNextFile();
	}
	FFindCloseFile();
}

DARKSDK void DriveList(void)
{
	// List Drives..
	char storedrive[_MAX_PATH];
	getcwd(storedrive, _MAX_PATH);
	_strlwr(storedrive);
	for(int drive = 1; drive <= 26; drive++)
	{
		if(!_chdrive( drive ))
		{
			wsprintf(filetext, "%c:", drive + 'A' - 1);
			g_pGlob->PrintStringFunction(filetext, true);
		}
	}
	_chdrive( storedrive[0] - 'a' + 1 );
}

DARKSDK void ChecklistForFiles(void)
{
	// Checklist flags
	g_pGlob->checklisthasvalues=true;
	g_pGlob->checklisthasstrings=true;
	g_pGlob->checklistexists=true;

	g_dwMaxStringSizeInEnum=0;
	g_bCreateChecklistNow=false;
	for(int pass=0; pass<2; pass++)
	{
		if(pass==1)
		{
			// Ensure checklist is large enough
			g_bCreateChecklistNow=true;
			for(int c=0; c<g_pGlob->checklistqty; c++)
				GlobExpandChecklist(c, g_dwMaxStringSizeInEnum);
		}

		FFindFirstFile();
		g_pGlob->checklistqty=0;
		while(FGetFileReturnValue()!=-1L)
		{
			size_t dwSize = strlen(filedata.name)+1;
			if(dwSize>g_dwMaxStringSizeInEnum) g_dwMaxStringSizeInEnum=static_cast<DWORD>(dwSize);
			if(g_bCreateChecklistNow)
			{
				strcpy(g_pGlob->checklist[g_pGlob->checklistqty].string, filedata.name);
				g_pGlob->checklist[g_pGlob->checklistqty].valuea = FGetActualTypeValue(filedata.attrib);
			}
			g_pGlob->checklistqty++;
			FFindNextFile();
		}
		FFindCloseFile();
	}
}

DARKSDK void ChecklistForDrives(void)
{
	// mike - 250604
	char szList [ 26 ] [ 255 ];
	int  iCount = 0;

	memset ( szList, 0, sizeof ( szList ) );

	strcpy_s ( szList [ iCount++ ], sizeof(szList[0]), "a:\\" );

	for ( int iCounter = 0; iCounter < MAX_HARD_DRIVE; iCounter++ )
	{
		if ( GetDriveType ( g_HardDiskLetters [ iCounter ] ) == DRIVE_FIXED )
			strcpy_s ( szList [ iCount++ ], sizeof(szList[0]), g_HardDiskLetters [ iCounter ] );

		if ( GetDriveType ( g_HardDiskLetters [ iCounter ] ) == DRIVE_CDROM )
			strcpy_s ( szList [ iCount++ ], sizeof(szList[0]), g_HardDiskLetters [ iCounter ] );
	}

	
	g_pGlob->checklisthasvalues=false;
	g_pGlob->checklisthasstrings=true;
	g_pGlob->checklistexists=true;

	g_pGlob->checklistqty = iCount;

	for(int c=0; c<g_pGlob->checklistqty; c++)
		GlobExpandChecklist(c, 255);

	for( int c=0; c<g_pGlob->checklistqty; c++)
	{
		strcpy_s(g_pGlob->checklist[c].string, 255, szList[c]);
	}

	/*
	char storedrive[_MAX_PATH];
	getcwd(storedrive, _MAX_PATH);
	_strlwr(storedrive);

	// Checklist flags
	g_pGlob->checklisthasvalues=false;
	g_pGlob->checklisthasstrings=true;
	g_pGlob->checklistexists=true;

	g_dwMaxStringSizeInEnum=0;
	g_bCreateChecklistNow=false;
	for(int pass=0; pass<2; pass++)
	{
		if(pass==1)
		{
			// Ensure checklist is large enough
			g_bCreateChecklistNow=true;
			for(int c=0; c<g_pGlob->checklistqty; c++)
				GlobExpandChecklist(c, g_dwMaxStringSizeInEnum);
		}

		g_pGlob->checklistqty=0;
		//for(int drive = 1; drive <= 26; drive++)
		for(int drive = 2; drive <= 26; drive++)
		{
			if(!_chdrive( drive ))
			{
				wsprintf(filetext, "%c:", drive + 'A' - 1);
				DWORD dwSize = static_cast<DWORD>(strlen(filetext)+1);
				if(dwSize>g_dwMaxStringSizeInEnum) g_dwMaxStringSizeInEnum=dwSize;
				if(g_bCreateChecklistNow)
				{
					strcpy(g_pGlob->checklist[g_pGlob->checklistqty].string, filetext);
				}
				g_pGlob->checklistqty++;
			}
		}
	}

	_chdrive( storedrive[0] - 'a' + 1 );
	*/
	
}

DARKSDK void FindFirst(void)
{
	FFindFirstFile();
}

DARKSDK void FindNext(void)
{
	if (hInternalFile != -1 && hInternalFile != 0)
	{
		FFindNextFile();
	}
	else
	{
		FileReturnValue = -1;
	}
}

DARKSDK int CanMakeFile( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
		return 0;
	// 031107 - used to determine if in LIMITED USER AREA (no write)
	if(DB_CanMakeFile((LPSTR)pFilename))
		return 1;
	else
		return 0;
}

DARKSDK void MakeFile( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTMAKEFILE);
		return;
	}
	if(!DB_MakeFile((LPSTR)pFilename))
		RunTimeWarning(RUNTIMEERROR_CANNOTMAKEFILE);
}

DARKSDK void DeleteFile( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeSoftWarning(RUNTIMEERROR_CANNOTDELETEFILE);
		return;
	}
	// LEEADD - 190803 - Will set the file to NORMAL so can delete READONLY files too
	SetFileAttributesA ( (LPSTR)pFilename, FILE_ATTRIBUTE_NORMAL );

	if(DB_FileExist((LPSTR)pFilename))
	{
		if (!DeleteFileA((LPSTR)pFilename))
		{
			DWORD dwErr = GetLastError();
			if(dwErr==ERROR_SHARING_VIOLATION)
			{
				RunTimeWarning(RUNTIMEERROR_FILEISLOCKED);
			}
			else
			{
				RunTimeSoftWarning(RUNTIMEERROR_CANNOTDELETEFILE);
			}
		}
	}
	else
	{
		RunTimeSoftWarning(RUNTIMEERROR_CANNOTDELETEFILE);
	}
}

DARKSDK void CopyFileCore( DWORD_PTR pFromFilename, DWORD_PTR pFilename2 )
{
	if (!IsReadablePointer(pFromFilename) || !IsReadablePointer(pFilename2))
	{
		RunTimeWarning(RUNTIMEERROR_FILEEXISTS);
		return;
	}
	if(!DB_CopyFile((LPSTR)pFromFilename, (LPSTR)pFilename2 ))
		RunTimeWarning(RUNTIMEERROR_FILEEXISTS);
}

DARKSDK void CopyFile( DWORD_PTR szFilename, DWORD_PTR pFilename2 )
{
	if (!IsReadablePointer(szFilename) || !IsReadablePointer(pFilename2))
		return;

	// Uses actual or virtual file..
	char VirtualFilename[_MAX_PATH];
	SafeStrCopy(VirtualFilename, szFilename, sizeof(VirtualFilename));
	if (g_pGlob && g_pGlob->UpdateFilenameFromVirtualTable)
		g_pGlob->UpdateFilenameFromVirtualTable( VirtualFilename);

	// Decrypt and use media, re-encrypt
	if (g_pGlob && g_pGlob->Decrypt)
		g_pGlob->Decrypt( VirtualFilename );
	CopyFileCore ( (DWORD_PTR)VirtualFilename, pFilename2 );
	if (g_pGlob && g_pGlob->Encrypt)
		g_pGlob->Encrypt( VirtualFilename );
}

DARKSDK void RenameFile( DWORD_PTR pFilename, DWORD_PTR pFilename2 )
{
	if (!IsReadablePointer(pFilename) || !IsReadablePointer(pFilename2))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTRENAMEFILE);
		return;
	}
	if(!DB_FileExist((LPSTR)pFilename2))
	{
		if(!DB_RenameFile((LPSTR)pFilename, (LPSTR)pFilename2 ))
			RunTimeWarning(RUNTIMEERROR_CANNOTRENAMEFILE);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILEEXISTS);
}

DARKSDK void MoveFile( DWORD_PTR pFilename, DWORD_PTR pFilename2 )
{
	if (!IsReadablePointer(pFilename) || !IsReadablePointer(pFilename2))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTMOVEFILE);
		return;
	}
	if(!DB_FileExist((LPSTR)pFilename2))
	{
		if(!DB_RenameFile((LPSTR)pFilename, (LPSTR)pFilename2))
			RunTimeWarning(RUNTIMEERROR_CANNOTMOVEFILE);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILEEXISTS);
}

DARKSDK void WriteByteToFile( DWORD_PTR pFilename, int iPos, int iByte )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
		return;
	}
	char FilenameString[_MAX_PATH];
	SafeStrCopy(FilenameString, pFilename, sizeof(FilenameString));
	if(DB_FileExist(FilenameString))
	{
		// Open file to be read
		HANDLE hreadfile = CreateFileA(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hreadfile!=INVALID_HANDLE_VALUE)
		{
			// Read file into memory
			DWORD bytesread;
			int filebuffersize = GetFileSize(hreadfile, NULL);	
			char* filebuffer = (char*)GlobalAlloc(GMEM_FIXED, filebuffersize);
			if (filebuffer)
			{
				ReadFile(hreadfile, filebuffer, filebuffersize, &bytesread, NULL); 
				CloseHandle(hreadfile);		

				// Modify byte
				int offset = iPos;
				if(offset>=0 && offset<filebuffersize)
					filebuffer[offset] = static_cast<char>(iByte);

				// Write back out again
				HANDLE hwritefile = CreateFileA(FilenameString, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if(hwritefile!=INVALID_HANDLE_VALUE)
				{
					// Write mem to file
					DWORD byteswritten;
					WriteFile(hwritefile, filebuffer, filebuffersize, &byteswritten, NULL); 
					CloseHandle(hwritefile);		
				}

				// Discard memory used
				GlobalFree(filebuffer);
				filebuffer=NULL;
			}
			else
			{
				CloseHandle(hreadfile);
			}
		}
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
}

DARKSDK int ReadByteFromFile( DWORD_PTR pFilename, int iPos )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
		return 0;
	}
	int iResult=0;
	char FilenameString[_MAX_PATH];
	SafeStrCopy(FilenameString, pFilename, sizeof(FilenameString));
	if(DB_FileExist(FilenameString))
	{
		// Open file to be read
		HANDLE hreadfile = CreateFileA(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hreadfile!=INVALID_HANDLE_VALUE)
		{
			// Read file into memory
			DWORD bytesread;
			int filebuffersize = GetFileSize(hreadfile, NULL);	
			char* filebuffer = (char*)GlobalAlloc(GMEM_FIXED, filebuffersize);
			if (filebuffer)
			{
				ReadFile(hreadfile, filebuffer, filebuffersize, &bytesread, NULL); 
				CloseHandle(hreadfile);		

				// Read byte
				int data = 0;
				int offset = iPos;
				if(offset>=0 && offset<filebuffersize) data = static_cast<unsigned char>(filebuffer[offset]);
				iResult=data;

				// Discard memory used
				GlobalFree(filebuffer);
				filebuffer=NULL;
			}
			else
			{
				CloseHandle(hreadfile);
			}
		}
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);

	return iResult;
}

DARKSDK void MakeDir( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTMAKEDIR);
		return;
	}
	if(!DB_MakeDir((LPSTR)pFilename))
		RunTimeWarning(RUNTIMEERROR_CANNOTMAKEDIR);
}

DARKSDK void DeleteDir( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTDELETEDIR);
		return;
	}
	if(!DB_DeleteDirRecursively((LPSTR)pFilename))
		RunTimeWarning(RUNTIMEERROR_CANNOTDELETEDIR);
}

DARKSDK void DeleteDir( DWORD_PTR pFilename, int iFlag )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTDELETEDIR);
		return;
	}
	if(iFlag==1)
	{
		if(!DB_DeleteDirRecursively((LPSTR)pFilename))
			RunTimeWarning(RUNTIMEERROR_CANNOTDELETEDIR);
	}
	else
	{
		if(!DB_DeleteDir((LPSTR)pFilename))
			RunTimeWarning(RUNTIMEERROR_CANNOTDELETEDIR);
	}
}

DARKSDK void ExecuteFile( DWORD_PTR pFilename, DWORD_PTR pFilename2, DWORD_PTR pFilename3 )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTEXECUTEFILE);
		return;
	}
	char szParam2[_MAX_PATH] = {};
	char szParam3[_MAX_PATH] = {};
	if (IsReadablePointer(pFilename2)) SafeStrCopy(szParam2, pFilename2, sizeof(szParam2));
	if (IsReadablePointer(pFilename3)) SafeStrCopy(szParam3, pFilename3, sizeof(szParam3));

	if(!DB_ExecuteFile(&ghExecuteFileProcess, const_cast<LPSTR>(""), (LPSTR)pFilename, szParam2, szParam3, false ))
		RunTimeWarning(RUNTIMEERROR_CANNOTEXECUTEFILE);
}

DARKSDK void ExecuteFileEx( DWORD_PTR pFilename, DWORD_PTR pFilename2, DWORD_PTR pFilename3, int iWaitForExeEndFlag )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTEXECUTEFILE);
		return;
	}
	char szParam2[_MAX_PATH] = {};
	char szParam3[_MAX_PATH] = {};
	if (IsReadablePointer(pFilename2)) SafeStrCopy(szParam2, pFilename2, sizeof(szParam2));
	if (IsReadablePointer(pFilename3)) SafeStrCopy(szParam3, pFilename3, sizeof(szParam3));

	if(!DB_ExecuteFile(&ghExecuteFileProcess, const_cast<LPSTR>(""),(LPSTR)pFilename, szParam2, szParam3, true ))
		RunTimeWarning(RUNTIMEERROR_CANNOTEXECUTEFILE);

	// Wait Here Until Exe Terminates
	if ( iWaitForExeEndFlag==1 )
	{
		while(ghExecuteFileProcess)
		{
			DWORD dwStatus;
			if(GetExitCodeProcess(ghExecuteFileProcess, &dwStatus)==TRUE)
			{
				if(dwStatus!=STILL_ACTIVE)
				{
					// Closes process after it deactivates
					CloseHandle(ghExecuteFileProcess);
					ghExecuteFileProcess=NULL;
				}
			}
			if(g_pGlob && g_pGlob->ProcessMessageFunction && g_pGlob->ProcessMessageFunction()==1)
			{
				// Closes process if main app terminates
				CloseHandle(ghExecuteFileProcess);
				ghExecuteFileProcess=NULL;
			}
		}
	}
}

DARKSDK DWORD ExecuteFileIndi( DWORD_PTR pFilename, DWORD_PTR pFilename2, DWORD_PTR pFilename3, int iPriorityOfProcess )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTEXECUTEFILE);
		return 0;
	}
	char szParam2[_MAX_PATH] = {};
	char szParam3[_MAX_PATH] = {};
	if (IsReadablePointer(pFilename2)) SafeStrCopy(szParam2, pFilename2, sizeof(szParam2));
	if (IsReadablePointer(pFilename3)) SafeStrCopy(szParam3, pFilename3, sizeof(szParam3));

	// Create process and return handle
	DWORD dwIndiExecuteFileProcess = 0;
	if(!DB_ExecuteFileIndi ( &dwIndiExecuteFileProcess, const_cast<LPSTR>(""),(LPSTR)pFilename, szParam2, szParam3, iPriorityOfProcess ) )
		RunTimeWarning(RUNTIMEERROR_CANNOTEXECUTEFILE);

	// return handle for later monitoring
	return dwIndiExecuteFileProcess;
}

DARKSDK DWORD ExecuteFileIndi( DWORD_PTR pFilename, DWORD_PTR pFilename2, DWORD_PTR pFilename3 )
{
	return ExecuteFileIndi( pFilename, pFilename2, pFilename3, 0 );
}

DARKSDK BOOL CALLBACK EnumWindowsProcForTerminator( HWND hwnd, LPARAM lParam )
{
	// check process ID of window
	DWORD dwProcessID = 0;
	GetWindowThreadProcessId ( hwnd, &dwProcessID );
	if ( dwProcessID == static_cast<DWORD>(lParam) )
	{
		// Post close message to any windows associated with process
		PostMessage ( hwnd, WM_CLOSE, 0, 0 );
	}
	return TRUE;
}

DARKSDK void StopExecutable ( DWORD dwIndiExecuteFileProcess )
{
	// check if exe in active and running
	if ( dwIndiExecuteFileProcess )
	{
		// Enumerate all windows and close any that are owned by the process ID
		EnumWindows ( EnumWindowsProcForTerminator, dwIndiExecuteFileProcess );
	}
}

DARKSDK int GetExecutableRunning ( DWORD dwIndiExecuteFileProcess )
{
	// running
	int iRunning=0;

	// check if exe in active and running
	if ( dwIndiExecuteFileProcess )
	{
		// get handle to process
		HANDLE hIndiExecuteFileProcess = OpenProcess ( PROCESS_QUERY_INFORMATION, TRUE, dwIndiExecuteFileProcess );

		DWORD dwStatus;
		if ( GetExitCodeProcess ( hIndiExecuteFileProcess, &dwStatus )==TRUE )
			if(dwStatus==STILL_ACTIVE)
				iRunning = 1;

		// close handle to process
		CloseHandle ( hIndiExecuteFileProcess );
	}

	// not running
	return iRunning;
}

//
// File Mapping Functions
//

DARKSDK void WriteFilemap ( DWORD_PTR pFilemapname, DWORD dwValue, DWORD_PTR pString, int iWriteType )
{
	if (!IsReadablePointer(pFilemapname))
		return;
	const char* szMapName = reinterpret_cast<const char*>(pFilemapname);
	if (szMapName[0] == '\0')
		return;

	// Open or create filemap
	HANDLE hFileMap = OpenFileMappingA(FILE_MAP_WRITE, TRUE, szMapName);
	if ( hFileMap == NULL )
		hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, szMapName);
	if ( hFileMap == NULL )
		return;

	LPVOID lpVoid = MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, 1024);
	if ( lpVoid == NULL )
		return;

	// Copy data to filemap
	if ( iWriteType == 0 )
	{
		*reinterpret_cast<DWORD*>(lpVoid) = dwValue;
	}
	else if ( iWriteType == 1 )
	{
		if (IsReadablePointer(pString))
		{
			const char* srcStr = reinterpret_cast<const char*>(pString);
			size_t dwStringSize = strlen(srcStr);
			if (dwStringSize > 1000) dwStringSize = 1000;
			*reinterpret_cast<DWORD*>(static_cast<char*>(lpVoid) + 4) = static_cast<DWORD>(dwStringSize);
			memcpy(static_cast<char*>(lpVoid) + 8, srcStr, dwStringSize);
			static_cast<char*>(lpVoid)[8 + dwStringSize] = '\0';
		}
	}

	// Release virtual file
	UnmapViewOfFile(lpVoid);
	// Keep hFileMap open so named shared memory persists across processes
}

DARKSDK void WriteFilemapValue ( DWORD_PTR pFilemapname, DWORD dwValue )
{
	// Write value to filemap
	WriteFilemap ( pFilemapname, dwValue, 0, 0 );
}

DARKSDK void WriteFilemapString ( DWORD_PTR pFilemapname, DWORD_PTR pString )
{
	// Write string to filemap
	if (IsReadablePointer(pString))
	{
		if (strlen(reinterpret_cast<const char*>(pString)) <= 1000)
			WriteFilemap ( pFilemapname, 0, pString, 1 );
	}
}

DARKSDK DWORD ReadFilemapValue ( DWORD_PTR pFilemapname )
{
	if (!IsReadablePointer(pFilemapname))
		return 0;

	const char* szMapName = reinterpret_cast<const char*>(pFilemapname);
	if (szMapName[0] == '\0')
		return 0;

	// Open or create filemap for reading
	HANDLE hFileMap = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, TRUE, szMapName);
	if ( hFileMap == NULL )
		hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, szMapName);
	if ( hFileMap == NULL )
		return 0;

	LPVOID lpVoid = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1024);
	if ( lpVoid == NULL )
		return 0;

	// Copy data from filemap
	DWORD dwValue = *reinterpret_cast<const DWORD*>(lpVoid);

	// Release virtual file
	UnmapViewOfFile(lpVoid);

	// return data
	return dwValue;
}

DARKSDK DWORD_PTR ReadFilemapString( DWORD_PTR pDestStr, DWORD_PTR pFilemapname )
{
	m_pWorkString[0] = '\0';

	if (IsReadablePointer(pFilemapname))
	{
		const char* szMapName = reinterpret_cast<const char*>(pFilemapname);
		if (szMapName[0] != '\0')
		{
			// Open or create filemap for reading
			HANDLE hFileMap = OpenFileMappingA(FILE_MAP_READ, TRUE, szMapName);
			if ( hFileMap == NULL )
				hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, szMapName);
			if ( hFileMap != NULL )
			{
				LPVOID lpVoid = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1024);
				if ( lpVoid != NULL )
				{
					// Safe copy from offset 8
					const char* src = static_cast<const char*>(lpVoid) + 8;
					size_t maxCopy = sizeof(m_pWorkString) - 1;
					size_t len = 0;
					while (len < maxCopy && len < (1024 - 8) && src[len] != '\0')
					{
						m_pWorkString[len] = src[len];
						len++;
					}
					m_pWorkString[len] = '\0';
					UnmapViewOfFile(lpVoid);
				}
			}
		}
	}

	// Create and return string
	if (pDestStr && g_pCreateDeleteStringFunction)
		g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString = GetReturnStringFromWorkString();

	// return data
	return reinterpret_cast<DWORD_PTR>(pReturnString);
}

//
// Sequential File Access Functions
//

DARKSDK void OpenToRead( int f, DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeError(RUNTIMEERROR_FILENOTEXIST);
		return;
	}

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]==NULL)
		{
			if(DB_FileExist((LPSTR)pFilename))
			{
				if(!DB_OpenToRead(f, (LPSTR)pFilename))
					RunTimeSoftWarning(RUNTIMEERROR_CANNOTOPENFILEFORREADING);
			}
			else
				RunTimeError(RUNTIMEERROR_FILENOTEXIST);
		}
		else
			RunTimeError(RUNTIMEERROR_FILEALREADYOPEN);
	}
	else
		RunTimeError(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void OpenToWrite( int f, DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_CANNOTOPENFILEFORWRITING);
		return;
	}

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]==NULL)
		{
			if(!DB_FileExist((LPSTR)pFilename))
			{
				if(!DB_OpenToWrite(f, (LPSTR)pFilename))
					RunTimeSoftWarning(RUNTIMEERROR_CANNOTOPENFILEFORWRITING);
			}
			else
				RunTimeWarning(RUNTIMEERROR_FILEEXISTS);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILEALREADYOPEN);
	}
	else
	{
		RunTimeError(RUNTIMEERROR_FILENUMBERINVALID);
	}
}

DARKSDK void CloseFile( int f )
{
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
			DB_CloseFile(f);
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK int ReadByte( int f )
{
	int iResult=0;
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Read from file
			uint8_t data;
			if(ReadFile(File[f], &data, sizeof(data), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
			if(bytes==0) FileEOF[f]=TRUE;

			iResult = data;
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);

	return iResult;
}

DARKSDK int ReadWord( int f )
{
	int iResult=0;
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Read from file
			uint16_t data;
			if(ReadFile(File[f], &data, sizeof(data), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
			if(bytes==0) FileEOF[f]=TRUE;

			iResult = data;
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);

	return iResult;
}

DARKSDK int ReadLong( int f )
{
	int iResult=0;
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Read from file
			DWORD data;
			if(ReadFile(File[f], &data, sizeof(data), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
			if(bytes==0) FileEOF[f]=TRUE;

			iResult = data;
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);

	return iResult;
}

DARKSDK DWORD ReadFloat( int f )
{
	float fResult=0.0f;
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Read from file
			float data;
			if(ReadFile(File[f], &data, sizeof(data), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
			if(bytes==0) FileEOF[f]=TRUE;

			fResult = data;
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);

	return *(DWORD*)&fResult;
}

DARKSDK DWORD_PTR ReadString( int f, DWORD_PTR pDestStr )
{
    /*
        20091129 v75 - IRM - http://forum.thegamecreators.com/?m=forum_view&t=81894&b=15
        Use of fixed size buffers (1024 bytes for m_pWorkString and 2048 for internal
        buffer in this function gave two chances for buffer overruns to crash the program.
        
        Replaced the use of these buffers with a single buffer implemented using an
        std::vector, and changed the routine GetReturnStringFromWorkString to allow an
        optional buffer to be provided (defaults to m_pWorkString).
    */

	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);

    LPSTR pReturnString=0;

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			uint8_t c=0;
            DWORD bytes;
            std::vector<char> WorkString;

			bool eof=false;
			do
			{
				if(ReadFile(File[f], &c, 1, &bytes, NULL)==0)
				{
					RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
					goto fileerror;
				}
				if(bytes==0)
                {
                    FileEOF[f]=TRUE;
					eof=true;
                }
				else if(c>=32 || c==9)
                {
                    WorkString.push_back(c);
                }
			} while((c>=32 || c==9) && !eof);

            WorkString.push_back(0);

			if(c==13)
			{
				if(ReadFile(File[f], &c, 1, &bytes, NULL)==0)
				{
					RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
					goto fileerror;
				}
				if(bytes==0) FileEOF[f]=TRUE;
			}

	        // Create and return string
	        pReturnString=GetReturnStringFromWorkString( &WorkString[0] );
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);

fileerror:

    return reinterpret_cast<DWORD_PTR>( pReturnString );
}

DARKSDK void ReadFileBlockCore(char* FilenameString, int f )
{
	// Get Size of fileblock
	DWORD bytes;
	DWORD nSize;
	ReadFile(File[f], &nSize, 4, &bytes, NULL);

	// Create mem for it
	char* pBuffer = (char*)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, nSize);
	if(pBuffer)
	{
		// Read it in
		ReadFile(File[f], pBuffer, nSize, &bytes, NULL);
		if(bytes==0) FileEOF[f]=TRUE;

		// Write it to file
		DWORD byteswritten;
		HANDLE hwritefile = CreateFile(FilenameString, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hwritefile!=INVALID_HANDLE_VALUE)
		{
			WriteFile(hwritefile, pBuffer, nSize, &byteswritten, NULL);
			CloseHandle(hwritefile);		
		}

		// Free buffer
		GlobalFree(pBuffer);
		pBuffer=NULL;
	}
}

DARKSDK void MakePathToThisFolder(char* thepathiwant)
{
	// get directory desired
	char file[_MAX_PATH];
	strcpy(file, thepathiwant);

	// Get path from filename (upto 8 nests)
	char filepath[8][256];
	int filepathindex=0;
	for(size_t n=0; n<8; n++)
		strcpy_s(filepath[n], sizeof(filepath[n]), "");

	for(size_t n=0; n<strlen(file); n++)
	{
		if(file[n]=='\\')
		{
			size_t o = 0;

			// Get folder name
			char folder[256];
			
			for(o=0; o<n; o++)
				folder[o]=file[o];
			folder[o]=0;

			// Copy and store it
			strcpy_s(filepath[filepathindex], sizeof(filepath[filepathindex]), folder);
			if(filepathindex<7) filepathindex++;

			// Truncate and continue
			size_t q=0;
			for(size_t p=n+1; p<=strlen(file); p++)
				file[q++]=file[p];
			file[q]=0;
			n=0;
		}
	}

	// Store current directory
	char olddir[256];
	_getcwd(olddir, 256);

	// If filename has a path, and it doesnt exist, make it
	for(int m=0; m<filepathindex; m++)
	{
		if(strcmp(filepath[m],"")!=0)
		{
			if(chdir(filepath[m])==-1)
			{
				mkdir(filepath[m]);
				chdir(filepath[m]);
			}
		}
	}

	// Restore directory
	chdir(olddir);
}

DARKSDK void ReadFileBlock( int f, DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
		return;
	}

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			char* FilenameString = (LPSTR)pFilename;

			// Store current directory
			char olddir[256];
			getcwd(olddir, 256);

			// If directory doesn't exist, create one
			char DirString[256];
			strcpy(DirString, FilenameString);

			// mike - 020206 - addition for vs8
			int n = 0;
			//for(int n=strlen(DirString)-1; n>0 ; n--)
			for(n=static_cast<int>(strlen(DirString))-1; n>0 ; n--)
				if(DirString[n]=='\\') break;
			DirString[n+1]=0;
			MakePathToThisFolder(DirString);

			// Create file (nicely into dir if created earlier)
			ReadFileBlockCore(FilenameString, f);

			// Restore directory
			chdir(olddir);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void SkipBytes( int f, int iSkipValue )
{
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			DWORD nSize = (int)iSkipValue;
			char* pBuffer = (char*)GlobalAlloc(GMEM_FIXED, nSize);
			if(pBuffer)
			{
				// Read from file skippable bytes
				if(ReadFile(File[f], pBuffer, nSize, &bytes, NULL)==0)
					RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
				if(bytes<nSize) FileEOF[f]=TRUE;
				GlobalFree(pBuffer);
				pBuffer=NULL;
			}
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void ReadDirBlock( int f, DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
		return;
	}

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Find Directory to write
			char* DirString = (LPSTR)pFilename;

			// Store current directory
			char olddir[256];
			getcwd(olddir, 256);

			// If directory doesn't exist, create one
			char pNewDir[_MAX_PATH];
			strcpy(pNewDir, DirString);
			size_t dwLength=strlen(pNewDir);
			if(pNewDir[dwLength-1]!='\\') { pNewDir[dwLength]='\\'; pNewDir[dwLength+1]=0; } 
			MakePathToThisFolder(pNewDir);
			chdir(pNewDir);

			// Read number of files in dirblock
			DWORD bytes;
			DWORD NumberOfFiles=0;
			if(ReadFile(File[f], &NumberOfFiles, sizeof(NumberOfFiles), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
			if(bytes==0) FileEOF[f]=TRUE;

			// Load all files in
			for(unsigned int n=0; n<NumberOfFiles; n++)
			{
				// Read size of filename
				int stringlength=0;
				if(ReadFile(File[f], &stringlength, sizeof(stringlength), &bytes, NULL)==0)
					RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
				if(bytes==0) FileEOF[f]=TRUE;
				if(stringlength>0)
				{
					// Read filename
					char* FilenameString = (char*)GlobalAlloc(GMEM_FIXED, stringlength);
					if(FilenameString)
					{
						if(ReadFile(File[f], FilenameString, stringlength, &bytes, NULL)==0)
							RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);

						// Get path from filename (upto 8 nests)
						MakePathToThisFolder(FilenameString);

						// Read fileblock
						ReadFileBlockCore(FilenameString, f);

						// Release string
						GlobalFree(FilenameString);
						FilenameString=NULL;
					}
					else
					{
						RunTimeWarning(RUNTIMEERROR_CANNOTREADFROMFILE);
					}
				}
			}

			// Restore directory
			chdir(olddir);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteByte( int f, int iValue )
{
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Write to file
			uint8_t data = static_cast<uint8_t>(iValue);
			if(WriteFile(File[f], &data, sizeof(data), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteWord( int f, int iValue )
{
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Write to file
			uint16_t data = static_cast<uint16_t>(iValue);
			if(WriteFile(File[f], &data, sizeof(data), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteLong( int f, int iValue )
{
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=nullptr)
		{
			// Write to file
			DWORD data = static_cast<DWORD>(iValue);
			if(WriteFile(File[f], &data, sizeof(data), &bytes, nullptr)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteFloat( int f, float fValue )
{
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Write to file
			float data = fValue;
			if(WriteFile(File[f], &data, sizeof(data), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteString( int f, DWORD_PTR pString )
{
    /*
        20091129 v75 - IRM - http://forum.thegamecreators.com/?m=forum_view&t=108603&b=15
        Copy of the input string to an internal buffer of fixed size (2k) caused buffer
        overruns into the stack and program crashes.

        Elimitated the buffer by writing from the original string (if set).
    */

    char carriage[3];
	carriage[0]=13;
	carriage[1]=10;
	carriage[2]=0;

	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
            // Only write the string if it is valid
            if (IsReadablePointer(pString))
            {
                LPSTR string = (char*)pString;
                size_t stringlength=strlen(string);

                // 20091129 v75 - IRM - Only write the string if >0 bytes
                if (stringlength)
                {
                    // 20091129 v75 - IRM - Write directly from the input data, not using a secondary buffer
    			    if(WriteFile(File[f], string, static_cast<DWORD>(stringlength), &bytes, NULL)==0)
	    			    RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);
                }
            }

			// Write Carriage-Return to file
			if(WriteFile(File[f], carriage, 2, &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteFileBlockCore( char* FilenameString, int f, int mode )
{
	DWORD bytes;
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Read fileblock from file
			DWORD bytesread;
			HANDLE hreadfile = CreateFile(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hreadfile!=INVALID_HANDLE_VALUE)
			{
				// Get Size of fileblock
				DWORD nSize = GetFileSize(hreadfile, NULL);

				// Create mem for it
				char* pBuffer = (char*)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, nSize);
				if(pBuffer)
				{
					// Read the data into buffer
					ReadFile(hreadfile, pBuffer, nSize, &bytesread, NULL);

					// Close handle to file
					CloseHandle(hreadfile);		
					hreadfile=NULL;

					// Write it out to my file
					if(mode==1)
					{
						// Don't write size, cannot read back!
					}
					else
						WriteFile(File[f], &nSize, 4, &bytes, NULL);

					WriteFile(File[f], pBuffer, nSize, &bytes, NULL);

					// Free buffer
					GlobalFree(pBuffer);
					pBuffer=NULL;
				}
			}
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteFileBlock( int f, DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
		return;
	}
	char* FilenameString = (LPSTR)pFilename;
	WriteFileBlockCore(FilenameString, f, 0);
}

DARKSDK void WriteFileBlock( int f, DWORD_PTR pFilename, int iFlag )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
		return;
	}
	char* FilenameString = (LPSTR)pFilename;
	WriteFileBlockCore(FilenameString, f, 1);
}

DARKSDK void WriteDirContents(int f, char* newdir, bool bMode, DWORD* pCount, char* relativedir)
{
	// Remember current dir
	char olddir[256];
	getcwd(olddir,256);

	// Switch to new dir
	chdir(newdir);

	// Go through dir and write out all files
	int res=-1;
	struct _finddata_t localdata;
	intptr_t hLocalFile = _findfirst("*.*", &localdata);
	if(strcmp(localdata.name,".")==0) res=_findnext(hLocalFile, &localdata);
	if(strcmp(localdata.name,"..")==0) res=_findnext(hLocalFile, &localdata);
	while(res!=-1L)
	{
		if(FGetActualTypeValue(localdata.attrib)==1)
		{
			char thisrelativedir[256];
			strcpy(thisrelativedir, relativedir);
			strcat(thisrelativedir, localdata.name);
			strcat(thisrelativedir, "\\");
			WriteDirContents(f, localdata.name, bMode, pCount, thisrelativedir);
		}
		else
		{
			if(bMode)
			{
				// Get size of filename
				DWORD bytes;
				char string[256];
				strcpy(string, relativedir);
				strcat(string, localdata.name);
				DWORD stringlength=static_cast<DWORD>(strlen(string)+1);

				// Write size of filename first
				if(WriteFile(File[f], &stringlength, sizeof(stringlength), &bytes, NULL)==0)
					RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);

				// Write filename string second
				if(WriteFile(File[f], string, stringlength, &bytes, NULL)==0)
					RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);

				// Write actual fileblock
				WriteFileBlockCore(localdata.name, f, 0);
			}
			else
			{
				int inc = *(pCount);
				*(pCount)=inc+1;
			}
		}
		res=_findnext(hLocalFile, &localdata);
	}
	_findclose(hLocalFile);

	// Restore old dir
	chdir(olddir);
}

DARKSDK void WriteDirBlock( int f, DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename))
	{
		RunTimeWarning(RUNTIMEERROR_FILENOTEXIST);
		return;
	}

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			// Find Directory to write
			char* DirString = (LPSTR)pFilename;

			// Count files in dirblock
			DWORD Count=0;
			char RelativeDir[256];
			strcpy(RelativeDir,"");
			WriteDirContents(f, DirString, false, &Count, RelativeDir);

			// Write Header to DirBlock
			DWORD bytes;
			if(WriteFile(File[f], &Count, sizeof(Count), &bytes, NULL)==0)
				RunTimeWarning(RUNTIMEERROR_CANNOTWRITETOFILE);

			// Write all files in dir
			strcpy(RelativeDir,"");
			WriteDirContents(f, DirString, true, &Count, RelativeDir);
		}
		else
			RunTimeWarning(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void ReadMemblock( int f, int mbi )
{
	// mike - 011005 - quit if invalid pointer
	if ( !g_MEM_Make )
		return;

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			if(mbi>=1 && mbi<=255)
			{
				// Get Size of MEMBLOCK
				DWORD bytes, size;
				if(ReadFile(File[f], &size, 4, &bytes, NULL)!=0)
				{
					// Create memblock memory
					LPSTR pMem = g_MEM_Make ( mbi, size );
					if(pMem)
					{
						// Gey Data of MEMBLOCK from FILE
						if(ReadFile(File[f], pMem, size, &bytes, NULL)==0)
							RunTimeError(RUNTIMEERROR_CANNOTREADFROMFILE);
					}
					else
						RunTimeError(RUNTIMEERROR_MEMBLOCKCREATIONFAILED);
				}
				else
					RunTimeError(RUNTIMEERROR_CANNOTREADFROMFILE);
			}
			else
				RunTimeError(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL);
		}
		else
			RunTimeError(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeError(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void MakeMemblockFromFile( int mbi, int f )
{
	// mike - 011005 - quit if invalid pointer
	if ( !g_MEM_Make )
		return;

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			if(mbi>=1 && mbi<=255)
			{
				// Get file size
				DWORD dwSize = GetFileSize(File[f], NULL);
				if(dwSize>0)
				{
					// Get Size of MEMBLOCK
					DWORD bytes;

					// Create memblock memory
					LPSTR pMem = g_MEM_Make ( mbi, dwSize );
					if(pMem)
					{
						// Gey Data of MEMBLOCK from FILE
						if(ReadFile(File[f], pMem, dwSize, &bytes, NULL)==0)
							RunTimeError(RUNTIMEERROR_CANNOTREADFROMFILE);
					}
					else
						RunTimeError(RUNTIMEERROR_MEMBLOCKCREATIONFAILED);
				}
				else
					RunTimeError(RUNTIMEERROR_CANNOTREADFROMFILE);
			}
			else
				RunTimeError(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL);
		}
		else
			RunTimeError(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeError(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void MakeFileFromMemblock( int f, int mbi )
{
	// mike - 011005 - quit if invalid pointer
	if ( !g_MEM_GetPtr || !g_MEM_GetSize )
		return;

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			if(mbi>=1 && mbi<=255)
			{
				LPSTR pPtr = g_MEM_GetPtr ( mbi );
				if(pPtr)
				{
					// Write it to file (difference from WRIITE MEMBLOCK is that no size DWORD is written = pure file)
					DWORD bytes;
					DWORD dwSize = g_MEM_GetSize ( mbi );

					// Write MEMBLOCK to FILE
					if(WriteFile(File[f], pPtr, dwSize, &bytes, NULL)==0)
						RunTimeError(RUNTIMEERROR_CANNOTWRITETOFILE);
				}
				else
					RunTimeError(RUNTIMEERROR_MEMBLOCKNOTEXIST);
			}
			else
				RunTimeError(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL);
		}
		else
			RunTimeError(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeError(RUNTIMEERROR_FILENUMBERINVALID);
}

DARKSDK void WriteMemblock( int f, int mbi )
{
	// mike - 011005 - quit if invalid pointer
	if ( !g_MEM_GetPtr || !g_MEM_GetSize )
		return;

	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
		{
			if(mbi>=1 && mbi<=255)
			{
				LPSTR pPtr = g_MEM_GetPtr ( mbi );
				if(pPtr)
				{
					// Write MEMBLOCK Size to FILE
					DWORD dwSize = g_MEM_GetSize ( mbi );
					DWORD bytes;
					if(WriteFile(File[f], &dwSize, 4, &bytes, NULL)!=0)
					{
						// Write MEMBLOCK to FILE
						if(WriteFile(File[f], pPtr, dwSize, &bytes, NULL)==0)
							RunTimeError(RUNTIMEERROR_CANNOTWRITETOFILE);
					}
					else
						RunTimeError(RUNTIMEERROR_CANNOTWRITETOFILE);
				}
				else
					RunTimeError(RUNTIMEERROR_MEMBLOCKNOTEXIST);
			}
			else
				RunTimeError(RUNTIMEERROR_MEMBLOCKRANGEILLEGAL);
		}
		else
			RunTimeError(RUNTIMEERROR_FILENOTOPEN);
	}
	else
		RunTimeError(RUNTIMEERROR_FILENUMBERINVALID);
}

//		
// Command Expression Functions
//

DARKSDK DWORD_PTR GetDir( DWORD_PTR pDestStr )
{
	// Create and return string
	if (!_getcwd(m_pWorkString, sizeof(m_pWorkString)))
		m_pWorkString[0] = '\0';
	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

DARKSDK DWORD_PTR GetFileName( DWORD_PTR pDestStr )
{
	if(hInternalFile != -1 && hInternalFile != 0 && FileReturnValue != -1)
		strcpy_s(m_pWorkString, sizeof(m_pWorkString), filedata.name);
	else
		m_pWorkString[0] = '\0';

	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

DARKSDK int GetFileType( void )
{
	if(FileReturnValue==-1L || hInternalFile==-1 || hInternalFile==0)
		return -1;
	else
		return FGetActualTypeValue(filedata.attrib);
}

DARKSDK DWORD_PTR GetFileDate( DWORD_PTR pDestStr )
{
	if(hInternalFile != -1 && hInternalFile != 0 && FileReturnValue != -1)
	{
		const char* timestr = ctime(&(filedata.time_write));
		if (timestr)
			sprintf_s(m_pWorkString, sizeof(m_pWorkString), "%.24s", timestr);
		else
			m_pWorkString[0] = '\0';
	}
	else
		m_pWorkString[0] = '\0';

	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

DARKSDK DWORD_PTR GetFileCreation( DWORD_PTR pDestStr )
{
	if(hInternalFile != -1 && hInternalFile != 0 && FileReturnValue != -1)
	{
		const char* timestr = ctime(&(filedata.time_create));
		if (timestr)
			sprintf_s(m_pWorkString, sizeof(m_pWorkString), "%.24s", timestr);
		else
			m_pWorkString[0] = '\0';
	}
	else
		m_pWorkString[0] = '\0';

	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

DARKSDK int FileExist( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename)) return 0;
	return DB_FileExist((LPSTR)pFilename) ? 1 : 0;
}

DARKSDK int FileSize( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename)) return 0;
	return DB_FileSize((LPSTR)pFilename);
}

DARKSDK int PathExist( DWORD_PTR pFilename )
{
	if (!IsReadablePointer(pFilename)) return 0;
	return DB_PathExist((LPSTR)pFilename) ? 1 : 0;
}

DARKSDK int FileOpen( int f )
{
	if(f>=1 && f<=MAX_FILES)
	{
		if(File[f]!=NULL)
			return 1;
		else
			return 0;
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);

	return 0;
}

DARKSDK int FileEnd( int f )
{
	if(f>=1 && f<=MAX_FILES)
	{
		if(FileEOF[f]==TRUE)
			return 1;
		else
			return 0;
	}
	else
		RunTimeWarning(RUNTIMEERROR_FILENUMBERINVALID);

	return 0;
}

DARKSDK DWORD_PTR Appname( DWORD_PTR pDestStr )
{
	// Create and return string
	GetModuleFileNameA(g_pGlob ? g_pGlob->hInstance : NULL, m_pWorkString, sizeof(m_pWorkString));
	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

DARKSDK DWORD_PTR Windir( DWORD_PTR pDestStr )
{
	// Create and return string
	GetWindowsDirectoryA(m_pWorkString, sizeof(m_pWorkString));	
	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

DARKSDK DWORD_PTR Mydocdir( DWORD_PTR pDestStr )
{
	// lee - 040407 - return the My Documents folder in full
	SHGetFolderPathA( NULL, CSIDL_PERSONAL, NULL, 0, m_pWorkString );
	if(pDestStr && g_pCreateDeleteStringFunction) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// DARK SDK SECTION //////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#ifdef DARKSDK_COMPILE

void ConstructorFile ( void )
{
	Constructor ( );
}

void DestructorFile  ( void )
{
	Destructor ( );
}

void SetErrorHandlerFile ( LPVOID pErrorHandlerPtr )
{
	SetErrorHandler ( pErrorHandlerPtr );
}

void PassCoreDataFile( LPVOID pGlobPtr )
{
	PassCoreData ( pGlobPtr );
}

void dbSetDir ( char* pString )
{
	SetDir ( (DWORD_PTR)pString );
}

void dbDir ( void )
{
	Dir ( );
}

void dbDriveList ( void )
{
	DriveList ( );
}

void dbPerformCheckListForFiles ( void )
{
	ChecklistForFiles ( );
}

void dbPerformCheckListForDrives ( void )
{
	ChecklistForDrives ( );
}

void dbFindFirst ( void )
{
	FindFirst ( );
}

void dbFindNext ( void )
{
	FindNext ( );
}

void dbMakeFile ( char* pFilename )
{
	MakeFile ( (DWORD_PTR)pFilename );
}

void dbDeleteFile ( char* pFilename )
{
	DeleteFile ( (DWORD_PTR)pFilename );
}

void dbCopyFile ( char* pFilename, char* pFilename2 )
{
	CopyFile ( (DWORD_PTR)pFilename, (DWORD_PTR)pFilename2 );
}

void dbRenameFile ( char* pFilename, char* pFilename2 )
{
	RenameFile ( (DWORD_PTR)pFilename, (DWORD_PTR)pFilename2 );
}

void dbMoveFile ( char* pFilename, char* pFilename2 )
{
	MoveFile ( (DWORD_PTR)pFilename, (DWORD_PTR)pFilename2 );
}

void dbWriteByteToFile ( char* pFilename, int iPos, int iByte )
{
	WriteByteToFile ( (DWORD_PTR)pFilename, iPos, iByte );
}

int dbReadByteFromFile ( char* pFilename, int iPos )
{
	return ReadByteFromFile ( (DWORD_PTR)pFilename, iPos );
}

void dbMakeDirectory ( char* pFilename )
{
	MakeDir ( (DWORD_PTR)pFilename );
}

void dbDeleteDirectory ( char* pFilename )
{
	DeleteDir ( (DWORD_PTR)pFilename );
}

void dbDeleteDirectory ( char* pFilename, int iFlag )
{
	//DeleteDirEx ( pFilename, iFlag );
}

void dbExecuteFile ( char* pFilename, char* pFilename2, char* pFilename3 )
{
	ExecuteFile ( (DWORD_PTR)pFilename, (DWORD_PTR)pFilename2, (DWORD_PTR)pFilename3 );
}

void dbExecuteFile ( char* pFilename, char* pFilename2, char* pFilename3, int iFlag )
{
	ExecuteFileEx ( (DWORD_PTR)pFilename, (DWORD_PTR)pFilename2, (DWORD_PTR)pFilename3, iFlag );
}

DWORD dbExecuteExecutable ( char* pFilename, char* pFilename2, char* pFilename3 )
{
	return ExecuteFileIndi ( (DWORD_PTR)pFilename, (DWORD_PTR)pFilename2, (DWORD_PTR)pFilename3 );
}

DWORD dbExecuteExecutable ( char* pFilename, char* pFilename2, char* pFilename3, int iPriority )
{
	return ExecuteFileIndi ( (DWORD_PTR)pFilename, (DWORD_PTR)pFilename2, (DWORD_PTR)pFilename3, iPriority );
}

void dbStopExecutable ( DWORD hIndiExecuteFileProcess )
{
	StopExecutable ( hIndiExecuteFileProcess );
}

void dbWriteFilemapValue ( char* pFilemapname, DWORD dwValue )
{
	WriteFilemapValue ( (DWORD_PTR)pFilemapname, dwValue );
}

void dbWriteFilemapString ( char* pFilemapname, char* pString )
{
	WriteFilemapString ( (DWORD_PTR)pFilemapname, (DWORD_PTR)pString );
}

DWORD dbReadFilemapValue ( char* pFilemapname )
{
	return ReadFilemapValue ( (DWORD_PTR)pFilemapname );
}

char* dbReadFilemapString ( char* pFilemapname )
{
	static char* szReturn = nullptr;
	DWORD_PTR    dwReturn = ReadFilemapString ( 0, reinterpret_cast<DWORD_PTR>(pFilemapname) );

	szReturn = reinterpret_cast<char*>(dwReturn);

	return szReturn;
}

DWORD_PTR dbReadFilemapString( DWORD_PTR pDestStr, DWORD_PTR pFilemapname )
{
	return ReadFilemapString ( (DWORD_PTR)pDestStr, (DWORD_PTR)pFilemapname );
}

void dbOpenToRead ( int f, char* pFilename )
{
	OpenToRead ( f, (DWORD_PTR)pFilename );
}

void dbOpenToWrite ( int f, char* pFilename )
{
	OpenToWrite ( f, (DWORD_PTR)pFilename );
}

void dbCloseFile ( int f )
{
	CloseFile ( f );
}

int dbReadByte ( int f )
{
	return ReadByte ( f );
}

int dbReadWord ( int f )
{
	return ReadWord ( f );
}

int dbReadFile ( int f )
{
	return ReadLong ( f );
}

float dbReadFloat ( int f )
{
	DWORD dwReturn = ReadFloat ( f );
	
	return *( float* ) &dwReturn;
}

char* dbReadString ( int f )
{
	static char* szReturn = NULL;
	DWORD		 dwReturn = ReadString ( f, NULL );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

void dbReadFileBlock ( int f, char* pFilename )
{
	ReadFileBlock ( f, (DWORD_PTR)pFilename );
}

void dbSkipBytes ( int f, int iSkipValue )
{
	SkipBytes ( f, iSkipValue );
}

void dbReadDirBlock ( int f, char* pFilename )
{
	ReadDirBlock ( f, (DWORD_PTR)pFilename );
}

void dbWriteByte ( int f, int iValue )
{
	WriteByte ( f, iValue );
}

void dbWriteWord ( int f, int iValue )
{
	WriteWord ( f, iValue );
}

void dbWriteLong ( int f, int iValue )
{
	WriteLong ( f, iValue );
}

void dbWriteFloat ( int f, float fValue )
{
	WriteFloat ( f, fValue );
}

void dbWriteString ( int f, char* pString )
{
	WriteString ( f, (DWORD_PTR)pString );
}

void dbWriteFileBlock ( int f, char* pFilename )
{
	WriteFileBlock ( f, (DWORD_PTR)pFilename );
}

void dbWriteFileBlockEx ( int f, char* pFilename, int iFlag )
{
	//WriteFileBlockEx ( f, pFilename, iFlag );
}

void dbWriteDirBlock ( int f, char* pFilename )
{
	WriteDirBlock ( f, (DWORD_PTR)pFilename );
}

void dbReadMemblock ( int f, int mbi )
{
	ReadMemblock ( f, mbi );
}

void dbMakeMemblockFromFile	( int mbi, int f )
{
	MakeMemblockFromFile ( mbi, f );
}

void dbWriteMemblock ( int f, int mbi )
{
	WriteMemblock ( f, mbi );
}

void dbMakeFileFromMemblock	( int f, int mbi )
{
	MakeFileFromMemblock ( f, mbi );
}

char* dbGetDir ( void )
{
	static char* szReturn = nullptr;
	DWORD_PTR    dwReturn = GetDir ( 0 );

	szReturn = reinterpret_cast<char*>(dwReturn);

	return szReturn;
}

char* dbGetFileName ( void )
{
	static char* szReturn = nullptr;
	DWORD_PTR    dwReturn = GetFileName ( 0 );

	szReturn = reinterpret_cast<char*>(dwReturn);

	return szReturn;
}

int dbGetFileType ( void )
{
	return GetFileType ( );
}

char* dbGetFileDate ( void )
{
	static char* szReturn = nullptr;
	DWORD_PTR    dwReturn = GetFileDate ( 0 );

	szReturn = reinterpret_cast<char*>(dwReturn);

	return szReturn;
}

char* dbGetFileCreation ( void )
{
	static char* szReturn = nullptr;
	DWORD_PTR    dwReturn = GetFileCreation ( 0 );

	szReturn = reinterpret_cast<char*>(dwReturn);

	return szReturn;
}

int dbFileExist ( char* pFilename )
{
	return FileExist ( reinterpret_cast<DWORD_PTR>(pFilename) );
}

int dbFileSize ( char* pFilename )
{
	return FileSize ( reinterpret_cast<DWORD_PTR>(pFilename) );
}

int dbPathExist ( char* pFilename )
{
	return PathExist ( reinterpret_cast<DWORD_PTR>(pFilename) );
}

int dbFileOpen ( int f )
{
	return FileOpen ( f );
}

int dbFileEnd ( int f )
{
	return FileEnd ( f );
}

char* dbAppname ( void )
{
	static char* szReturn = nullptr;
	DWORD_PTR    dwReturn = Appname ( 0 );

	szReturn = reinterpret_cast<char*>(dwReturn);

	return szReturn;
}

char* dbWindir ( void )
{
	static char* szReturn = nullptr;
	DWORD_PTR    dwReturn = Windir ( 0 );

	szReturn = reinterpret_cast<char*>(dwReturn);

	return szReturn;
}

int dbExecutableRunning ( DWORD hIndiExecuteFileProcess )
{
	return GetExecutableRunning ( hIndiExecuteFileProcess );
}

//
// lee - 300706 - GDK fixes
//
unsigned char dbReadByte ( int f, unsigned char* pByte )
{
	*pByte = dbReadByte ( f );
	return *pByte;
}
WORD dbReadWord	( int f, WORD* pWord )
{
	*pWord = dbReadWord ( f );
	return *pWord;
}
int	dbReadFile ( int f, int* pInteger )
{
	*pInteger = dbReadFile ( f );
	return *pInteger;
}
float dbReadFloat ( int f, float* pFloat )
{
	*pFloat = dbReadFloat ( f );
	return *pFloat;
}
char* dbReadString ( int f, char* pString )
{
	pString = dbReadString ( f );
	return pString;
}
int	dbReadLong ( int f, int* pLong )
{
	*pLong = ReadLong ( f );
	return *pLong;
}
void dbCD ( char* pPath )
{
	SetDir ( (DWORD_PTR)pPath );
}
void dbWriteFile ( int f, int iValue )
{
	WriteLong ( f, iValue );
}

#endif

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
