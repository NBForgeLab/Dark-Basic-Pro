#include "stdafx.h"
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

#include "globstruct.h"
#include "CError.h"

// GLOBALS
GlobStruct*				g_pGlob = NULL;
char					g_szErrorList [ 256 ] [ 256 ];
bool					g_bErrorFile = false;

// FUNCTIONS
DARKSDK void ReceiveCoreDataPtr ( LPVOID pCore );
DARKSDK int  GetAssociatedDLLs  ( void );
DARKSDK void Destructor         ( void );

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
	SetupFileBlocks ( );
}

int GetAssociatedDLLs ( void )
{
	return 2;
}

void Destructor ( void )
{
	DestroyFileBlocks ( );
}

char* ENHANCEMENTSSetupString ( const char* szInput )
{
	if ( !szInput || !IsReadablePointer(reinterpret_cast<DWORD_PTR>(szInput)) )
		return nullptr;

	char* pReturn = nullptr;
	DWORD dwSize  = static_cast<DWORD>( strlen ( szInput ) );

	if ( g_pGlob && g_pGlob->CreateDeleteString )
	{
		g_pGlob->CreateDeleteString((DWORD_PTR*)&pReturn, dwSize + 1 );
		if ( !pReturn )
		{
			Error ( 2 );
			return nullptr;
		}
	}
	else
	{
		// Standalone / test fallback when running outside DBPro engine core
		pReturn = static_cast<char*>( HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize + 1) );
		if ( !pReturn )
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

void SetupErrorCodes ( void )
{
	// clear out the error table
	memset ( g_szErrorList, 0, sizeof ( g_szErrorList ) );
	g_bErrorFile = false;
}

void Error ( int iID )
{
	char szNum[512];
	sprintf ( szNum, "Enhancement Runtime Error %d", iID );
	RunTimeError ( 0, szNum );
}
