#if defined(__has_include)
#if __has_include("..\..\..\..\GameGuru\Include\preprocessor-flags.h")
#include "..\..\..\..\GameGuru\Include\preprocessor-flags.h"
#endif
#endif

#include "core.h"
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstring>
#include "ipc.h"

#ifdef ENABLEIMGUI
#include "..\..\..\..\GameGuru\Imgui\imgui.h"
#include "..\..\..\..\GameGuru\Imgui\imgui_impl_win32.h"
#include "..\..\..\..\GameGuru\Imgui\imgui_gg_dx11.h"
#endif

#include "CInputC.h"

struct sFileMap
{
	cIPC*  pIPC = nullptr;
	char   szName [ 256 ] = {};

	sFileMap ( ) = default;
};

sFileMap g_FileMap [ 256 ];

#define FILEMAP_ID_DWORD		0
#define FILEMAP_ID_STRING		1
#define FILEMAP_ID_FLOAT		2

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

#undef DARKSDK
#define DARKSDK __declspec(dllexport)

DARKSDK void  CreateFileMap    ( int iID, DWORD_PTR dwName, DWORD dwSize );
DARKSDK void  OpenFileMap      ( int iID, DWORD_PTR dwName );
DARKSDK void  CloseFileMap     ( int iID );
DARKSDK void  DestroyFileMap   ( int iID );

DARKSDK DWORD GetFileMapDWORD  ( int iID, DWORD dwOffset );
DARKSDK DWORD_PTR GetFileMapString ( DWORD_PTR dwDestStr, int iID, DWORD dwOffset );
DARKSDK DWORD GetFileMapFloat  ( int iID, DWORD dwOffset );

DARKSDK void  SetFileMapDWORD  ( int iID, DWORD dwOffset, DWORD dwValue  );
DARKSDK void  SetFileMapString ( int iID, DWORD dwOffset, DWORD_PTR dwString );
DARKSDK void  SetFileMapFloat  ( int iID, DWORD dwOffset, float fValue );

DARKSDK void  SetEventAndWait  ( int iID );

bool CheckFileMapID ( int iID )
{
	if ( iID < 0 || iID > 255 )
	{
		Error ( 7 );
		return false;
	}

	return true;
}

DARKSDK void CreateFileMap ( int iID, DWORD_PTR dwName, DWORD dwSize )
{
	if ( !CheckFileMapID ( iID ) )
		return;

	if ( !IsReadablePointer(dwName) )
		return;

	char szSafeName[256];
	SafeStrCopy(szSafeName, dwName, sizeof(szSafeName));
	if ( szSafeName[0] == '\0' )
		return;

	SAFE_DELETE ( g_FileMap [ iID ].pIPC );
	g_FileMap [ iID ].pIPC = new cIPC ( szSafeName, dwSize );
	strcpy_s ( g_FileMap [ iID ].szName, sizeof(g_FileMap [ iID ].szName), szSafeName );
}

DARKSDK void OpenFileMap ( int iID, DWORD_PTR dwName )
{
	if ( !CheckFileMapID ( iID ) )
		return;

	if ( !IsReadablePointer(dwName) )
		return;

	char szSafeName[256];
	SafeStrCopy(szSafeName, dwName, sizeof(szSafeName));
	if ( szSafeName[0] == '\0' )
		return;

	if ( g_FileMap [ iID ].pIPC == nullptr )
	{		
		g_FileMap [ iID ].pIPC = new cIPC ( szSafeName, 0 );
		strcpy_s ( g_FileMap [ iID ].szName, sizeof(g_FileMap [ iID ].szName), szSafeName );
	}
	else
	{
		if ( strcmp ( g_FileMap [ iID ].szName, szSafeName ) != 0 )
		{
			SAFE_DELETE ( g_FileMap [ iID ].pIPC );
			g_FileMap [ iID ].pIPC = new cIPC ( szSafeName, 0 );
			strcpy_s ( g_FileMap [ iID ].szName, sizeof(g_FileMap [ iID ].szName), szSafeName );
		}
	}
}

DARKSDK void CloseFileMap ( int iID )
{
	return;
}

