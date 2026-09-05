#define ZIP_PASSWORD "mypassword"

#include "stdafx.h"
#include "Zlib\cZip.h"
#include "core.h"
#include "Enchancements.h"
#include "CFileC.h"
#include <string>
#include <string_view>
#include <algorithm>
#include <filesystem>

void OpenFileBlock ( char* szFile, int iID, LPCSTR szKey );

const long	MAX_VOLUME_SIZE_BYTES	= 100000000;	// maximum size of the zip
const int	ZIP_COMPRESS_LEVEL		= 9;			// maximum compression level

#define MAX_ZIP	255

// RAII current working directory preservation
class CurrentDirectoryGuard
{
public:
	CurrentDirectoryGuard()
	{
		DWORD dwLen = GetCurrentDirectoryA(sizeof(m_szOriginalDir), m_szOriginalDir);
		m_bValid = (dwLen > 0 && dwLen < sizeof(m_szOriginalDir));
	}

	~CurrentDirectoryGuard()
	{
		if (m_bValid)
		{
			SetCurrentDirectoryA(m_szOriginalDir);
		}
	}

	CurrentDirectoryGuard(const CurrentDirectoryGuard&) = delete;
	CurrentDirectoryGuard& operator=(const CurrentDirectoryGuard&) = delete;

private:
	char m_szOriginalDir[MAX_PATH] = {};
	bool m_bValid = false;
};

// file block structure
struct sFileBlock
{
	cZip*			pZip;
	bool			bEncrypted;
	char			szName     [ MAX_PATH ];
	char			szZipName  [ MAX_PATH ];
	char			szPassword [ 256 ];
	int				iCompression;
	int				iFileCount;
	
	sFileBlock ( )
	{
		pZip = NULL;
		bEncrypted = false;
		memset ( szName,     0, sizeof ( szName     ) );
		memset ( szZipName,  0, sizeof ( szZipName  ) );
		memset ( szPassword, 0, sizeof ( szPassword ) );
		strcpy_s ( szPassword, sizeof(szPassword), "default" );
		iCompression = 9;
		iFileCount = 0;
	}
};

sFileBlock	g_FileBlocks    [ MAX_ZIP ];
char	    g_TempDirectory [ MAX_PATH ] = {};
char        g_szRestoreDir  [ MAX_PATH ] = {};

////////////////////////////////////////////////////////////////////
// INTERNAL HELPERS ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

bool CheckID ( int iID )
{
	if ( iID < 0 || iID >= MAX_ZIP )
	{
		Error ( 7 );
		return false;
	}
	return true;
}

bool CheckData ( int iID )
{
	if ( !CheckID ( iID ) )
		return false;

	if ( !g_FileBlocks [ iID ].pZip )
	{
		Error ( 11 );
		return false;
	}
	return true;
}

void SetupFileBlocks ( void )
{
	GetTempPathA ( sizeof(g_TempDirectory), g_TempDirectory );
}

void DestroyFileBlocks ( void )
{
	for ( int i = 0; i < MAX_ZIP; i++ )
	{
		if ( g_FileBlocks [ i ].pZip )
		{
			char szOriginalName [ MAX_PATH ] = "";
			std::string name(g_FileBlocks [ i ].szName);
			auto dot = name.rfind('.');
			if ( dot != std::string::npos )
			{
				strncpy_s ( szOriginalName, sizeof(szOriginalName), name.substr(0, dot).c_str(), _TRUNCATE );
			}
			else
			{
				strncpy_s ( szOriginalName, sizeof(szOriginalName), g_FileBlocks [ i ].szName, _TRUNCATE );
			}

			delete g_FileBlocks [ i ].pZip;
			g_FileBlocks [ i ].pZip = NULL;

			strcat_s ( szOriginalName, sizeof(szOriginalName), ".000" );
			DeleteFileA ( szOriginalName );
		}
	}
}

void SetTempDirectory ( void )
{
	GetCurrentDirectoryA ( sizeof(g_szRestoreDir), g_szRestoreDir );
	if ( g_TempDirectory[0] )
	{
		SetCurrentDirectoryA ( g_TempDirectory );
	}
}

void RestoreDirectory ( void )
{
	if ( g_szRestoreDir[0] )
	{
		SetCurrentDirectoryA ( g_szRestoreDir );
	}
}

