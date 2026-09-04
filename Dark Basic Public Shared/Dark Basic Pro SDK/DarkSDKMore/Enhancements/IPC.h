#ifndef INTERPROCESSCOMMUNICATION_H__
#define INTERPROCESSCOMMUNICATION_H__

#include <windows.h>
#include <cstdint>

struct tInterData
{
	DWORD ServerPID;
	DWORD ClientPID;
};

class cIPC
{
	public:
		cIPC  ( LPCSTR SharedName, DWORD Size, BOOL bHandlesInheritable = FALSE );
		~cIPC ( );

		void   SendBuffer			( LPCVOID Buffer, DWORD dwOffset, DWORD Size );
		void   ReceiveBuffer		( LPVOID Buffer, DWORD dwOffset, DWORD Size );

		[[nodiscard]] DWORD GetSize() const noexcept { return m_nSize; }
		[[nodiscard]] bool IsValid() const noexcept { return m_lpMem != nullptr && m_hDataMutex != nullptr; }

	public:

		HANDLE 			m_hDataEvent = nullptr;

	private:

		DWORD  			LastError = ERROR_SUCCESS;
		bool   			m_bIsServer = false;
		tInterData*		m_ipcd = nullptr;
		DWORD			m_nSize = 0;
		DWORD			m_PID = 0;
		HANDLE 			m_hFileMap = nullptr;
		HANDLE 			m_hDataMutex = nullptr;
		LPVOID 			m_lpMem = nullptr;
		LPVOID 			m_lpMappedViewOfFile = nullptr;
		char			m_szFileMapName [ 80 ] = {};
		char  			m_szMutexName   [ 80 ] = {};
		char  			m_szEventName   [ 80 ] = {};
		BOOL   			m_bHandlesInheritable = FALSE;
};

#endif