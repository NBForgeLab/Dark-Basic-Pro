#include "IPC.h"
#include <cstdio>
#include <cstring>
#include <span>
#include <algorithm>
#include <cstdint>

cIPC::~cIPC ( )
{
	if ( m_lpMappedViewOfFile ) UnmapViewOfFile ( m_lpMappedViewOfFile );
	if ( m_hFileMap           ) CloseHandle     ( m_hFileMap           );
	if ( m_hDataMutex         ) CloseHandle     ( m_hDataMutex         );
	if ( m_hDataEvent         ) CloseHandle     ( m_hDataEvent         );

	m_lpMappedViewOfFile = nullptr;
	m_hFileMap           = nullptr;
	m_hDataMutex         = nullptr;
	m_hDataEvent         = nullptr;
	m_lpMem              = nullptr;
}

cIPC::cIPC ( LPCSTR SharedName, DWORD Size, BOOL bHandlesInheritable ) 
	: m_nSize ( Size ), m_bHandlesInheritable ( bHandlesInheritable )
{
	m_hDataMutex			= nullptr;
	m_hDataEvent			= nullptr;
	m_lpMem					= nullptr;
	m_hFileMap				= nullptr;
	m_lpMappedViewOfFile	= nullptr;
	m_ipcd                  = nullptr;

	if ( SharedName && SharedName[0] )
	{
		snprintf ( m_szFileMapName, sizeof(m_szFileMapName), "%s", SharedName );
		snprintf ( m_szMutexName,   sizeof(m_szMutexName),   "MUTEX_%s", SharedName );
		snprintf ( m_szEventName,   sizeof(m_szEventName),   "EVENT_%s", SharedName );
	}
	else
	{
		m_szFileMapName[0] = 0;
		m_szMutexName[0] = 0;
		m_szEventName[0] = 0;
		return;
	}

	// open or create file map
	m_hFileMap = OpenFileMappingA ( FILE_MAP_ALL_ACCESS, m_bHandlesInheritable, m_szFileMapName );
	if ( m_hFileMap == nullptr )
	{
		DWORD dwAccessFlags = PAGE_READWRITE | SEC_COMMIT;
		m_hFileMap = CreateFileMappingA ( INVALID_HANDLE_VALUE, nullptr, dwAccessFlags, 0, Size + sizeof ( tInterData ), m_szFileMapName );
		LastError = ::GetLastError ( );
		if ( m_hFileMap == nullptr )
			return;
	}

	// map view of file
	m_lpMappedViewOfFile = MapViewOfFile ( m_hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0 );
	if ( m_lpMappedViewOfFile == nullptr )
	{
		CloseHandle ( m_hFileMap );
		m_hFileMap = nullptr;
		return;
	}

	// Query actual mapped capacity if opened existing
	if ( m_nSize == 0 )
	{
		MEMORY_BASIC_INFORMATION mbi{};
		if ( VirtualQuery ( m_lpMappedViewOfFile, &mbi, sizeof(mbi) ) == sizeof(mbi) )
		{
			if ( mbi.RegionSize > sizeof(tInterData) )
				m_nSize = static_cast<DWORD>( mbi.RegionSize - sizeof(tInterData) );
		}
	}

	// initialize internal data structure
	m_ipcd = static_cast<tInterData*>( m_lpMappedViewOfFile );

	// initialize buffer exchange memory
	m_lpMem = static_cast<char*>( m_lpMappedViewOfFile ) + sizeof ( tInterData );

	// try to open mutex
	m_hDataMutex = OpenMutexA ( MUTEX_ALL_ACCESS, m_bHandlesInheritable, m_szMutexName );
	if ( m_hDataMutex == nullptr )
	{
		m_hDataMutex = CreateMutexA ( nullptr, FALSE, m_szMutexName );
		if ( m_hDataMutex == nullptr )
		{
			UnmapViewOfFile   ( m_lpMappedViewOfFile );
			CloseHandle       ( m_hFileMap );
			m_lpMappedViewOfFile = nullptr;
			m_hFileMap           = nullptr;
			return;
		}
	}

	// try to open event
	m_hDataEvent = OpenEventA ( EVENT_ALL_ACCESS, FALSE, m_szEventName );
	if ( m_hDataEvent == nullptr )
	{
		m_hDataEvent = CreateEventA ( nullptr, TRUE, FALSE, m_szEventName );
		if ( m_hDataEvent == nullptr )
		{
			CloseHandle       ( m_hDataMutex );
			UnmapViewOfFile   ( m_lpMappedViewOfFile );
			CloseHandle       ( m_hFileMap );
			m_hDataMutex         = nullptr;
			m_lpMappedViewOfFile = nullptr;
			m_hFileMap           = nullptr;
			return;
		}
	}

	// success
	LastError = ERROR_SUCCESS;
}