void GetZipName ( int iID, const char* szFileName )
{
	if ( !CheckID ( iID ) || !szFileName )
		return;

	std::string name(szFileName);
	auto slashPos = name.find_last_of("/\\");
	auto dotPos = name.rfind('.');
	std::string base;
	if ( dotPos != std::string::npos && ( slashPos == std::string::npos || dotPos > slashPos ) )
	{
		base = name.substr(0, dotPos);
	}
	else
	{
		base = name;
	}
	base += ".zip";

	strncpy_s(g_FileBlocks[iID].szZipName, sizeof(g_FileBlocks[iID].szZipName), base.c_str(), _TRUNCATE);
}

////////////////////////////////////////////////////////////////////
// COMMAND FUNCTIONS ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void CreateFileBlock ( int iID, char* szFilename )
{
	if ( !CheckID ( iID ) || !szFilename )
		return;

	SAFE_DELETE ( g_FileBlocks [ iID ].pZip );
	DeleteFileA ( szFilename );

	g_FileBlocks [ iID ].pZip = NULL;
	g_FileBlocks [ iID ].bEncrypted = false;
	
	memset ( g_FileBlocks [ iID ].szName,     0, sizeof ( g_FileBlocks [ iID ].szName     ) );
	memset ( g_FileBlocks [ iID ].szZipName,  0, sizeof ( g_FileBlocks [ iID ].szZipName  ) );
	memset ( g_FileBlocks [ iID ].szPassword, 0, sizeof ( g_FileBlocks [ iID ].szPassword ) );
	
	strcpy_s ( g_FileBlocks [ iID ].szPassword, sizeof(g_FileBlocks [ iID ].szPassword), ZIP_PASSWORD );
	g_FileBlocks [ iID ].iCompression = 9;
	g_FileBlocks [ iID ].iFileCount = 0;

	g_FileBlocks [ iID ].pZip = new cZip;
	if ( !g_FileBlocks [ iID ].pZip )
	{
		Error ( 11 );
		return;
	}

	g_FileBlocks [ iID ].pZip->Open ( szFilename, ZIP_PASSWORD );
	strncpy_s ( g_FileBlocks [ iID ].szName, sizeof(g_FileBlocks [ iID ].szName), szFilename, _TRUNCATE );
}

void AddOrObtainResourceFromBlock ( int /*iID*/, int /*iIndex*/, char* /*dwFilename*/, int /*iType*/, int /*iMode*/ )
{
}

void AddObjectToBlock ( int iID, int /*iObject*/, char* /*dwFilename*/ )
{
	if ( !CheckID ( iID ) ) return;
}

void AddImageToBlock ( int iID, int /*iImage*/, char* /*dwFilename*/ )
{
	if ( !CheckID ( iID ) ) return;
}

void AddBitmapToBlock ( int iID, int /*iBitmap*/, char* /*dwFilename*/ )
{
	if ( !CheckID ( iID ) ) return;
}

void SetFileBlockKey ( int iID, char* /*dwKey*/ )
{
	if ( !CheckID ( iID ) ) return;
}

void SetFileBlockCompression ( int iID, int iLevel )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return;

	g_FileBlocks [ iID ].iCompression = iLevel;
}

void AddFileToBlock ( int iID, char* szFile )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) || !szFile )
		return;

	char newFileName[1024];
	strcpy_s ( newFileName, sizeof(newFileName), szFile );
	for ( int i = 0 ; i < (int)strlen ( newFileName ) ; i++ )
	{
		if ( newFileName[i] == '\\' ) newFileName[i] = '/';
	}

	g_FileBlocks [ iID ].pZip->Add ( newFileName, newFileName );
}

void RemoveFileFromBlock ( int iID, char* /*dwFile*/ )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return;
}

void SaveFileBlock ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return;

	g_FileBlocks [ iID ].pZip->Close ( );

	GetZipName ( iID, g_FileBlocks [ iID ].szName );

	if ( g_FileBlocks [ iID ].szZipName[0] &&
	     _stricmp( g_FileBlocks [ iID ].szZipName, g_FileBlocks [ iID ].szName ) != 0 &&
	     GetFileAttributesA( g_FileBlocks [ iID ].szZipName ) != INVALID_FILE_ATTRIBUTES )
	{
		MoveFileExA ( g_FileBlocks [ iID ].szZipName, g_FileBlocks [ iID ].szName, MOVEFILE_REPLACE_EXISTING );
	}
}

