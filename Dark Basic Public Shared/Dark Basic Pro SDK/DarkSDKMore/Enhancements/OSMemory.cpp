#include "stdafx.h"
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <climits>

#undef DARKSDK
#define DARKSDK __declspec(dllexport)

DARKSDK int GetInstalledMemory   ( int iReturn );
DARKSDK int GetMemoryAvailable   ( int iReturn );
DARKSDK int GetMemoryPercentUsed ( void );
DARKSDK int GetMemoryPercentFree ( void );

DARKSDK int GetInstalledMemory ( int iReturn )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	if ( iReturn == 0 )
	{
		const uint64_t kb = (memStatus.ullTotalPhys + 1023) / 1024;
		return static_cast<int>( std::min<uint64_t>( kb, static_cast<uint64_t>(INT_MAX) ) );
	}
	if ( iReturn == 1 )
	{
		const uint64_t mb = (memStatus.ullTotalPhys + (1024 * 1024 - 1)) / (1024 * 1024);
		return static_cast<int>( std::min<uint64_t>( mb, static_cast<uint64_t>(INT_MAX) ) );
	}
	if ( iReturn == 2 )
	{
		const uint64_t gb = (memStatus.ullTotalPhys + (1024ULL * 1024 * 1024 - 1)) / (1024ULL * 1024 * 1024);
		return static_cast<int>( std::min<uint64_t>( gb, static_cast<uint64_t>(INT_MAX) ) );
	}

	return 0;
}

DARKSDK int GetMemoryAvailable ( int iReturn )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	if ( iReturn == 0 )
	{
		const uint64_t kb = (memStatus.ullAvailPhys + 1023) / 1024;
		return static_cast<int>( std::min<uint64_t>( kb, static_cast<uint64_t>(INT_MAX) ) );
	}
	if ( iReturn == 1 )
	{
		const uint64_t mb = (memStatus.ullAvailPhys + (1024 * 1024 - 1)) / (1024 * 1024);
		return static_cast<int>( std::min<uint64_t>( mb, static_cast<uint64_t>(INT_MAX) ) );
	}
	if ( iReturn == 2 )
	{
		const uint64_t gb = (memStatus.ullAvailPhys + (1024ULL * 1024 * 1024 - 1)) / (1024ULL * 1024 * 1024);
		return static_cast<int>( std::min<uint64_t>( gb, static_cast<uint64_t>(INT_MAX) ) );
	}

	return 0;
}

DARKSDK int GetMemoryPercentUsed ( void )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	return static_cast<int>( std::clamp<DWORD>( memStatus.dwMemoryLoad, 0, 100 ) );
}

DARKSDK int GetMemoryPercentFree ( void )
{
	MEMORYSTATUSEX memStatus{};
	memStatus.dwLength = sizeof(memStatus);
	if ( !::GlobalMemoryStatusEx ( &memStatus ) )
		return 0;

	const int used = static_cast<int>( std::clamp<DWORD>( memStatus.dwMemoryLoad, 0, 100 ) );
	return 100 - used;
}