void cIPC::ReceiveBuffer ( LPVOID Buffer, DWORD dwOffset, DWORD Size )
{
	if ( !Buffer || Size == 0 || !m_lpMem || !m_hDataMutex )
	{
		if ( Buffer && Size > 0 )
			memset ( Buffer, 0, Size );
		return;
	}

	// Strict mathematical bounds checking with 64-bit overflow prevention
	const uint64_t reqEnd = static_cast<uint64_t>(dwOffset) + static_cast<uint64_t>(Size);
	if ( m_nSize == 0 || reqEnd > static_cast<uint64_t>(m_nSize) )
	{
		char szDiag[256];
		snprintf(szDiag, sizeof(szDiag), "[Enhancements IPC] ReceiveBuffer out of bounds in '%s': offset=%lu, size=%lu, capacity=%lu\n",
			m_szFileMapName, dwOffset, Size, m_nSize);
		OutputDebugStringA(szDiag);
		memset ( Buffer, 0, Size );
		return;
	}

	DWORD dwWait = WaitForSingleObject ( m_hDataMutex, 1000 );
	if ( dwWait != WAIT_OBJECT_0 && dwWait != WAIT_ABANDONED )
	{
		char szDiag[256];
		snprintf(szDiag, sizeof(szDiag), "[Enhancements IPC] Mutex timeout on ReceiveBuffer for '%s'\n", m_szMutexName);
		OutputDebugStringA(szDiag);
		memset ( Buffer, 0, Size );
		return;
	}

	// Modern C++20 span copy
	std::span<const std::byte> sourceSpan (
		reinterpret_cast<const std::byte*>(m_lpMem) + dwOffset,
		Size
	);
	std::span<std::byte> destSpan (
		reinterpret_cast<std::byte*>(Buffer),
		Size
	);
	std::ranges::copy(sourceSpan, destSpan.begin());

	ReleaseMutex ( m_hDataMutex );
}

void cIPC::SendBuffer ( LPCVOID Buffer, DWORD dwOffset, DWORD Size )
{
	if ( !Buffer || Size == 0 || !m_lpMem || !m_hDataMutex )
		return;

	// Strict mathematical bounds checking with 64-bit overflow prevention
	const uint64_t reqEnd = static_cast<uint64_t>(dwOffset) + static_cast<uint64_t>(Size);
	if ( m_nSize == 0 || reqEnd > static_cast<uint64_t>(m_nSize) )
	{
		char szDiag[256];
		snprintf(szDiag, sizeof(szDiag), "[Enhancements IPC] SendBuffer out of bounds in '%s': offset=%lu, size=%lu, capacity=%lu\n",
			m_szFileMapName, dwOffset, Size, m_nSize);
		OutputDebugStringA(szDiag);
		return;
	}

	DWORD dwWait = WaitForSingleObject ( m_hDataMutex, 1000 );
	if ( dwWait != WAIT_OBJECT_0 && dwWait != WAIT_ABANDONED )
	{
		char szDiag[256];
		snprintf(szDiag, sizeof(szDiag), "[Enhancements IPC] Mutex timeout on SendBuffer for '%s'\n", m_szMutexName);
		OutputDebugStringA(szDiag);
		return;
	}

	// Modern C++20 span copy
	std::span<const std::byte> sourceSpan (
		reinterpret_cast<const std::byte*>(Buffer),
		Size
	);
	std::span<std::byte> destSpan (
		reinterpret_cast<std::byte*>(m_lpMem) + dwOffset,
		Size
	);
	std::ranges::copy(sourceSpan, destSpan.begin());

	ReleaseMutex ( m_hDataMutex );
}
