
////////////////////////////////////////////////////////////////////
// INFORMATION /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

/*
	CORE SET UP COMMANDS
*/

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

// SOUND DLL
// LPDIRECTSOUND8 GetSoundInterface ( void )

/*
*/

////////////////////////////////////////////////////////////////////
// DEFINES AND INCLUDES ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

//#define INITGUID

//#define DARKSDK	__declspec ( dllexport )
#define DARKSDK	
#define _CRT_SECURE_NO_WARNINGS
#define _USING_V110_SDK71_

#include "stdafx.h"

#include <mmsystem.h>
#include <mmreg.h>
#include <dsound.h>
#include "core.h"
#include <stdio.h>

extern "C" FILE* GG_fopen( const char* filename, const char* mode )
{
	return fopen( filename, mode );
}

extern "C" HANDLE GG_CreateFile( LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile )
{
	return CreateFileA( lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
}

extern "C" HANDLE GG_CreateFileW( LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile )
{
	return CreateFileW( lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile );
}

#include ".\globstruct.h"
#include ".\..\..\Shared\Error\CError.h"

//#define SAFE_DELETE( p )		{ if ( p ) { delete ( p );       ( p ) = NULL; } }
//#define SAFE_RELEASE( p )		{ if ( p ) { ( p )->Release ( ); ( p ) = NULL; } }
//#define SAFE_DELETE_ARRAY( p )	{ if ( p ) { delete [ ] ( p );   ( p ) = NULL; } }

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// GLOBALS /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

GlobStruct*				g_pGlob = NULL;
//LPDIRECTSOUND8			g_pSound  = NULL;
char					g_szErrorList [ 256 ] [ 256 ];
bool					g_bErrorFile = false;

GetSoundPFN				g_pGetSound       = NULL;
GetSoundBufferPFN		g_pGetSoundBuffer = NULL;

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// FUNCTIONS ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

DARKSDK void ReceiveCoreDataPtr ( LPVOID pCore );
DARKSDK int  GetAssociatedDLLs  ( void );
DARKSDK void Destructor         ( void );

//extern void VoiceSetup	          ( void );
extern void HardDriveSetup        ( void );
		void LoadSystemDLL        ( void );
		void LoadSoundDLL         ( void );
		void SetupErrorCodes      ( void );
		void SetupFileBlocks      ( void );
		void DestroyFileBlocks    ( void );

void ReceiveCoreDataPtr ( LPVOID pCore )
{
	g_pGlob = ( GlobStruct* ) pCore;

	if ( !g_pGlob )
	{
		Error ( 1 );
		return;
	}

	SetupErrorCodes ( );
	LoadSoundDLL    ( );
}

int GetAssociatedDLLs ( void )
{
	return 2;
}

void Destructor ( void )
{
}

char* ENHANCEMENTSSetupString ( const char* szInput )
{
	if ( !szInput || !IsReadablePointer(reinterpret_cast<DWORD_PTR>(szInput)) )
		return nullptr;

	if ( !g_pGlob || !g_pGlob->CreateDeleteString )
		return nullptr;

	char* pReturn = nullptr;
	DWORD dwSize  = static_cast<DWORD>( strlen ( szInput ) );
	g_pGlob->CreateDeleteString((DWORD_PTR*)&pReturn, dwSize + 1 );
	if ( !pReturn )
	{
		Error ( 2 );
		return nullptr;
	}
	memcpy ( pReturn, szInput, dwSize );
	pReturn [ dwSize ] = 0;
	return pReturn;
}

char* SetupString ( const char* szInput )
{
	return ENHANCEMENTSSetupString( szInput );
}

void LoadSoundDLL ( void )
{
}

void SetupErrorCodes ( void )
{
	// clear out the error table
	memset ( g_szErrorList, 0, sizeof ( g_szErrorList ) );
	g_bErrorFile = false;
}

void Error ( int iID )
{
	/*
	if ( g_bErrorFile )
	{
		MessageBox ( NULL, g_szErrorList [ iID ], g_szErrorList [ 0 ], MB_ICONERROR | MB_OK );
	}
	else
	{
		char szNum [ 3 ];

		//itoa ( iID, szNum, 10 );
		_itoa ( iID, szNum, 10 );

		MessageBox ( NULL, szNum, "Enhancement Runtime Error", MB_ICONERROR | MB_OK );
	}
	*/
	char szNum[512];
	sprintf ( szNum, "Enhancement Runtime Error %d", iID );
	RunTimeError ( 0, szNum );
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