void CloseFileBlock ( int iID )
{
	if ( !CheckID ( iID ) )
		return;

	if ( g_FileBlocks [ iID ].pZip )
	{
		g_FileBlocks [ iID ].pZip->Close ( );
		SAFE_DELETE ( g_FileBlocks [ iID ].pZip );
	}
}

void ExtractFileFromBlock ( int iID, const char* szFilename, const char* szPath )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) || !szFilename || !szPath )
		return;

	CurrentDirectoryGuard cwdGuard;

	std::filesystem::path targetDir(szPath);
	std::filesystem::path targetFilePath = targetDir / szFilename;

	std::error_code ec;
	std::filesystem::create_directories(targetFilePath.parent_path(), ec);

	// Normalize lookup filename to forward slashes for zip compatibility
	char target[MAX_PATH] = {};
	strncpy_s(target, sizeof(target), szFilename, _TRUNCATE);
	for (int i = 0; target[i]; ++i)
	{
		if (target[i] == '\\') target[i] = '/';
	}

	// Case-insensitive lookup in zip entry table
	const char* actualZipName = target;
	unsigned int count = g_FileBlocks[iID].pZip->GetFileCount();
	for (unsigned int i = 0; i < count; ++i)
	{
		const char* fn = g_FileBlocks[iID].pZip->GetFilename(i);
		if (fn && _stricmp(fn, target) == 0)
		{
			actualZipName = fn;
			break;
		}
	}

	std::string outPathStr = targetFilePath.string();
	g_FileBlocks[iID].pZip->Extract(const_cast<char*>(actualZipName), outPathStr.data());
}

void OpenFileBlock ( char* szFile, int iID )
{
	if ( !CheckID ( iID ) || !szFile )
		return;

	OpenFileBlock ( szFile, iID, ZIP_PASSWORD );
}

void OpenFileBlock ( char* szFile, int iID, LPCSTR szKey )
{
	if ( !CheckID ( iID ) || !szFile )
		return;

	SAFE_DELETE ( g_FileBlocks [ iID ].pZip );

	strncpy_s ( g_FileBlocks [ iID ].szName, sizeof(g_FileBlocks [ iID ].szName), szFile, _TRUNCATE );
	strncpy_s ( g_FileBlocks [ iID ].szPassword, sizeof(g_FileBlocks [ iID ].szPassword), szKey ? szKey : ZIP_PASSWORD, _TRUNCATE );

	g_FileBlocks [ iID ].pZip = new cZip;
	if ( !g_FileBlocks [ iID ].pZip )
	{
		Error ( 11 );
		return;
	}

	if ( !g_FileBlocks [ iID ].pZip->Open ( szFile, szKey ? szKey : ZIP_PASSWORD ) )
	{
		SAFE_DELETE ( g_FileBlocks [ iID ].pZip );
		Error ( 133 );
		return;
	}

	g_FileBlocks [ iID ].iFileCount = g_FileBlocks [ iID ].pZip->GetFileCount();
}

void OpenFileBlockNoPw ( char* szFile, int iID, char* szKey )
{
	if ( !CheckID ( iID ) || !szFile )
		return;

	SAFE_DELETE ( g_FileBlocks [ iID ].pZip );

	strncpy_s ( g_FileBlocks [ iID ].szName, sizeof(g_FileBlocks [ iID ].szName), szFile, _TRUNCATE );
	strncpy_s ( g_FileBlocks [ iID ].szPassword, sizeof(g_FileBlocks [ iID ].szPassword), szKey ? szKey : "", _TRUNCATE );

	g_FileBlocks [ iID ].pZip = new cZip;
	if ( !g_FileBlocks [ iID ].pZip )
	{
		Error ( 11 );
		return;
	}

	if ( !g_FileBlocks [ iID ].pZip->Open ( szFile, szKey ? szKey : "" ) )
	{
		SAFE_DELETE ( g_FileBlocks [ iID ].pZip );
		Error ( 133 );
		return;
	}

	g_FileBlocks [ iID ].iFileCount = g_FileBlocks [ iID ].pZip->GetFileCount();
}

void DeleteFileBlock ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return;

	SAFE_DELETE ( g_FileBlocks [ iID ].pZip );
}

