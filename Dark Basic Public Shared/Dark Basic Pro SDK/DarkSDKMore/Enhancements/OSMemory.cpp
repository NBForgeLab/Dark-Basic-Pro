
////////////////////////////////////////////////////////////////////
// INFORMATION /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

/*
	MEMORY COMMANDS
*/

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// DEFINES AND INCLUDES ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

//#define DARKSDK	__declspec ( dllexport )
#define DARKSDK	

#include "stdafx.h"
#include <math.h>
#include "Enchancements.h"

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

/*
	// export names for string table -

	GET INSTALLED MEMORY[%LL%?GetInstalledMemory@@YAHH@Z%
	GET MEMORY AVAILABLE[%LL%?GetMemoryAvailable@@YAHH@Z%
	GET MEMORY PERCENT USED[%L%?GetMemoryPercentUsed@@YAHXZ%
	GET MEMORY PERCENT FREE[%L%?GetMemoryPercentFree@@YAHXZ%
*/

int GetInstalledMemory ( int iReturn )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	if ( iReturn == 0 )
		return static_cast<int>( (memStatus.ullTotalPhys + 1023) / 1024 );
	if ( iReturn == 1 )
		return static_cast<int>( (memStatus.ullTotalPhys + (1024 * 1024 - 1)) / (1024 * 1024) );
	if ( iReturn == 2 )
		return static_cast<int>( (memStatus.ullTotalPhys + (1024ULL * 1024 * 1024 - 1)) / (1024ULL * 1024 * 1024) );

	return 0;
}

int GetMemoryAvailable ( int iReturn )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	if ( iReturn == 0 )
		return static_cast<int>( (memStatus.ullAvailPhys + 1023) / 1024 );
	if ( iReturn == 1 )
		return static_cast<int>( (memStatus.ullAvailPhys + (1024 * 1024 - 1)) / (1024 * 1024) );
	if ( iReturn == 2 )
		return static_cast<int>( (memStatus.ullAvailPhys + (1024ULL * 1024 * 1024 - 1)) / (1024ULL * 1024 * 1024) );

	return 0;
}

int GetMemoryPercentUsed ( void )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	return static_cast<int>( memStatus.dwMemoryLoad );
}

int GetMemoryPercentFree ( void )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	return 100 - static_cast<int>( memStatus.dwMemoryLoad );
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
