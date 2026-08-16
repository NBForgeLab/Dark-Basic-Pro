#define _CRT_SECURE_NO_WARNINGS

#if defined(__has_include)
#if __has_include("..\..\..\..\GameGuru\Include\preprocessor-flags.h")
#include "..\..\..\..\GameGuru\Include\preprocessor-flags.h"
#endif
#endif

#include "core.h"
#include <vector>
#include "ipc.h"

#ifdef ENABLEIMGUI
#include "..\..\..\..\GameGuru\Imgui\imgui.h"
#include "..\..\..\..\GameGuru\Imgui\imgui_impl_win32.h"
#include "..\..\..\..\GameGuru\Imgui\imgui_gg_dx11.h"
#endif

#include "CInputC.h"

struct sFileMap
{
	cIPC*  pIPC;
	char   szName [ 256 ];

	sFileMap ( )
	{
		pIPC = NULL;

		strcpy ( szName, "" );

		//hCreate = NULL;
		//hOpen   = NULL;
		//pData   = NULL;	
	}

	
};

sFileMap g_FileMap [ 256 ];

#define FILEMAP_ID_DWORD		0
#define FILEMAP_ID_STRING		1
#define FILEMAP_ID_FLOAT		2

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

#undef DARKSDK
#define DARKSDK __declspec(dllexport)

DARKSDK void  CreateFileMap    ( int iID, DWORD dwName, DWORD dwSize );
DARKSDK void  OpenFileMap      ( int iID, DWORD dwName );
DARKSDK void  CloseFileMap     ( int iID );
DARKSDK void  DestroyFileMap   ( int iID );

DARKSDK DWORD GetFileMapDWORD  ( int iID, DWORD dwOffset );
DARKSDK DWORD GetFileMapString ( DWORD dwDestStr, int iID, DWORD dwOffset );
DARKSDK DWORD GetFileMapFloat  ( int iID, DWORD dwOffset );

DARKSDK void  SetFileMapDWORD  ( int iID, DWORD dwOffset, DWORD dwValue  );
DARKSDK void  SetFileMapString ( int iID, DWORD dwOffset, DWORD dwString );
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

DARKSDK void CreateFileMap ( int iID, DWORD dwName, DWORD dwSize )
{
	if ( !CheckFileMapID ( iID ) )
		return;

	char* pName = ( char* ) ( uintptr_t ) dwName;
	SAFE_DELETE ( g_FileMap [ iID ].pIPC );

	g_FileMap [ iID ].pIPC = new cIPC ( pName, dwSize );

	if ( pName )
		strcpy ( g_FileMap [ iID ].szName, pName );
}

DARKSDK void OpenFileMap ( int iID, DWORD dwName )
{
	if ( !CheckFileMapID ( iID ) )
		return;

	char* pName = ( char* ) ( uintptr_t ) dwName;
	if ( !pName )
		return;

	if ( g_FileMap [ iID ].pIPC == NULL )
	{		
		g_FileMap [ iID ].pIPC = new cIPC ( pName, 0 );
		strcpy ( g_FileMap [ iID ].szName, pName );
	}
	else
	{
		if ( strcmp ( g_FileMap [ iID ].szName, pName ) != 0 )
		{
			SAFE_DELETE ( g_FileMap [ iID ].pIPC );
			g_FileMap [ iID ].pIPC = new cIPC ( pName, 0 );
			strcpy ( g_FileMap [ iID ].szName, pName );
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
	strcpy ( g_FileMap [ iID ].szName, "" );
}

DARKSDK DWORD GetFileMapDWORD ( int iID, DWORD dwOffset )
{
	if ( !g_FileMap [ iID ].pIPC )
		return 0;

	DWORD dwValue = 0;
	g_FileMap [ iID ].pIPC->ReceiveBuffer ( &dwValue, dwOffset, sizeof ( dwValue ) );
	return dwValue;
}

DARKSDK DWORD GetFileMapString ( DWORD dwDestStr, int iID, DWORD dwOffset )
{
	if ( !g_FileMap [ iID ].pIPC )
		return 0;

	char szString [ 256 ] = "";
	g_FileMap [ iID ].pIPC->ReceiveBuffer ( &szString, dwOffset, sizeof ( szString ) );

	DWORD dwSize        = (DWORD)strlen ( szString );
	char* pReturnString	= NULL;	
	
	g_pGlob->CreateDeleteString((DWORD_PTR*)&pReturnString, dwSize + 1 );
	if ( pReturnString )
	{
		strcpy ( pReturnString, szString );
	}

	return (DWORD)(uintptr_t)pReturnString;
}

DARKSDK DWORD GetFileMapFloat ( int iID, DWORD dwOffset )
{
	if ( !g_FileMap [ iID ].pIPC )
		return 0;

	float fValue = 0;
	g_FileMap [ iID ].pIPC->ReceiveBuffer ( &fValue, dwOffset, sizeof ( fValue ) );
	return *( DWORD* ) &fValue;
}

DARKSDK void SetFileMapDWORD ( int iID, DWORD dwOffset, DWORD dwValue )
{
	if ( !g_FileMap [ iID ].pIPC )
		return;

	g_FileMap [ iID ].pIPC->SendBuffer ( &dwValue, dwOffset, sizeof ( dwValue ) );
}

DARKSDK void SetFileMapString ( int iID, DWORD dwOffset, DWORD dwString )
{
	if ( !g_FileMap [ iID ].pIPC )
		return;

	char* pString         = ( char* ) ( uintptr_t ) dwString;
	if ( !pString )
		return;

	char  szBlank [ 255 ] = "";
	g_FileMap [ iID ].pIPC->SendBuffer ( szBlank, dwOffset, sizeof ( szBlank ) );
	g_FileMap [ iID ].pIPC->SendBuffer ( pString, dwOffset, (DWORD)strlen ( pString ) * sizeof ( char ) );
}

DARKSDK void SetFileMapFloat ( int iID, DWORD dwOffset, float fValue )
{
	if ( !g_FileMap [ iID ].pIPC )
		return;

	g_FileMap [ iID ].pIPC->SendBuffer ( &fValue, dwOffset, sizeof ( fValue ) );
}

DARKSDK void SetEventAndWait ( int iID )
{
	if ( !g_FileMap [ iID ].pIPC )
		return;

	HANDLE hEvent = g_FileMap [ iID ].pIPC->m_hDataEvent;
	if ( !hEvent ) return;

	DWORD dwTimeOutDelay = 5000;
	ResetEvent ( hEvent );
	WaitForSingleObject ( hEvent, dwTimeOutDelay );
}