DARKSDK void DestroyFileMap ( int iID )
{
	if ( !CheckFileMapID ( iID ) )
		return;

	SAFE_DELETE ( g_FileMap [ iID ].pIPC );
	g_FileMap [ iID ].szName[0] = '\0';
}

DARKSDK DWORD GetFileMapDWORD ( int iID, DWORD dwOffset )
{
	if ( !CheckFileMapID(iID) || !g_FileMap [ iID ].pIPC )
		return 0;

	DWORD dwValue = 0;
	g_FileMap [ iID ].pIPC->ReceiveBuffer ( &dwValue, dwOffset, sizeof ( dwValue ) );
	return dwValue;
}

DARKSDK DWORD_PTR GetFileMapString ( DWORD_PTR dwDestStr, int iID, DWORD dwOffset )
{
	if ( dwDestStr && g_pGlob && g_pGlob->CreateDeleteString )
	{
		g_pGlob->CreateDeleteString ( (DWORD_PTR*)&dwDestStr, 0 );
	}

	if ( !CheckFileMapID(iID) || !g_FileMap [ iID ].pIPC )
		return 0;

	char szString [ 256 ] = {};
	g_FileMap [ iID ].pIPC->ReceiveBuffer ( szString, dwOffset, sizeof ( szString ) - 1 );
	szString [ sizeof ( szString ) - 1 ] = '\0';

	DWORD dwSize        = static_cast<DWORD>( strlen ( szString ) );
	char* pReturnString	= nullptr;	
	
	if ( g_pGlob && g_pGlob->CreateDeleteString )
	{
		g_pGlob->CreateDeleteString((DWORD_PTR*)&pReturnString, dwSize + 1 );
	}
	if ( pReturnString )
	{
		strcpy_s ( pReturnString, dwSize + 1, szString );
	}

	return reinterpret_cast<DWORD_PTR>(pReturnString);
}

DARKSDK DWORD GetFileMapFloat ( int iID, DWORD dwOffset )
{
	if ( !CheckFileMapID(iID) || !g_FileMap [ iID ].pIPC )
		return 0;

	float fValue = 0.0f;
	g_FileMap [ iID ].pIPC->ReceiveBuffer ( &fValue, dwOffset, sizeof ( fValue ) );
	DWORD dwResult = 0;
	memcpy ( &dwResult, &fValue, sizeof(dwResult) );
	return dwResult;
}

DARKSDK void SetFileMapDWORD ( int iID, DWORD dwOffset, DWORD dwValue )
{
	if ( !CheckFileMapID(iID) || !g_FileMap [ iID ].pIPC )
		return;

	g_FileMap [ iID ].pIPC->SendBuffer ( &dwValue, dwOffset, sizeof ( dwValue ) );
}

DARKSDK void SetFileMapString ( int iID, DWORD dwOffset, DWORD_PTR dwString )
{
	if ( !CheckFileMapID(iID) || !g_FileMap [ iID ].pIPC )
		return;

	if ( !IsReadablePointer(dwString) )
		return;

	size_t stringLen = SafeStrLen(dwString);
	const char* pString = reinterpret_cast<const char*>(dwString);

	// Clear out destination slot
	char szBlank [ 256 ] = {};
	g_FileMap [ iID ].pIPC->SendBuffer ( szBlank, dwOffset, sizeof ( szBlank ) );

	if ( stringLen > 0 )
	{
		g_FileMap [ iID ].pIPC->SendBuffer ( pString, dwOffset, static_cast<DWORD>(stringLen) );
	}
}

DARKSDK void SetFileMapFloat ( int iID, DWORD dwOffset, float fValue )
{
	if ( !CheckFileMapID(iID) || !g_FileMap [ iID ].pIPC )
		return;

	g_FileMap [ iID ].pIPC->SendBuffer ( &fValue, dwOffset, sizeof ( fValue ) );
}

DARKSDK void SetEventAndWait ( int iID )
{
	if ( !CheckFileMapID(iID) || !g_FileMap [ iID ].pIPC )
		return;

	HANDLE hEvent = g_FileMap [ iID ].pIPC->m_hDataEvent;
	if ( !hEvent ) return;

	DWORD dwTimeOutDelay = 5000;
	ResetEvent ( hEvent );
	WaitForSingleObject ( hEvent, dwTimeOutDelay );
}
