
////////////////////////////////////////////////////////////////////
// INFORMATION /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

/*
	CORE SET UP COMMANDS
*/

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// DEFINES AND INCLUDES ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

#define DARKSDK	__declspec ( dllexport )	

#include "stdafx.h"
#include "globstruct.h"
#include <mmsystem.h>
#include <dsound.h>

#define SAFE_DELETE( p )		{ if ( p ) { delete ( p );       ( p ) = NULL; } }
#define SAFE_RELEASE( p )		{ if ( p ) { ( p )->Release ( ); ( p ) = NULL; } }
#define SAFE_DELETE_ARRAY( p )	{ if ( p ) { delete [ ] ( p );   ( p ) = NULL; } }

// Special MARCO to exit a function if using FREE ENHANCEMENT PACK DLL
#ifdef FREEDLLVERSION
 #define FREEDLLEXITHERE return;
 #define FREEDLLEXITHERERET return 0;
#else
 #define FREEDLLEXITHERE // no exit command
 #define FREEDLLEXITHERERET // no exit command
#endif

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// GLOBALS /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

extern GlobStruct*			g_pGlob;

typedef LPDIRECTSOUND8         ( *GetSoundPFN       ) ( void );
typedef IDirectSound3DBuffer8* ( *GetSoundBufferPFN ) ( int  );
extern GetSoundPFN				g_pGetSound;
extern GetSoundBufferPFN		g_pGetSoundBuffer;

// GLOBAL

void Error ( int iID );
char* SetupString ( const char* szInput );

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

	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(reinterpret_cast<const void*>(ptr), &mbi, sizeof(mbi)) != sizeof(mbi))
		return 0;

	const uintptr_t pageEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
	if (ptr >= pageEnd)
		return 0;

	const size_t maxReadable = static_cast<size_t>(pageEnd - ptr);
	return strnlen(reinterpret_cast<const char*>(ptr), maxReadable);
}

static inline void SafeStrCopy(char* dest, DWORD_PTR src, size_t maxLen)
{
	if (!dest || maxLen == 0) return;
	dest[0] = '\0';

	const size_t len = SafeStrLen(src);
	if (len == 0) return;

	const size_t copyLen = (len < maxLen) ? len : (maxLen - 1);
	memcpy(dest, reinterpret_cast<const void*>(src), copyLen);
	dest[copyLen] = '\0';
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////