void PerformCheckListForFileBlockData ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return;

	if ( !g_pGlob )
	{
		Error ( 1 );	
		return;
	}

	g_pGlob->checklistexists     = true;
	g_pGlob->checklisthasstrings = true;
	g_pGlob->checklisthasvalues  = true;
	g_pGlob->checklistqty        = g_FileBlocks [ iID ].pZip->GetFileCount();

	for ( int i = 0; i < g_pGlob->checklistqty; i++ )
	{
		GlobExpandChecklist ( i, 255 );
		const char* fn = g_FileBlocks [ iID ].pZip->GetFilename(i);
		if ( fn && g_pGlob->checklist [ i ].string )
		{
			DWORD cap = g_pGlob->checklist [ i ].dwStringSize ? g_pGlob->checklist [ i ].dwStringSize : 255;
			strncpy_s ( g_pGlob->checklist [ i ].string, cap, fn, _TRUNCATE );
		}
	}
}

int GetFileBlockNumFiles ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return 0;

	return g_FileBlocks [ iID ].pZip->GetFileCount();
}

const char* GetFileBlockFileName ( int iID, unsigned int index )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return nullptr;

	if ( index >= static_cast<unsigned int>(g_FileBlocks [ iID ].pZip->GetFileCount()) )
		return nullptr;

	return g_FileBlocks [ iID ].pZip->GetFilename( index );
}

const char* const* GetFileBlockAllFileNames ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return nullptr;

	return g_FileBlocks [ iID ].pZip->GetAllFileNames();
}

const unsigned long* GetFileBlockAllFileSizes ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return nullptr;

	return g_FileBlocks [ iID ].pZip->GetAllFileSizes();
}

void LoadObjectFromBlock ( int iID, char* dwFile, int iObject )
{
	if ( !CheckID ( iID ) ) return;
	AddOrObtainResourceFromBlock ( iID, iObject, dwFile, 0, 1 );
}

void LoadBitmapFromBlock ( int iID, char* dwFile, int iBitmap )
{
	if ( !CheckID ( iID ) ) return;
	AddOrObtainResourceFromBlock ( iID, iBitmap, dwFile, 2, 1 );
}

void LoadImageFromBlock ( int iID, char* dwFile, int iImage )
{
	if ( !CheckID ( iID ) ) return;
	AddOrObtainResourceFromBlock ( iID, iImage, dwFile, 1, 1 );
}

void LoadSoundFromBlock ( int iID, char* /*dwFile*/, int /*iSound*/ )
{
	if ( !CheckID ( iID ) ) return;
}

void LoadFileFromBlock ( int iID, char* /*dwFile*/, int /*iFile*/ )
{
	if ( !CheckID ( iID ) ) return;
}

void LoadMemblockFromBlock ( int iID, char* /*dwFile*/, int /*iMemblock*/ )
{
	if ( !CheckID ( iID ) ) return;
}

void LoadAnimationFromBlock ( int iID, char* /*dwFile*/, int /*iAnimation*/ )
{
	if ( !CheckID ( iID ) ) return;
}

int GetFileBlockExists ( int iID )
{
	if ( !CheckID ( iID ) )
		return 0;

	return ( g_FileBlocks [ iID ].pZip != NULL ) ? 1 : 0;
}

int GetFileBlockSize ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return 0;

	SaveFileBlock ( iID );

	HANDLE hfile = GG_CreateFile ( g_FileBlocks [ iID ].szName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hfile == INVALID_HANDLE_VALUE )
		return 0;

	DWORD dwFileSizeHigh = 0;
	DWORD dwFileSize = GetFileSize ( hfile, &dwFileSizeHigh );
	CloseHandle ( hfile );

	return static_cast<int>( dwFileSize );
}

int GetFileBlockCount ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return 0;

	return g_FileBlocks [ iID ].pZip->GetFileCount();
}

int GetFileBlockDataExists ( int iID, char* szFile )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) || !szFile )
		return 0;

	char target[MAX_PATH] = {};
	strncpy_s ( target, sizeof(target), szFile, _TRUNCATE );
	for ( int i = 0; target[i]; ++i )
	{
		if ( target[i] == '\\' ) target[i] = '/';
	}

	unsigned int count = g_FileBlocks [ iID ].pZip->GetFileCount();
	for ( unsigned int i = 0; i < count; ++i )
	{
		const char* fn = g_FileBlocks [ iID ].pZip->GetFilename(i);
		if ( fn && _stricmp(fn, target) == 0 )
			return 1;
	}

	return 0;
}

int GetFileBlockKey ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return 0;

	return 1;
}

int GetFileBlockCompression ( int iID )
{
	if ( !CheckID ( iID ) || !CheckData ( iID ) )
		return 0;

	return g_FileBlocks [ iID ].iCompression;
}
