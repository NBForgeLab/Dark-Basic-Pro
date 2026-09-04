//
// DarkDLLCore
//

// Standard Includes
#define _CRT_SECURE_NO_DEPRECATE
#define WINVER 0x0601
#include "windows.h"
#include "math.h"
#include <cmath>
#include "time.h"
#include <cstdint>
#include <new>
#include <memory>
#include <cstdio>

// External Includes
#include "..\error\cerror.h"
#include "..\..\DarkSDK\Core\resource.h"
#include "..\..\..\DBPCompiler\Encryptor.h"
#include ".\..\Core\EncryptedFile.h"

// Include Memory Manager & Globals
#include ".\..\MemoryManager\DarkMemoryManager.h"
char g_MM_DLLName [ 256 ] = { "Core" };
char g_MM_FunctionName [ 256 ]= { "<none>" };

// Internal Includes
#include "DBDLLCore.h"
#include "TextLineSplitter.h"
#include "DBDLLDisplay.h"
#include "DBDLLCoreInternal.h"
#include "DBDLLArray.h"
#include "RenderList.h"

// Vectors and stack
#include <vector>
#include <stack>
#include <string>

DB_ENTER_NS()
	
//f64 CMathTable::g_sin[RESOLUTION];

DB_LEAVE_NS()

// External Pointers (for cores own error handling)
extern CRuntimeErrorHandler* g_pErrorHandler;

// Prototypes
LPSTR GetTypePatternCore ( LPSTR dwTypeName, DWORD dwTypeIndex );
DWORD GetNextSyncDelay();
DARKSDK DWORD ProcessMessagesOnly(void);
DARKSDK void CreateSingleString(DWORD_PTR* dwVariableSpaceAddress, DWORD dwSize);
DARKSDK void UpdateFilenameFromVirtualTable( LPSTR szStringAddress );
DARKSDK void Decrypt( LPSTR szStringAddress );
DARKSDK void Encrypt( LPSTR szStringAddress );
DARKSDK void ChangeMouse( DWORD dwCursorID );

static constexpr uint32_t kDBProStringMagic = 0xDB575247;

struct DBProStringHeader
{
	uint32_t magic;
	uint32_t size;
};

static inline char* AllocateDynamicString(size_t len)
{
	// Reject lengths that would wrap size_t in sizeof(header)+len+1 or that
	// exceed what the 32-bit header field (and DBPro DWORD lengths) can
	// represent; new[] would otherwise allocate a truncated block and the
	// subsequent copy would overrun it.
	if (len > 0xFFFFFFFEu - sizeof(DBProStringHeader))
		throw std::bad_alloc();
	const size_t totalBytes = sizeof(DBProStringHeader) + len + 1;
	char* pMem = new char[totalBytes];
	memset(pMem, 0, totalBytes);
	DBProStringHeader* pHeader = reinterpret_cast<DBProStringHeader*>(pMem);
	pHeader->magic = kDBProStringMagic;
	pHeader->size = static_cast<uint32_t>(len);
	return pMem + sizeof(DBProStringHeader);
}

static inline bool IsDynamicHeapString(const void* ptr)
{
	if (!ptr) return false;
	const char* strPtr = static_cast<const char*>(ptr);
	const void* headerPtr = strPtr - sizeof(DBProStringHeader);
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(headerPtr, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
	if (mbi.State != MEM_COMMIT || (mbi.Protect != PAGE_READWRITE && mbi.Protect != PAGE_WRITECOPY)) return false;
	const DBProStringHeader* pHeader = reinterpret_cast<const DBProStringHeader*>(headerPtr);
	return (pHeader->magic == kDBProStringMagic);
}

static inline void FreeDynamicString(void* ptr)
{
	if (!ptr) return;
	char* strPtr = static_cast<char*>(ptr);
	void* headerPtr = strPtr - sizeof(DBProStringHeader);
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(headerPtr, &mbi, sizeof(mbi)) == sizeof(mbi) &&
		mbi.State == MEM_COMMIT &&
		(mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_WRITECOPY))
	{
		DBProStringHeader* pHeader = reinterpret_cast<DBProStringHeader*>(headerPtr);
		if (pHeader->magic == kDBProStringMagic)
		{
			pHeader->magic = 0; // Clear magic to prevent double-free
			delete[] reinterpret_cast<char*>(pHeader);
		}
	}
}

// Array validation and architecture: array blocks carry a strongly-typed
// 56-byte header with a distinct 32-bit magic signature (kDBProArrayMagic).
#pragma pack(push, 4)
struct DBProArrayHeader {
	uint32_t dimensions[9]; // 36 bytes (offsets 0..35: 9 dimension bounds)
	uint32_t magic;         // 4 bytes  (offsets 36..39: kDBProArrayMagic = 0xDB574152)
	uint32_t size;          // 4 bytes  (offsets 40..43: dwSizeOfArray)
	uint32_t itemSize;      // 4 bytes  (offsets 44..47: dwSizeOfOneDataItem)
	uint32_t typeId;        // 4 bytes  (offsets 48..51: dwTypeValueOfOneDataItem)
	uint32_t cursor;        // 4 bytes  (offsets 52..55: internal cursor index)
};
#pragma pack(pop)
static_assert(sizeof(DBProArrayHeader) == 56, "DBProArrayHeader must be exactly 56 bytes");

static constexpr uint32_t kDBProArrayMagic = 0xDB574152;

static inline DBProArrayHeader* GetArrayHeader(DWORD_PTR dwArrayPtr) noexcept
{
	if (!dwArrayPtr) return nullptr;
	return reinterpret_cast<DBProArrayHeader*>(reinterpret_cast<char*>(dwArrayPtr) - sizeof(DBProArrayHeader));
}

static inline const DBProArrayHeader* GetArrayHeader(const void* dwArrayPtr) noexcept
{
	if (!dwArrayPtr) return nullptr;
	return reinterpret_cast<const DBProArrayHeader*>(static_cast<const char*>(dwArrayPtr) - sizeof(DBProArrayHeader));
}

static inline bool IsDynamicArrayMemory(const void* ptr) noexcept
{
	if (!ptr) return false;

	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(ptr, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
	if (mbi.State != MEM_COMMIT ||
		(mbi.Protect != PAGE_READWRITE && mbi.Protect != PAGE_WRITECOPY) ||
		mbi.Type != MEM_PRIVATE)
		return false;

	const auto* pHeader = reinterpret_cast<const DBProArrayHeader*>(ptr);
	return (pHeader->magic == kDBProArrayMagic);
}

static inline bool IsValidArrayHandle(DWORD_PTR dwArrayPtr) noexcept
{
	if (!dwArrayPtr) return false;
	const char* pHead = reinterpret_cast<const char*>(dwArrayPtr) - sizeof(DBProArrayHeader);
	return IsDynamicArrayMemory(pHead);
}

// Direct layout: element data starts immediately after the 56-byte header.
// Handle returned by CreateArray points to the first data element.
// Element n lives at handle + n * itemSize.
static inline char* GetArrayDataPtr(DBProArrayHeader* pHeader) noexcept
{
	return reinterpret_cast<char*>(pHeader) + sizeof(DBProArrayHeader);
}

static inline const char* GetArrayDataPtr(const DBProArrayHeader* pHeader) noexcept
{
	return reinterpret_cast<const char*>(pHeader) + sizeof(DBProArrayHeader);
}

static inline size_t GetArrayTotalAllocationBytes(uint32_t size, uint32_t itemSize) noexcept
{
	return sizeof(DBProArrayHeader) + static_cast<size_t>(size) * itemSize;
}

struct ScopedFileHandle
{
	HANDLE handle = INVALID_HANDLE_VALUE;
	ScopedFileHandle(HANDLE h) noexcept : handle(h) {}
	~ScopedFileHandle() noexcept { if (handle != INVALID_HANDLE_VALUE && handle != nullptr) CloseHandle(handle); }
	ScopedFileHandle(const ScopedFileHandle&) = delete;
	ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;
	ScopedFileHandle(ScopedFileHandle&& o) noexcept : handle(o.handle) { o.handle = INVALID_HANDLE_VALUE; }
	ScopedFileHandle& operator=(ScopedFileHandle&& o) noexcept {
		if (this != &o) {
			if (handle != INVALID_HANDLE_VALUE && handle != nullptr) CloseHandle(handle);
			handle = o.handle;
			o.handle = INVALID_HANDLE_VALUE;
		}
		return *this;
	}
	operator HANDLE() const noexcept { return handle; }
	bool IsValid() const noexcept { return handle != INVALID_HANDLE_VALUE && handle != nullptr; }
};

// Touch System works under XP and Win7 now
bool bDetectAndActivateWindows7TouchSystem = false;

// Global Core Vars
DBPRO_GLOBAL char*			g_pVarSpace					= nullptr;
DBPRO_GLOBAL char*			g_pDataSpace				= nullptr;

// Global Stack Store Vars
DBPRO_GLOBAL uint32_t		g_dwStackStoreSize			= 0;
DBPRO_GLOBAL uint32_t*		g_pStackStore				= nullptr;

// Global Performance Switches
DBPRO_GLOBAL bool			g_bAlwaysActiveOff			= false;
DBPRO_GLOBAL bool			g_bProcessorFriendly		= false; // leefix - 070403 - patch 4 slowdown bug
DBPRO_GLOBAL bool			g_bAlwaysActiveOneOff		= false; // leeadd - 201204 - flag to draw just once (typically when PAINT refreshes)
DBPRO_GLOBAL bool			g_bSyncOff					= true;
DBPRO_GLOBAL bool			g_bSceneStarted				= false;
DBPRO_GLOBAL bool			g_bCanRenderNow				= true;
DBPRO_GLOBAL uint32_t		g_dwSyncMask				= 0xFFFFFFFF;

// Global Sync Settings
DBPRO_GLOBAL uint32_t		g_dwManualSuperStepSetting	= 0;
DBPRO_GLOBAL uint32_t*      g_pdwSyncRateSetting        = nullptr;
DBPRO_GLOBAL uint32_t       g_dwSyncRateSettingSize     = 0;
DBPRO_GLOBAL uint32_t       g_dwSyncRateCurrent         = 0;

// Global Performance Flags used Internally
DBPRO_GLOBAL bool			g_bCascadeQuitFlag			= false;
DBPRO_GLOBAL bool			g_bUseExternalDisplayLayer	= false;
DBPRO_GLOBAL bool			g_bExternalDisplayActive	= false;
DBPRO_GLOBAL uint32_t		g_dwRecordedTimer			= 0;

// Global Error Handling and Pointers
DBPRO_GLOBAL char*			g_pCommandLineString		= nullptr;
DBPRO_GLOBAL LPVOID			g_ErrorHandler				= nullptr;
DBPRO_GLOBAL LPVOID			g_EscapeValue				= nullptr;
DBPRO_GLOBAL LPVOID			g_BreakOutPosition			= nullptr;

// U71 - added to store structure patterns in core (passed in from EXEBlock)
DBPRO_GLOBAL uint32_t		g_dwStructPatternQty		= 0;
DBPRO_GLOBAL char*			g_pStructPatternsPtr		= nullptr;

// Global Display Vars
DBPRO_GLOBAL HBITMAP		g_hDisplayBitmap			= nullptr;
DBPRO_GLOBAL HDC			g_hdcDisplay				= nullptr;
DBPRO_GLOBAL COLORREF		g_colFore					= RGB(255,255,255);
DBPRO_GLOBAL COLORREF		g_colBack					= RGB(0,0,0);
DBPRO_GLOBAL HBRUSH			g_hBrush					= nullptr;
DBPRO_GLOBAL uint32_t		g_dwScreenWidth				= 0;
DBPRO_GLOBAL uint32_t		g_dwScreenHeight			= 0;

DBPRO_GLOBAL HICON			g_hUseIcon					= nullptr;
DBPRO_GLOBAL HCURSOR		g_hUseArrow 				= nullptr;
DBPRO_GLOBAL HCURSOR		g_hUseHourglass 			= nullptr;
DBPRO_GLOBAL HCURSOR		g_hCustomCursors[30]        = {};
DBPRO_GLOBAL HCURSOR		g_ActiveCursor 				= nullptr;
DBPRO_GLOBAL HCURSOR		g_OldCursor 				= nullptr;

// Global Draw Order Flags
DBPRO_GLOBAL bool			g_bDrawAutoStuffFirst		= true;
DBPRO_GLOBAL bool			g_bDrawSpritesFirst			= false;
DBPRO_GLOBAL bool			g_bDrawEntirelyToCamera		= false;
DBPRO_GLOBAL bool			g_bDrawQuadInSync			= true; // but quad draw skipped if RenderQuad(0) returns zero

// Global Input Vars
DBPRO_GLOBAL uint32_t		g_dwWindowsTextEntrySize	= 0;
DBPRO_GLOBAL uint32_t		g_dwWindowsTextEntryPos		= 0;
DBPRO_GLOBAL uint8_t	g_cKeyPressed				= 0;
DBPRO_GLOBAL uint8_t	g_cInkeyCodeKey				= 0;
DBPRO_GLOBAL int			g_iEntryCursorState			= 0;
DBPRO_GLOBAL uint16_t		g_wWinKey					= 0;

// Global Data Vars
DBPRO_GLOBAL char*			g_pDataLabelStart			= nullptr;
DBPRO_GLOBAL char*			g_pDataLabelPtr				= nullptr;
DBPRO_GLOBAL char*			g_pDataLabelEnd				= nullptr;

// Global Security Data
DBPRO_GLOBAL int			g_iSecurityCode				= 0;

// Global Data Shared By DLLs
DBPRO_GLOBAL GlobStruct		g_Glob;
DBPRO_GLOBAL GlobStruct*	g_pGlob = &g_Glob;

// Small helper function to get more accurate timings
class AccurateTimer
{
private:
	UINT  Period;
	DWORD LastTime;

	// Disable copying
	AccurateTimer(const AccurateTimer&);
	AccurateTimer& operator=(const AccurateTimer&);
public:
	AccurateTimer() 
	{
		TIMECAPS caps;
		timeGetDevCaps(&caps, sizeof(caps));
		Period = caps.wPeriodMin;
		timeBeginPeriod(Period);
	}
	~AccurateTimer()
	{
		timeEndPeriod(Period);
	}
	DWORD Get()
	{
		LastTime = timeGetTime();
		return LastTime;
	}
	DWORD Last() const
	{
		return LastTime;
	}
};




//
// DLL Entry and Exit Function
//
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	// Memory Manager Identity
	strcpy ( g_MM_FunctionName, "DllMain" );
	#ifdef  __USE_MEMORY_MANAGER__
	mm_SnapShot();
	#endif

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:

			// Global Shared Data With Other DLLs
			ZeroMemory(&g_Glob, sizeof(GlobStruct));
			
			// Fill Glob With Required Default Values
			g_Glob.bWindowsMouseVisible		= true;

			// Fill Glob With Required Default Values
			g_Glob.dwForeColor				= -1;//(white)
			g_Glob.dwBackColor				= 0;

			// Global Control Defaults
			g_Glob.bEscapeKeyEnabled		= true;

			// Must always have at least a one to create a pass
			g_Glob.dwRedrawCount=1;
			g_Glob.dwRedrawPhase=0;

			// Assign Function Ptrs to Glob (for other DLLs to use from startup)
			g_Glob.CreateDeleteString = CreateSingleString;
			g_Glob.ProcessMessageFunction = ProcessMessagesOnly;
			g_Glob.PrintStringFunction = PrintString;
			g_Glob.UpdateFilenameFromVirtualTable = UpdateFilenameFromVirtualTable;
			g_Glob.Decrypt = Decrypt;
			g_Glob.Encrypt = Encrypt;
			g_Glob.ChangeMouseFunction = ChangeMouse;

			// leeprepare - 130104 - U6 GLOBSTRUCT DYNAMIC ARRAY
			//g_Glob.dwDynMemSize = (DWORD)(1*sizeof(DWORD));
			//g_Glob.pDynMemPtr = new char [ g_Glob.dwDynMemSize ];

			// aaron - 20120813 - sine table generation
			{
				db3::CMathTable genSinTable;
			}

			break;

		case DLL_PROCESS_DETACH:
		{
			// free dynamic array within globstruct
			//SAFE_DELETE ( g_Glob.pDynMemPtr );

			// done
			break;
		}
	}
	return TRUE;
}

// WINDOWS FRIENDLY FUNCTIONS

bool IsArraySingleDim ( DWORD_PTR dwArrayPtr )
{
	// Detect if array has single dimension only, false if multi or irregular array
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return false;
	const auto* pHeader = GetArrayHeader(dwArrayPtr);
	if (!pHeader || pHeader->magic != kDBProArrayMagic) return false;
	if (pHeader->itemSize > 1024000) return false;
	if (pHeader->dimensions[1] > 0) return false;
	return true;
}

DARKSDK DWORD ProcessMessagesOnly(void)
{
	// U76 - Windows 7 touch has no 'touch-release' via WM_MOUSE commands
	// so create an artificial persistence so MOUSECLICK(DX) can detect it
	if ( g_Glob.dwWindowsMouseLeftTouchPersist > 0 )
		if ( timeGetTime() > g_Glob.dwWindowsMouseLeftTouchPersist )
			g_Glob.dwWindowsMouseLeftTouchPersist=0;

	// Vars
	MSG msg;

	// Cascade means it will continue to quit (for rapid exit)
	if(g_bCascadeQuitFlag==true)
		return 1;

	// Message Pump
	while(TRUE)
	{
		// Standard Windows Processing
		if(PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			if(msg.message!=WM_QUIT)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				OutputDebugStringA("[DBDLLCore] WM_QUIT received -> setting g_bCascadeQuitFlag=true\n");
				g_bCascadeQuitFlag=true;
				return 1;
			}
		}
		else
		{
			// Processor Friendly
			if(g_bProcessorFriendly) Sleep(1);
			break;
		}
	}

	// Complete
	return 0;
}

DARKSDK void ConstantNonDisplayUpdate(void)
{
	// Update All NonVisuals (this gets called about six times because of processmessage calls..)
	if(g_Music_Update) g_Music_Update();
	if(g_Sound_Update) g_Sound_Update();
	if(g_Animation_UpdateAnims) g_Animation_UpdateAnims();
}

DARKSDK void ExternalDisplaySync ( int iSkipSyncRateCodeAkaFastSync )
{
	// Skip this phase if app has been shut down (always active off)
	if ( g_bAlwaysActiveOneOff ) 
		return;

	// V111 - 110608 - FASTSYNC should not use sync delay!
	if ( iSkipSyncRateCodeAkaFastSync==0 )
	{
		AccurateTimer Timer;

		// Skip refreshes causing even faster FPS rates!
		if(g_dwManualSuperStepSetting>0)
		{
			if(Timer.Get()-g_dwRecordedTimer<g_dwManualSuperStepSetting)
				return;
		}

		// Force FPS
		if(g_dwSyncRateSettingSize>0)
		{
			DWORD dwDifference = GetNextSyncDelay();
			while(Timer.Get()-g_dwRecordedTimer < dwDifference)
				if(ProcessMessagesOnly()==1) return;
		}
		else
		{
			// Need to ad least process these monitors
			ConstantNonDisplayUpdate();
		}

		// u74b7 - Only update the sync timer if not using fastsync
		// Record time of update
		g_dwRecordedTimer = Timer.Last();
	}

	// leefix - 260604 - u54 - in case input wants single data-grab functionality
	if(g_Input_ClearData) g_Input_ClearData();

	// 270515 - calls this to grab latest viewproj and record previous viewproj
	if ( iSkipSyncRateCodeAkaFastSync==0 )
		if ( g_Basic3D_UpdateViewProjForMotionBlur ) 
			g_Basic3D_UpdateViewProjForMotionBlur();

	// If using External Graphics API
	if(g_bExternalDisplayActive)
	{
		// leeadd - 160306 - u60 - camera zero off suspends normal operations
		bool bSuspendScreenOperations = false;
		if ( !(g_dwSyncMask & 1) )
		{
			// flag the suspension of regular screen zero activity
			bSuspendScreenOperations = true;
		}
		if ( bSuspendScreenOperations==false )
		{
			// If BSP used, compute responses
			if(g_World3D_End) g_World3D_End();
			if(g_Basic3D_AutomaticCollisionEnd) g_Basic3D_AutomaticCollisionEnd();

			// Draw Phase : Store backbuffer before any 3D is drawn..
			if(g_Sprites_SaveBack) g_Sprites_SaveBack();

			// Draw Phase : Draw Sprites Last
			if(g_bDrawSpritesFirst==false)
				if(g_Sprites_Update)
					g_Sprites_Update();
			
			// Ensures AutoStuff is first to be rendered
			if(g_bDrawAutoStuffFirst==true)
			{
				if(g_bSceneStarted)
				{
					// leeadd - 071108 - U71 - render quad if flagged
					if ( g_bDrawQuadInSync )
					{
						if(g_Basic3D_RenderQuad)
						{
							if(g_Basic3D_RenderQuad(0)==1)
							{
								g_Camera3D_RunCode ( 0 );
								g_Basic3D_RenderQuad(1);
							}
						}
					}
					g_GFX_End();
					if ( g_bCanRenderNow )
						g_GFX_Render();
				}
				g_bSceneStarted=true;
				g_GFX_Begin();

				// restore before-Sprites-drawn on new screen render
				if(g_Sprites_RestoreBack) g_Sprites_RestoreBack();
			}

			// Draw Phase : Draw Sprites First
			if(g_bDrawSpritesFirst==true)
				if(g_Sprites_Update)
					g_Sprites_Update();
		}

		// Draw Phase : Draw 3D Gemoetry
		if( g_Camera3D_StartSceneInt )
		{
			// Reset polycount and drawprim count
			if ( g_pGlob ) g_pGlob->dwNumberOfPolygonsDrawn=0;
			if ( g_pGlob ) g_pGlob->dwNumberOfPrimCalls=0;

			// Disable backdrop if camera zero disabled
			int iMode = 0; if ( bSuspendScreenOperations ) iMode = 1;

			// U75 - 080410 - ensure animation in scene only calculated once (on SYNC)
			if ( iSkipSyncRateCodeAkaFastSync==0 )
				if(g_Basic3D_UpdateAnimationCycle)
					g_Basic3D_UpdateAnimationCycle();

			// Draw all 3D - all cameras loop
			g_Camera3D_StartSceneInt ( iMode );
			do 
			{
				int iThisCamera = 1 + g_Camera3D_GetRenderCamera();
				if ( iThisCamera <= 32 )
				{
					// camera 0 - 31 can be masked
					DWORD dwCamBit = 1;
					if ( iThisCamera > 1 ) dwCamBit = dwCamBit << (DWORD)(iThisCamera-1);
					dwCamBit = dwCamBit & g_dwSyncMask;
					if ( dwCamBit==0 ) iThisCamera = 0;
					// 20120313 IanM - Removed incorrect 'prediction' code for next camera rendering.
				}
				if ( iThisCamera > 0 )
				{
					// Push all polygons for 3D components
					for(g_pGlob->dwRedrawPhase=0; g_pGlob->dwRedrawPhase<g_pGlob->dwRedrawCount; g_pGlob->dwRedrawPhase++)
					{
						// u74b8 - replace hard-coded calls with a dynamic list of function pointers
						ExecuteRenderList();
					}
				}
				// Next camera or finish..
			} while (g_Camera3D_FinishSceneEx(false)==0);

			// leeadd - 071107 - U71 - after 3D operations, direct whether SPRITES/2D/IMAGE
			// drawing is to take place by default (bitmap or camera zero)
			if ( g_bDrawEntirelyToCamera==true ) g_Camera3D_RunCode ( 1 );

		}
		if ( bSuspendScreenOperations==false )
		{
			// Ensures AutoStuff is last to be rendered
			if(g_bDrawAutoStuffFirst==false)
			{
				if(g_bSceneStarted)
				{
					// leeadd - 071108 - U71 - render quad if flagged
					if ( g_bDrawQuadInSync )
					{
						if(g_Basic3D_RenderQuad)
						{
							if(g_Basic3D_RenderQuad(0)==1)
							{
								g_Camera3D_RunCode ( 0 );
								g_Basic3D_RenderQuad(1);
							}
						}
					}
					g_GFX_End();
					if ( g_bCanRenderNow )
						g_GFX_Render();
				}
				g_bSceneStarted=true;
				g_GFX_Begin();

				// restore before-Sprites-drawn on new screen render
				if(g_Sprites_RestoreBack) g_Sprites_RestoreBack();
			}

			// If BSP used, set response check
			if(g_World3D_Start) g_World3D_Start();
			if(g_Basic3D_AutomaticCollisionStart) g_Basic3D_AutomaticCollisionStart();
		}
	}
}

DARKSDK void ExternalDisplayUpdate(void)
{
	// Call external sync if automatic
	if(g_bSyncOff) ExternalDisplaySync(0);
}

#ifdef DARKSDK_COMPILE
extern int g_iDarkGameSDKQuit;
#endif

/* Proved incompatible with Windows XP (user.dll)
LRESULT DecodeGesture(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// U76 - added but not used (yet)
	GESTUREINFO gi;  
	ZeroMemory(&gi, sizeof(GESTUREINFO));
	gi.cbSize = sizeof(GESTUREINFO);
	BOOL bResult  = GetGestureInfo((HGESTUREINFO)lParam, &gi);
	BOOL bHandled = FALSE;
	if ( bResult )
	{
		// now interpret the gesture
		switch (gi.dwID)
		{
		   case GID_ZOOM:
			   // Code for zooming goes here     
			   break;
		   case GID_PAN:
			   // Code for panning goes here
			   break;
		   case GID_ROTATE:
			   // Code for rotation goes here
			   break;
		   case GID_TWOFINGERTAP:
			   // Code for two-finger tap goes here
			   break;
		   case GID_PRESSANDTAP:
			   // Code for tap goes here
			   break;
		   default:
			   // A gesture was not recognized
			   break;
		}
	}
	else
	{
		DWORD dwErr = GetLastError();
		if (dwErr > 0)
		{
			//MessageBoxW(hWnd, L"Error!", L"Could not retrieve a GESTUREINFO structure.", MB_OK);
		}
	}
	if ( bHandled )
	{
		return 0;
	}
	else
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}
*/

LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message )
	{
		case WM_SETTEXT:
		{
			// leeadd - 130306 - igl - handle HWND message
			LPSTR pIncomingStr = (LPSTR)lParam;
			if ( g_Glob.hwndIGLoader==NULL )
			{
				if ( pIncomingStr )
				{
					if ( strlen ( pIncomingStr ) > 7 )
					{
						// only if IGL text message
						if ( strnicmp ( pIncomingStr, "PARENT|", 7  )==NULL )
						{
							// extract HWND from lParam
							char str [ 256 ];
							strcpy ( str, pIncomingStr + 7 );
							int num = atoi ( str );
							g_Glob.hwndIGLoader = (HWND)num;

							// not a title text change
							return 0;
						}
					}
				}
			}
			else
			{
				// messages can be sent from igloader, relay them to the igLoader DLL if present
				if ( hWnd==g_Glob.hWnd )
				{
					// from igLoader, if IGL DLL present, send this text to it
					if ( g_Glob.g_igLoader )
					{
						// send raw string to igloader dll for adding to callback list
						typedef void ( *IGL_AddStringToCallbackList ) ( LPSTR );
						IGL_AddStringToCallbackList AddToCBList;
						AddToCBList = ( IGL_AddStringToCallbackList ) GetProcAddress ( g_Glob.g_igLoader, "?IglAddSetTextString@@YAXPAD@Z" );
						if ( AddToCBList ) AddToCBList ( pIncomingStr );

						// no window text change
						return 0;
					}
				}
			}
		}

		case WM_ACTIVATE:
		{
			HWND hwndPrevious = (HWND) lParam;       // window handle
			if ( hwndPrevious==NULL || hwndPrevious==hWnd )
			{
				if(LOWORD(wParam)==WA_ACTIVE
				|| LOWORD(wParam)==WA_CLICKACTIVE)
				{
					// Used to signal a refresh
					///g_Glob.bInvalidFlag=true; 060215 - interferes with foreground enforcement (SteamUI)!

//					// leeremove - 180705 - caused huge delays if active messages got confused
//					// leeadd - 201204 - if active, enable sync
//					if ( g_bAlwaysActiveOff )
//					{
//						g_bAlwaysActiveOneOff = false;
//						g_bProcessorFriendly = false;
//					}
				}
				else
				{
//					// leeremove - 180705 - caused huge delays if active messages got confused
//					// leeadd - 201204 - if inactive, disable sync
//					if ( g_bAlwaysActiveOff )
//					{
//						g_bAlwaysActiveOneOff = true;
//						g_bProcessorFriendly = true;
//					}
				}
			}

			// 20/7/11 - Win7 - ensure we register for TOUCH over GESTURE (also allows LBUTTONDOWN to happen instantly!)
			if ( bDetectAndActivateWindows7TouchSystem==false )
			{
				bDetectAndActivateWindows7TouchSystem = true;
				OSVERSIONINFO osvi;
				BOOL bIsWindows7orLater;
				ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
				osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
				GetVersionEx(&osvi);
				bIsWindows7orLater = ( (osvi.dwMajorVersion > 6) || ( (osvi.dwMajorVersion == 6) && (osvi.dwMinorVersion >= 1) ));
				if ( bIsWindows7orLater==TRUE )
				{
					// must dynamically find the user32.dll function and call it IF Windows 7 (allows Windows XP to run)
					// RegisterTouchWindow(g_Glob.hWnd, 0);
					typedef UINT (CALLBACK* sRegisterTouchWindowFnc)(HWND,ULONG);
					HMODULE hWinUserDLL = LoadLibrary ( "user32.dll" );
					if ( hWinUserDLL )
					{
						sRegisterTouchWindowFnc pRegTouchWin = (sRegisterTouchWindowFnc) GetProcAddress ( hWinUserDLL, "RegisterTouchWindow" );
						if ( pRegTouchWin ) pRegTouchWin ( g_Glob.hWnd, 0 );
						FreeLibrary ( hWinUserDLL );
					}
				}
			}

			break;
		}

		case WM_CLOSE:
		{
			#ifdef DARKSDK_COMPILE
			g_iDarkGameSDKQuit = 1;
			#endif
			PostQuitMessage(0);
			return TRUE;
		}

		case WM_DESTROY:
		case WM_NCDESTROY:
		{
			#ifdef DARKSDK_COMPILE
			g_iDarkGameSDKQuit = 1;
			#endif
			PostQuitMessage(0);
			break;
		}

		case WM_ERASEBKGND:
			return TRUE;

		case WM_SIZE:
		case WM_SIZING:
		case WM_MOVE:
		case WM_MOVING:
		case WM_PAINT:
			{
				// 180214 - record new size in glob struct
				RECT rc;
				//if ( message==WM_SIZE )
				//{
				//	GetClientRect(hWnd, &rc);
				//	g_pGlob->dwWindowWidth = rc.right - rc.left;
				//	g_pGlob->dwWindowHeight = rc.bottom - rc.top;
				//}

				// GDI Paint
				PAINTSTRUCT ps;
				HDC hdcClient = BeginPaint(hWnd, &ps);
				if(hdcClient)
				{
					if(g_hdcDisplay)
					{
						GetClientRect(hWnd, &rc);
						HGDIOBJ hdcOld = SelectObject(g_hdcDisplay, g_hDisplayBitmap);
						BitBlt(hdcClient, rc.left, rc.top, rc.right, rc.bottom, g_hdcDisplay, 0, 0, SRCCOPY);
						SelectObject(g_hdcDisplay, hdcOld);
					}
					else
					{
						// 210203 - if array of protected boxes setup (from controls requiring primary surface)
						if ( g_pGlob->dwSafeRectMax>0 )
						{
							// Clear Device
							GetClientRect(hWnd, &rc);
							HBRUSH bGrey = GetSysColorBrush ( COLOR_3DFACE );
							HBRUSH bOld = (HBRUSH)SelectObject(hdcClient, bGrey ); 
							Rectangle(hdcClient, -5, -5, rc.right+5, rc.bottom+5);
							SelectObject(hdcClient, bOld ); 
						}
					}
					EndPaint(hWnd, &ps);
				}

				// Ensures rendered areas are retained (when moving window or menu refreshing)
				if ( g_Glob.dwAppDisplayModeUsing==1 )
				{
					// only dwDisplayMode=1 (window) should do this (otherwise render several times!!)
					if (g_GFX_End && g_GFX_Render && g_GFX_Begin)
					{
						// ensure refresh is not done in middle of draw-phase
						g_GFX_End(); g_GFX_Render(); g_GFX_Begin();
					}
				}
			}
			return TRUE;

		case WM_MOUSEMOVE:
			{
				// Get Client Raw Mouse Position
				g_Glob.iWindowsMouseX = LOWORD(lParam);  // horizontal position of cursor 
				g_Glob.iWindowsMouseY = HIWORD(lParam);  // vertical position of cursor 
				
				// Special Scale for When Windows Stretch Beyond Physical Size of Backbuffer
				RECT rc;
				GetClientRect(hWnd, &rc);
				float xRatio = (float)g_Glob.dwWindowWidth/(float)rc.right;
				float yRatio = (float)g_Glob.dwWindowHeight/(float)rc.bottom;
				g_Glob.iWindowsMouseX = (int)((float)g_Glob.iWindowsMouseX * xRatio);
				g_Glob.iWindowsMouseY = (int)((float)g_Glob.iWindowsMouseY * yRatio);

				// Restore cursor when move mouse
				if ( g_ActiveCursor != NULL ) SetCursor ( g_ActiveCursor );

			}
			break;

		case WM_LBUTTONDOWN:
			g_Glob.iWindowsMouseClick|=1;
			g_Glob.dwWindowsMouseLeftTouchPersist=timeGetTime()+250; // U76 - many cycles
			if ( GetFocus()!=hWnd ) 
			{
				SetFocus ( hWnd );
				// 060215 - puts this back in foreground
				//SetForegroundWindow ( hWnd );
			}
			break;

		case WM_RBUTTONDOWN:
			g_Glob.iWindowsMouseClick|=2;
			if ( GetFocus()!=hWnd ) SetFocus ( hWnd );
			break;

		// aaron - 20120811 - Potential issues when using xor depending on obscure and rare window interaction
		case WM_LBUTTONUP:
			g_Glob.iWindowsMouseClick &= ~1UL;
			//g_Glob.iWindowsMouseClick^=1;
			break;

		case WM_RBUTTONUP:
			g_Glob.iWindowsMouseClick &= ~2UL;
			//g_Glob.iWindowsMouseClick^=2;
			break;

//		case WM_GESTURE:
//			// U77 - Touch for Windows 7
//			// ensure tap = click (not mouse position)
//			return DecodeGesture(hWnd, message, wParam, lParam);

		case WM_SYSKEYDOWN:
			g_wWinKey = static_cast<WORD>( wParam ); // leefix - 240604 - u54 - for WAIT KEY bug F10
			break;

		case WM_KEYDOWN:
			g_wWinKey = static_cast<WORD>( wParam );
			if((int)wParam==VK_ESCAPE)
			{
				if(g_EscapeValue) *(DWORD*)g_EscapeValue=1;
				if(g_Glob.bEscapeKeyEnabled)
				{
					#ifdef DARKSDK_COMPILE
					g_iDarkGameSDKQuit = 1;
					#endif
					PostQuitMessage(0);
				}
			}
			return TRUE;

		case WM_SYSKEYUP:
			g_wWinKey=0; // leefix - 240604 - u54 - for WAIT KEY bug F10
			return TRUE;

		case WM_KEYUP:
			g_cInkeyCodeKey=0;
			g_wWinKey=0;
			return TRUE;

		case WM_CHAR:

			// If win string cleared externally (InputDLL)
			if(g_Glob.pWindowsTextEntry)
				if(g_Glob.pWindowsTextEntry[0]==0)
					g_dwWindowsTextEntryPos=0;

			// Key that was pressed
			g_cKeyPressed = static_cast<unsigned char>(wParam);
			g_cInkeyCodeKey = g_cKeyPressed;

			// Ensure string is always big enough
			if(g_Glob.pWindowsTextEntry == nullptr)
			{
				g_dwWindowsTextEntrySize = 32;
				g_Glob.pWindowsTextEntry = new char[g_dwWindowsTextEntrySize]();
				g_dwWindowsTextEntryPos = 0;
			}
			if(g_dwWindowsTextEntryPos > g_dwWindowsTextEntrySize - 4)
			{
				g_dwWindowsTextEntrySize = g_dwWindowsTextEntrySize * 2;
				char* pNewString = new char[g_dwWindowsTextEntrySize]();
				strcpy_s(pNewString, g_dwWindowsTextEntrySize, g_Glob.pWindowsTextEntry);
				delete[] g_Glob.pWindowsTextEntry;
				g_Glob.pWindowsTextEntry = pNewString;
			}

			// leeadd - 020605 - Add/Remove from entry$() string
			// leeafix - 110206 - created behaviour disaster in U59 - place this behaviour in ENTRY$(1) parameter of CINPUT.CPP
//			if ( g_cKeyPressed==8 )
//			{
//				// Remove character from entry
//				if ( g_dwWindowsTextEntryPos>0 )
//				{
//					g_dwWindowsTextEntryPos--;
//					g_Glob.pWindowsTextEntry[g_dwWindowsTextEntryPos]=0;
//				}
//			}
			// Add character to entry string
			g_Glob.pWindowsTextEntry[g_dwWindowsTextEntryPos]=g_cKeyPressed;
			g_dwWindowsTextEntryPos++;
			g_Glob.pWindowsTextEntry[g_dwWindowsTextEntryPos]=0;

			return TRUE;

		case WM_USER+1: // Show/Hide Cursor
			if(wParam==0) ShowCursor(FALSE);
			if(wParam==1) ShowCursor(TRUE);
			return TRUE;
	}
	
	// Default Action
	return DefWindowProc(hWnd, message, wParam, lParam);
}

DARKSDK void InternalClearWindowsEntry(void)
{
	if(g_Glob.pWindowsTextEntry)
	{
		strcpy(g_Glob.pWindowsTextEntry,"");
		g_dwWindowsTextEntryPos		= 0;
		g_cKeyPressed				= 0;
	}
}

DARKSDK DWORD InternalProcessMessages(void)
{
	DWORD dwResult = ProcessMessagesOnly();
	//if(g_bSyncOff) ConstantNonDisplayUpdate(); // leefix - 100605 - added 'g_bSyncOff' so ONLY SYNC does this when SYNC ON

	// mike - 101005 - not calling this and using fastsync results in problems for 3D sound
	// lee - 100208 - placing this here slows down tight loops and large programs by several MS (moved to FASTSYNC)
	// ConstantNonDisplayUpdate();


	if(g_bUseExternalDisplayLayer) ExternalDisplayUpdate();
	return dwResult;
}

DARKSDK DWORD ProcessMessages(DWORD dwPositionInMachineCode)
{
	// Process Messages from a program in debug mode
	DWORD dwReturnValue=0;

	// When breakout position filled, leave immediately
	if(g_BreakOutPosition)
	{
		// If Exit requested, store position before leave
		if(*(DWORD*)g_BreakOutPosition==1)
		{
			*(DWORD*)g_BreakOutPosition=dwPositionInMachineCode;
			return 1;
		}
	}

	// Process Internal Message Loop
	dwReturnValue = InternalProcessMessages();

	// Return Value
	return dwReturnValue;
}

DARKSDK DWORD ProcessMessages(void)
{
	// Process Messages from a program in fullspeed mode
	return InternalProcessMessages();
}

DARKSDK DWORD Quit(void)
{
	OutputDebugStringA("[DBDLLCore] Quit() called -> setting g_bCascadeQuitFlag=true\n");
	// Initate Cascade Quit
	g_bCascadeQuitFlag=true;
	if(g_EscapeValue) *(DWORD*)g_EscapeValue=2;

	// Process any other tasks during Final QUIT
	if(g_Glob.pExitPromptString)
	{
		// Produce an Exit Window with Strings
		MessageBox(NULL, g_Glob.pExitPromptString, g_Glob.pExitPromptString2, MB_OK);

		// Free Strings via dynamic string manager
		if(IsDynamicHeapString(g_Glob.pExitPromptString))
			FreeDynamicString(g_Glob.pExitPromptString);
		g_Glob.pExitPromptString = nullptr;

		if(g_Glob.pExitPromptString2)
		{
			if(IsDynamicHeapString(g_Glob.pExitPromptString2))
				FreeDynamicString(g_Glob.pExitPromptString2);
			g_Glob.pExitPromptString2 = nullptr;
		}
	}

	// Complete
	return 0;
}

DARKSDK void StackSnapshotStore(DWORD dwStackPositionNow)
{
	// No Stack save - data would be useless when new m/c executed
}

DARKSDK DWORD StackSnapshotRestore(void)
{
	// No Stack save - data would be useless when new m/c executed
	return 0;
}

// aaron - 20120811 - more flexible memory management routine
// p: in pointer (nullptr: alloc; else: realloc)
// n: size to have (0: free)
DB_EXPORT void *ManageMemory(void *p, size_t n) {
	void *q;

	if (n) {
		if (p)
			q = realloc(p, n);
		else
			q = malloc(n);
	} else {
		if (p)
			free(p);

		return nullptr;
	}

	return q;
}

DARKSDK int TestMemory ( int iSizeInBytes )
{
	if (iSizeInBytes <= 0) return 0;
	try
	{
		char* pMem = new char[iSizeInBytes];
		if ( pMem )
		{
			// can still reserve memory chunk
			delete[] pMem;
			return 1;
		}
		else
			return 0;
	}
	catch(...)
	{
		return 0;
	}
}

DARKSDK void CreateSingleString(DWORD_PTR* dwVariableSpaceAddress, DWORD dwSize)
{
	if (!dwVariableSpaceAddress) return;
	if (dwSize > 0)
	{
		char* pNew = AllocateDynamicString(dwSize);
		*dwVariableSpaceAddress = reinterpret_cast<DWORD_PTR>(pNew);
	}
	else
	{
		// Delete a core string
		LPSTR strPtr = (LPSTR)*dwVariableSpaceAddress;
		if ( strPtr )
		{
			FreeDynamicString( strPtr );
		}
		*dwVariableSpaceAddress = 0;
	}
}

DARKSDK void Break(void)
{
	// Set Escape Value to Break Into Debugger
	if(g_EscapeValue) *(DWORD*)g_EscapeValue=1;
}

DARKSDK LRESULT SendDataToDebugger(int iType, LPSTR pData, DWORD dwDataSize)
{
	LRESULT lResult=0;

	// Create Virtual File for Transfer
	HANDLE hFileMap = CreateFileMapping((HANDLE)0xFFFFFFFF,NULL,PAGE_READWRITE,0,dwDataSize,"DBPROEDITORMESSAGE");
	if(hFileMap)
	{
		LPVOID lpVoid = MapViewOfFile(hFileMap,FILE_MAP_WRITE,0,0,dwDataSize+4);
		if(lpVoid)
		{
			// Copy to Virtual File
			*(DWORD*)lpVoid = dwDataSize;
			memcpy((LPSTR)lpVoid+4, pData, dwDataSize);

			// Find Debugger to send to
			HWND hWnd = FindWindow(NULL,"DBProDebugger");
			if(hWnd)
			{
				// Found - transmit
				lResult = SendMessage(hWnd, WM_USER+10, iType, 0);
			}

			// Release virtual file
			UnmapViewOfFile(lpVoid);
		}
		CloseHandle(hFileMap);
	}

	// May have result
	return lResult;
}

DARKSDK void BreakS(DWORD_PTR pString)
{
	// Send String to CLI Debug Console
	LPSTR lpReturnError = new char[1024];
	const char* szSource = (const char*)pString;
	if (szSource)
		snprintf(lpReturnError, 1024, "%s", szSource);
	else
		lpReturnError[0] = 0;
	SendDataToDebugger(31, lpReturnError, static_cast<DWORD>(strlen(lpReturnError)));
	delete[] lpReturnError;
	lpReturnError=NULL;

	// Set Escape Value to Break Into Debugger
	if(g_EscapeValue) *(DWORD*)g_EscapeValue=1;
}

DARKSDK bool DoesFileExist(LPSTR Filename)
{
	// success or failure
	bool bSuccess = true;

	// open File To See If Exist
	HANDLE hfile = CreateFile(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hfile==INVALID_HANDLE_VALUE)
		bSuccess=false;
	else
		CloseHandle(hfile);

	// return result
	return bSuccess;
}

DARKSDK void UpdateFilenameFromVirtualTable( LPSTR szStringAddress )
{

	// String is input with external filename
	if ( !szStringAddress )
		return;

	// If Virtual Table area available
	if(g_Glob.pEXEUnpackDirectory==NULL)
		return;

	// Construct path to virtual file (if it is there or not) leefix - 200704 - can get very big!
	LPSTR pFilename = new char[_MAX_PATH*3];
	strcpy(pFilename, g_Glob.pEXEUnpackDirectory);
	strcat(pFilename, "\\media\\");
	strcat(pFilename, szStringAddress);

	// If File exists, use that instead of external file
	if(DoesFileExist(pFilename)==true)
	{
		// Virtual Table File better than local external file
		strcpy(szStringAddress, pFilename);
	}

	// Free usages
	delete[] pFilename;

}

DARKSDK LPSTR ReadFileData(LPSTR FilenameString, DWORD* dwDataSize)
{
	// Read File Data
	HANDLE hreadfile = CreateFile(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hreadfile==INVALID_HANDLE_VALUE)
		return NULL;

	// Read readout file into memory
	DWORD bytesread=0;
	DWORD filebuffersize = GetFileSize(hreadfile, NULL);	
	LPSTR filebuffer = (char*)GlobalAlloc(GMEM_FIXED, filebuffersize);
	ReadFile(hreadfile, filebuffer, filebuffersize, &bytesread, NULL); 
	CloseHandle(hreadfile);

	*dwDataSize = filebuffersize;
	return filebuffer;
}

DARKSDK void WriteFileData(LPSTR pFilename, LPSTR pData, DWORD dwDataSize)
{

	// Delete existing file
	DeleteFile(pFilename);

	// Write New File with new data
	DWORD byteswritten=0;
	HANDLE hfile = CreateFile(pFilename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hfile!=INVALID_HANDLE_VALUE)
	{
		WriteFile(hfile, pData, dwDataSize, &byteswritten, NULL); 
		CloseHandle(hfile);
	}
}

DARKSDK void EncryptDecrypt( LPSTR szStringAddress, bool bEncryptIfTrue, bool bDoNotUseTempFolder )
{
	// Dave - for safety
	g_Glob.dwEncryptionUniqueKey = 0;

	// String is input with external filename
	if ( !szStringAddress )
		return;

	// If re-encrypting, and filename is in temp, just delete it and finish
	LPSTR pLocalTRemp = g_Glob.pEXEUnpackDirectory;
	if ( bDoNotUseTempFolder==false && bEncryptIfTrue==true )
	{
		if ( strnicmp ( szStringAddress, pLocalTRemp, strlen(pLocalTRemp) )==NULL )
		{
			// Delete temp decrypted file
			if(DoesFileExist(szStringAddress)) DeleteFile(szStringAddress);

			// Finish early
			return;
		}
	}

	// length of six... e.g. "_e_Z.x" is the min size file (1 letter name)
	if ( strlen(szStringAddress) < 6 ) return;

	char checkForEncryptName[_MAX_PATH];
	char keyName[10] = "7yFkC3Oa";
		
	strcpy( checkForEncryptName , szStringAddress );

	// a file that needs decrypting will start with "_e_"
	bool okayToProceed = false;
	if ( ( checkForEncryptName[0] == '_' && checkForEncryptName[1] == 'e' && checkForEncryptName[2] == '_' ) || strstr ( checkForEncryptName , "\\_e_" )  )
		okayToProceed = true;
	else
	{
		if ( bEncryptIfTrue )
		{
			if ( ( checkForEncryptName[0] == '_' && checkForEncryptName[1] == 'w' && checkForEncryptName[2] == '_' ) || strstr ( checkForEncryptName , "\\_w_" )  )
			{
				okayToProceed = true;
				strcpy ( keyName , "iG72VL8q" );
			}
		}
	}

	if ( okayToProceed )
	{
		// Dave - set key to 1 to show this file is encrypted
		g_Glob.dwEncryptionUniqueKey = 1;

		char stringToMakeKey[_MAX_PATH];
		strcpy_s ( stringToMakeKey, sizeof(stringToMakeKey), "" );
		size_t tLength = 0;
		for ( size_t c = 0 ; c < strlen(checkForEncryptName) ; c++ )
		{
			if ( checkForEncryptName[c] != '.' )
			{
				stringToMakeKey[tLength++] = checkForEncryptName[c];
				stringToMakeKey[tLength] = 0;
			}
		}

		int len = static_cast<int>(strlen(stringToMakeKey))-1;
		if ( len <= 0 ) return;

		for ( int c = len; c >= 0 && c > len-9 ; c-- )
		{
			keyName[len - c] = stringToMakeKey[c];
			if ( keyName[len - c] >= 65 && keyName[len - c] < 90 ) keyName[len - c]++;
			else if ( keyName[len - c] >= 97 && keyName[len - c] < 122 ) keyName[len - c]++;
			else if ( keyName[len - c] == 90 ) keyName[len - c] = 65;
			else if ( keyName[len - c] == 122 ) keyName[len - c] = 97;
		}
	}
	else
		return;

	// If exe not setup for encryption, dont use it
	if(g_Glob.dwEncryptionUniqueKey==0)
		return;

	// Check if file exists
	LPSTR pFilename = new char[_MAX_PATH];
	strcpy(pFilename, szStringAddress);
	if(DoesFileExist(pFilename))
	{
		// Open File Data
		DWORD dwDataSize = 0;
		LPSTR pData = ReadFileData(pFilename, &dwDataSize);

		// Decrypt File Data
		CEncryptor Encryptor(0);
		Encryptor.SetUniqueKey(1);
		Encryptor.SetUniqueKeyName(keyName);
		Encryptor.EncryptFileData(pData, dwDataSize, bEncryptIfTrue);

		// temp or real
		if ( bDoNotUseTempFolder==false )
		{
			// get file ext
			char pExt[32];
			strcpy ( pExt, "" );
			for ( int n=static_cast<int>(strlen(pFilename))-1; n>0; n-- )
			{
				if ( pFilename[n]=='.' )
				{
					strcpy ( pExt, pFilename+n );
					break;
				}
			}

			// work out write file to local write-safe area
			strcpy ( pFilename, pLocalTRemp );
			strcat ( pFilename, "\\decrypted" );
			strcat ( pFilename, pExt );

			// if decrypting file, write decrypted file to safe-write area
			// if encrypting, simply delete the temp writted file in safe-write area
			if ( bEncryptIfTrue==false )
			{
				// Write New File
				WriteFileData(pFilename, pData, dwDataSize);

				// And point module using this function to the new filename
				strcpy ( szStringAddress, pFilename );
			}
		}
		else
		{
			// Write New File and overrite what is there
			WriteFileData(pFilename, pData, dwDataSize);
		}

		// Free FileData
		if(pData)
		{
			GlobalFree(pData);
			pData=NULL;
		}
	}

	// Free usages
	delete[] pFilename;

	//Dave - set key back to 0 after
	g_Glob.dwEncryptionUniqueKey = 0;
}

DARKSDK void Decrypt( LPSTR szStringAddress )
{

	// lee - 230306 - u6b4 - only encrypt/decrypt if from DBPDATA temp folder (should not touch local files!)
	// Dave - 28/03/2014 - commented this out because we will check if a file needs decrypting from now on
	//if ( strnicmp ( szStringAddress, g_Glob.pEXEUnpackDirectory, strlen(g_Glob.pEXEUnpackDirectory) )==NULL )
		EncryptDecrypt ( szStringAddress, false, false );
}

DARKSDK void Encrypt( LPSTR szStringAddress )
{

	// lee - 230306 - u6b4 - only encrypt/decrypt if from DBPDATA temp folder (should not touch local files!)
	// Dave - 28/03/2014 - commented this out because we will check if a file needs decrypting from now on
	//if ( strnicmp ( szStringAddress, g_Glob.pEXEUnpackDirectory, strlen(g_Glob.pEXEUnpackDirectory) )==NULL )
		EncryptDecrypt ( szStringAddress, true, false );
}

//Dave added 28/03/2014 so we can encrypt from dbpro
DARKSDK void EncryptDBPro ( const char* szStringAddress )
{
	if (!szStringAddress) return;
	char pFilename[_MAX_PATH];
	strcpy_s(pFilename, _MAX_PATH, szStringAddress);
	if(!DoesFileExist(pFilename))
		return;

	char newFileName[_MAX_PATH];
	sprintf_s ( newFileName, _MAX_PATH, "_e_%s" , szStringAddress );

	char buf[BUFSIZ];
	size_t size = 0;

	FILE* source = nullptr;
	FILE* dest = nullptr;
	if (fopen_s(&source, szStringAddress, "rb") != 0 || !source) return;
	if (fopen_s(&dest, newFileName, "wb") != 0 || !dest)
	{
		fclose(source);
		return;
	}

	while ((size = fread(buf, 1, BUFSIZ, source)) > 0)
	{
		fwrite(buf, 1, size, dest);
	}

	fclose(source);
	fclose(dest);

	EncryptDecrypt ( newFileName, true, true );
}

//Dave used for workshop encryption
DARKSDK void EncryptWorkshopDBPro ( const char* szStringAddress )
{
	if (!szStringAddress) return;
	char pFilename[_MAX_PATH];
	strcpy_s(pFilename, _MAX_PATH, szStringAddress);
	if(!DoesFileExist(pFilename))
		return;

	char originalPath[MAX_PATH];
	GetCurrentDirectory ( MAX_PATH, originalPath );

	char* pLocalFile = NULL;
	char filePath[MAX_PATH];
	strcpy_s( filePath, MAX_PATH, pFilename );
	pLocalFile = strrchr ( pFilename , '\\' );
	if ( pLocalFile )
	{	
		char tempName[_MAX_PATH];
		strcpy_s(tempName, _MAX_PATH, pLocalFile + 1);
		strcpy_s(pFilename, _MAX_PATH, tempName);
		pLocalFile = strrchr ( filePath , '\\' );
		if (pLocalFile) pLocalFile[0] = '\0';
		SetCurrentDirectory ( filePath );
	}

	char newFileName[_MAX_PATH];
	sprintf_s ( newFileName, _MAX_PATH, "_w_%s" , pFilename );

	char buf[BUFSIZ];
	size_t size = 0;

	FILE* source = nullptr;
	FILE* dest = nullptr;
	if (fopen_s(&source, pFilename, "rb") != 0 || !source)
	{
		SetCurrentDirectory(originalPath);
		return;
	}
	if (fopen_s(&dest, newFileName, "wb") != 0 || !dest)
	{
		fclose(source);
		SetCurrentDirectory(originalPath);
		return;
	}

	while ((size = fread(buf, 1, BUFSIZ, source)) > 0)
	{
		fwrite(buf, 1, size, dest);
	}

	fclose(source);
	fclose(dest);

	EncryptDecrypt ( newFileName, true, true );

	SetCurrentDirectory(originalPath);
}

DARKSDK bool EncryptNewFile ( const char* szStringAddress )
{
	// do not encrypt any sky models (as they use internal image loads which are also encrypted 
	// and I cannot load encryped files from within the temp folder where the decrypted.x is)
	const char* pScanFilename = szStringAddress;
	char pThisDirAndFile[MAX_PATH];
	GetCurrentDirectory ( MAX_PATH, pThisDirAndFile );
	strcat_s ( pThisDirAndFile, MAX_PATH, "\\" );
	strcat_s ( pThisDirAndFile, MAX_PATH, pScanFilename );
	int iScanMax = static_cast<int>(strlen(pThisDirAndFile))-8;
	if ( iScanMax < 0 ) iScanMax = 0;
	if ( strlen ( pThisDirAndFile ) > 8 )
	{
		for ( int n=0; n<iScanMax; n++ )
		{
			if ( strnicmp ( pThisDirAndFile + n, "skybank\\", 8 )==NULL || strnicmp ( pThisDirAndFile + n, "skybank/", 8 )==NULL )
			{
				if ( strnicmp ( pThisDirAndFile + n, "skybank\\", 8 )==NULL || strnicmp ( pThisDirAndFile + n, "skybank/", 8 )==NULL )
				{
					if ( strnicmp ( pThisDirAndFile + strlen(pThisDirAndFile) - 2, ".x", 2 )==NULL )
					{
						return false;
					}
				}
			}
		}
	}

	char newFileName[_MAX_PATH];
	sprintf_s ( newFileName, _MAX_PATH, "_e_%s" , szStringAddress );

	char buf[BUFSIZ];
	size_t size = 0;

	FILE* source = nullptr;
	FILE* dest = nullptr;
	if (fopen_s(&source, szStringAddress, "rb") != 0 || !source) return false;
	if (fopen_s(&dest, newFileName, "wb") != 0 || !dest)
	{
		fclose(source);
		return false;
	}

	while ((size = fread(buf, 1, BUFSIZ, source)) > 0)
	{
		fwrite(buf, 1, size, dest);
	}

	fclose(source);
	fclose(dest);

	EncryptDecrypt ( newFileName, true, true );

	return true;
}

// Delete any empty folders
DARKSDK void EncryptAllFiles(const char* szStringAddress)
{
	if (!szStringAddress) return;
	std::string rootDir(szStringAddress);
	WIN32_FIND_DATAA data = { 0 };
    
	std::stack<std::string> directoryListStack;
	directoryListStack.push(rootDir);

	// keep going until we have emptied the directory stack
	while ( !directoryListStack.empty ( ) )
	{
		std::string currentDir = directoryListStack.top();
		directoryListStack.pop();

		std::string searchLocation = currentDir + "\\*.*";

		HANDLE hFind = FindFirstFileA(searchLocation.c_str(), &data);
		if (hFind == INVALID_HANDLE_VALUE)
			continue;

		do 
		{
			if ( strcmp ( data.cFileName, "." ) != 0 && strcmp ( data.cFileName, ".." ) != 0 )
			{
				if ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				{
					directoryListStack.push(currentDir + "\\" + data.cFileName);
				}
				else
				{
					if ( strstr(data.cFileName, ".dds") != NULL ||  strstr(data.cFileName, ".png") != NULL ||  strstr(data.cFileName, ".jpg") != NULL || strstr(data.cFileName, ".x") != NULL || strstr(data.cFileName, ".dbo") != NULL ||  strstr(data.cFileName, ".wav") != NULL ||  strstr(data.cFileName, ".mp3") != NULL )
					{
						if ( strstr ( data.cFileName, "_e_" )  !=  data.cFileName )
						{
							char originalFolder[MAX_PATH];
							GetCurrentDirectoryA ( MAX_PATH, originalFolder );						
							SetCurrentDirectoryA ( currentDir.c_str() );
							bool bEncryptedOkay = EncryptNewFile( data.cFileName );
							SetCurrentDirectoryA ( originalFolder );
							UpdateWindow ( NULL );
							std::string fullPath = currentDir + "\\" + data.cFileName;
							if ( bEncryptedOkay==true ) DeleteFileA ( fullPath.c_str() );
						}
					}
				}
			}
		}
		while ( FindNextFileA ( hFind, &data ) != 0 );

		FindClose ( hFind );
	}


}

// DISPLAY FUNCTIONS
DARKSDK void ConstructPostDisplayItems(HINSTANCE hInstance)
{
	// Construct internals of SupportDLLs (After Display Created)
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_GFX && g_Glob.g_GFXmade==false)
	#endif
	{
		g_Glob.g_GFXmade=true;
		if(g_GFX_PassCoreData) g_GFX_PassCoreData( (LPVOID)&g_Glob, 1 );
	}
	
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Basic2D && g_Glob.g_Basic2Dmade==false)
	#endif
	{
		g_Glob.g_Basic2Dmade=true;
		g_Basic2D_Constructor( g_Glob.g_GFX );
		if(g_Basic2D_SetErrorHandler) g_Basic2D_SetErrorHandler(g_ErrorHandler);
		if(g_Basic2D_PassCoreData) g_Basic2D_PassCoreData( (LPVOID)&g_Glob );
	}
	
	/*
	if(g_Glob.g_Text && g_Glob.g_Textmade==false)
	{
		g_Glob.g_Textmade=true;
		g_Text_Constructor( g_Glob.g_GFX );
		if(g_Text_SetErrorHandler) g_Text_SetErrorHandler(g_ErrorHandler);
		if(g_Text_PassCoreData) g_Text_PassCoreData( (LPVOID)&g_Glob );
	}
	*/

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Text)
	#endif
	{
		if(g_Glob.g_Textmade==false)
		{
			g_Glob.g_Textmade=true;
			if(g_Text_PassCoreData) g_Text_PassCoreData( (LPVOID)&g_Glob );
			if(g_Text_SetErrorHandler) g_Text_SetErrorHandler(g_ErrorHandler);
			g_Text_Constructor( g_Glob.g_GFX );
		}
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Image && g_Glob.g_Imagemade==false)
	#endif
	{
		g_Glob.g_Imagemade=true;
		g_Image_Constructor ( g_Glob.g_GFX );
		if(g_Image_PassSpriteInstance) g_Image_PassSpriteInstance ( g_Glob.g_Sprites );
		if(g_Image_SetErrorHandler) g_Image_SetErrorHandler( g_ErrorHandler );
		if(g_Image_PassCoreData) g_Image_PassCoreData( (LPVOID)&g_Glob );
		g_Image_SetColorKey  ( 0, 0, 0 );
//		g_Image_SetMipmapNum ( 1 ); // leefix - 200303 - allow mipmaps
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Transforms && g_Glob.g_Transformsmade==false)
	#endif
	{
		g_Glob.g_Transformsmade=true;
		if(g_Transforms_PassCoreData) g_Transforms_PassCoreData( (LPVOID)&g_Glob );
		if(g_Transforms_SetErrorHandler) g_Transforms_SetErrorHandler( g_ErrorHandler );
		g_Transforms_Constructor();
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Sprites && g_Glob.g_Spritesmade==false)
	#endif
	{
		
		g_Glob.g_Spritesmade=true;
		if(g_Sprites_PassCoreData) g_Sprites_PassCoreData( (LPVOID)&g_Glob );
		if(g_Sprites_SetErrorHandler) g_Sprites_SetErrorHandler( g_ErrorHandler );
		g_Sprites_Constructor ( g_Glob.g_GFX, g_Glob.g_Image );
		
	}
}

DARKSDK void ConstructPostDLLItems(HINSTANCE hInstance)
{
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Input && g_Glob.g_Inputmade==false)
	#endif
	{
		g_Glob.g_Inputmade=true;
		if(g_Input_PassCoreData) g_Input_PassCoreData( (LPVOID)&g_Glob );
		if(g_Input_SetErrorHandler) g_Input_SetErrorHandler( g_ErrorHandler );
		g_Input_Constructor ( hInstance );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_System && g_Glob.g_Systemmade==false)
	#endif
	{
		g_Glob.g_Systemmade=true;
		if(g_System_PassCoreData) g_System_PassCoreData( (LPVOID)&g_Glob );
		if(g_System_SetErrorHandler) g_System_SetErrorHandler( g_ErrorHandler );
		g_System_Constructor();
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Sound && g_Glob.g_Soundmade==false)
	#endif
	{
		g_Glob.g_Soundmade=true;
		if(g_Sound_PassCoreData) g_Sound_PassCoreData( (LPVOID)&g_Glob );
		if(g_Sound_SetErrorHandler) g_Sound_SetErrorHandler( g_ErrorHandler );
		g_Sound_Constructor ( g_Glob.hWnd );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Music && g_Glob.g_Musicmade==false)
	#endif
	{
		g_Glob.g_Musicmade=true;
		if(g_Music_PassCoreData) g_Music_PassCoreData( (LPVOID)&g_Glob );
		if(g_Music_SetErrorHandler) g_Music_SetErrorHandler( g_ErrorHandler );
		g_Music_Constructor ( g_Glob.hWnd );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_File && g_Glob.g_Filemade==false)
	{
		g_Glob.g_Filemade=true;
		if(g_File_PassCoreData) g_File_PassCoreData( (LPVOID)&g_Glob );
		if(g_File_SetErrorHandler) g_File_SetErrorHandler( g_ErrorHandler );
		g_File_Constructor ( hInstance );
	}
	#else
		g_Glob.g_Filemade=true;
		if(g_File_PassCoreData) g_File_PassCoreData( (LPVOID)&g_Glob );
		if(g_File_SetErrorHandler) g_File_SetErrorHandler( g_ErrorHandler );
		g_File_Constructor ( );
	#endif

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_FTP && g_Glob.g_FTPmade==false)
	#endif
	{
		g_Glob.g_FTPmade=true;
		if(g_FTP_PassCoreData) g_FTP_PassCoreData( (LPVOID)&g_Glob );
		if(g_FTP_SetErrorHandler) g_FTP_SetErrorHandler( g_ErrorHandler );
		g_FTP_Constructor ( hInstance );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Memblocks && g_Glob.g_Memblocksmade==false)
	#endif
	{
		g_Glob.g_Memblocksmade=true;
		if(g_Memblocks_PassCoreData) g_Memblocks_PassCoreData( (LPVOID)&g_Glob );
		if(g_Memblocks_SetErrorHandler) g_Memblocks_SetErrorHandler( g_ErrorHandler );
		g_Memblocks_Constructor ( hInstance );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Animation && g_Glob.g_Animationmade==false)
	#endif
	{
		g_Glob.g_Animationmade=true;
		if(g_Animation_PassCoreData) g_Animation_PassCoreData( (LPVOID)&g_Glob );
		if(g_Animation_SetErrorHandler) g_Animation_SetErrorHandler( g_ErrorHandler );
		g_Animation_Constructor ( hInstance );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Bitmap && g_Glob.g_Bitmapmade==false)
	#endif
	{
		g_Glob.g_Bitmapmade=true;
		if(g_Bitmap_PassCoreData) g_Bitmap_PassCoreData( (LPVOID)&g_Glob );
		if(g_Bitmap_SetErrorHandler) g_Bitmap_SetErrorHandler( g_ErrorHandler );
		g_Bitmap_Constructor ( hInstance );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Multiplayer && g_Glob.g_Multiplayermade==false)
	#endif
	{
		g_Glob.g_Multiplayermade=true;
		if(g_Multiplayer_PassCoreData) g_Multiplayer_PassCoreData( (LPVOID)&g_Glob );
		if(g_Multiplayer_SetErrorHandler) g_Multiplayer_SetErrorHandler( g_ErrorHandler );
		g_Multiplayer_Constructor();
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Camera3D && g_Glob.g_Camera3Dmade==false)
	#endif
	{
		g_Glob.g_Camera3Dmade=true;
		if(g_Camera3D_PassCoreData) g_Camera3D_PassCoreData( (LPVOID)&g_Glob );
		if(g_Camera3D_SetErrorHandler) g_Camera3D_SetErrorHandler( g_ErrorHandler );
		g_Camera3D_Constructor(g_Glob.g_GFX, g_Glob.g_Image);
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Light3D && g_Glob.g_Light3Dmade==false)
	#endif
	{
		g_Glob.g_Light3Dmade=true;
		if(g_Light3D_PassCoreData) g_Light3D_PassCoreData( (LPVOID)&g_Glob );
		if(g_Light3D_SetErrorHandler) g_Light3D_SetErrorHandler( g_ErrorHandler );
		g_Light3D_Constructor(g_Glob.g_GFX);
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Matrix3D && g_Glob.g_Matrix3Dmade==false)
	#endif
	{
		g_Glob.g_Matrix3Dmade=true;
		if(g_Matrix3D_PassCoreData) g_Matrix3D_PassCoreData( (LPVOID)&g_Glob );
		if(g_Matrix3D_SetErrorHandler) g_Matrix3D_SetErrorHandler( g_ErrorHandler );
		g_Matrix3D_Constructor(g_Glob.g_GFX, g_Glob.g_Image);
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Basic3D && g_Glob.g_Basic3Dmade==false)
#endif
	{
		g_Glob.g_Basic3Dmade=true;
		if(g_Basic3D_PassCoreData) g_Basic3D_PassCoreData( (LPVOID)&g_Glob );
		if(g_Basic3D_SetErrorHandler) g_Basic3D_SetErrorHandler( g_ErrorHandler );
		g_Basic3D_Constructor(g_Glob.g_GFX, g_Glob.g_Image, g_Glob.g_Vectors, g_Glob.g_Basic3D );
//		if(g_Basic3D_SendFormats) g_Basic3D_SendFormats ( g_Glob.g_XObject, g_Glob.g_3DSObject, g_Glob.g_MDLObject, g_Glob.g_MD2Object, g_Glob.g_MD3Object, g_Glob.g_PrimObject );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_World3D && g_Glob.g_World3Dmade==false)
	#endif
	{
		g_Glob.g_World3Dmade=true;
		if(g_World3D_PassCoreData) g_World3D_PassCoreData( (LPVOID)&g_Glob );
		if(g_World3D_SetErrorHandler) g_World3D_SetErrorHandler( g_ErrorHandler );
		g_World3D_Constructor(g_Glob.g_GFX, g_Glob.g_Image, g_Glob.g_Camera3D, g_Glob.g_Basic3D );
	}

	if(g_Glob.g_Q2BSP && g_Glob.g_Q2BSPmade==false)
	{
		g_Glob.g_Q2BSPmade=true;
// Handled by Q3BSPInternals
//		g_Q2BSP_Constructor(g_Glob.g_GFX, g_Glob.g_Image, g_Glob.g_Camera3D, g_Glob.g_Basic3D );
//		if(g_Q2BSP_SetErrorHandler) g_Q2BSP_SetErrorHandler( g_ErrorHandler );
//		if(g_Q2BSP_PassCoreData) g_Q2BSP_PassCoreData( (LPVOID)&g_Glob );
	}
	
	if(g_Glob.g_OwnBSP && g_Glob.g_OwnBSPmade==false)
	{
		g_Glob.g_OwnBSPmade=true;
// Handled by Q3BSPInternals
//		g_OwnBSP_Constructor(g_Glob.g_GFX, g_Glob.g_Image, g_Glob.g_Camera3D, g_Glob.g_Basic3D );
//		if(g_OwnBSP_SetErrorHandler) g_OwnBSP_SetErrorHandler( g_ErrorHandler );
//		if(g_OwnBSP_PassCoreData) g_OwnBSP_PassCoreData( (LPVOID)&g_Glob );
	}
	
	if(g_Glob.g_BSPCompiler && g_Glob.g_BSPCompilermade==false)
	{
		g_Glob.g_BSPCompilermade=true;
// Handled by Q3BSPInternals
//		g_BSPCompiler_Constructor(g_Glob.g_GFX );
//		if(g_BSPCompiler_SetErrorHandler) g_BSPCompiler_SetErrorHandler( g_ErrorHandler );
//		if(g_BSPCompiler_PassCoreData) g_BSPCompiler_PassCoreData( (LPVOID)&g_Glob );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Vectors && g_Glob.g_Vectorsmade==false)
	#endif
	{
		g_Glob.g_Vectorsmade=true;
		if(g_Vectors_Constructor) g_Vectors_Constructor(g_Glob.g_GFX);
		if(g_Vectors_SetErrorHandler) g_Vectors_SetErrorHandler( g_ErrorHandler );
		if(g_Vectors_PassCoreData) g_Vectors_PassCoreData( (LPVOID)&g_Glob );
	}

	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Particles && g_Glob.g_Particlesmade==false)
	#endif
	{
		g_Glob.g_Particlesmade=true;
		g_Particles_Constructor(g_Glob.g_GFX, g_Glob.g_Image);
		if(g_Particles_SetErrorHandler) g_Particles_SetErrorHandler( g_ErrorHandler );
		if(g_Particles_PassCoreData) g_Particles_PassCoreData( (LPVOID)&g_Glob );
	}
	
	if(g_Glob.g_LODTerrain && g_Glob.g_LODTerrainmade==false)
	{
		g_Glob.g_LODTerrainmade=true;
		g_LODTerrain_Constructor(g_Glob.g_GFX, g_Glob.g_Image, g_Glob.g_Camera3D);
		if(g_LODTerrain_SetErrorHandler) g_LODTerrain_SetErrorHandler( g_ErrorHandler );
		if(g_LODTerrain_PassCoreData) g_LODTerrain_PassCoreData( (LPVOID)&g_Glob );
	}

	if(g_Glob.g_CSG && g_Glob.g_CSGmade==false)
	{
		g_Glob.g_CSGmade=true;
		g_CSG_Constructor(g_Glob.g_GFX);
		if(g_CSG_SetErrorHandler) g_CSG_SetErrorHandler( g_ErrorHandler );
		if(g_CSG_PassCoreData) g_CSG_PassCoreData( (LPVOID)&g_Glob );
	}
}

DARKSDK void FreeExternalDLLItems(void)
{
	#ifndef DARKSDK_COMPILE
		// A destructor pointer is only valid after SetDBDLLExtCalls resolved
		// it, so the pointer itself must gate the call: cleanup can run after
		// an aborted startup where the glob slot is set but resolution never
		// happened.
		// Apply Destructors before we leave (to support DLLs)
		if(g_Glob.g_Vectors && g_Vectors_Destructor) g_Vectors_Destructor();
		if(g_Glob.g_LODTerrain && g_LODTerrain_Destructor) g_LODTerrain_Destructor();
		if(g_Glob.g_CSG && g_CSG_Destructor) g_CSG_Destructor();

		// Apply Destructors before we leave (DLLs are unloaded later in EXE)
		if(g_Glob.g_Camera3D && g_Camera3D_Destructor) g_Camera3D_Destructor();
		if(g_Glob.g_Light3D && g_Light3D_Destructor) g_Light3D_Destructor();
		if(g_Glob.g_Matrix3D && g_Matrix3D_Destructor) g_Matrix3D_Destructor();
		if(g_Glob.g_Basic3D && g_Basic3D_Destructor) g_Basic3D_Destructor();
		if(g_Glob.g_World3D && g_World3D_Destructor) g_World3D_Destructor();
	// Handled by Q3BSPInternals
	//	if(g_Glob.g_Q2BSP) g_Q2BSP_Destructor(); -Q3BSP internals
	//	if(g_Glob.g_OwnBSP) g_OwnBSP_Destructor(); -Q3BSP internals
	//	if(g_Glob.g_BSPCompiler) g_BSPCompiler_Destructor();
		if(g_Glob.g_Particles && g_Particles_Destructor) g_Particles_Destructor();
		if(g_Glob.g_Multiplayer && g_Multiplayer_Destructor) g_Multiplayer_Destructor();
		if(g_Glob.g_Transforms && g_Transforms_Destructor) g_Transforms_Destructor();
		if(g_Glob.g_Bitmap && g_Bitmap_Destructor) g_Bitmap_Destructor();
		if(g_Glob.g_Animation && g_Animation_Destructor) g_Animation_Destructor();
		if(g_Glob.g_Memblocks && g_Memblocks_Destructor) g_Memblocks_Destructor();
		if(g_Glob.g_FTP && g_FTP_Destructor) g_FTP_Destructor();
		if(g_Glob.g_File && g_File_Destructor) g_File_Destructor();
		if(g_Glob.g_System && g_System_Destructor) g_System_Destructor();
		if(g_Glob.g_Input && g_Input_Destructor) g_Input_Destructor();
		if(g_Glob.g_Sound && g_Sound_Destructor) g_Sound_Destructor();
		if(g_Glob.g_Music && g_Music_Destructor) g_Music_Destructor();
	#else
		g_Vectors_Destructor();
		g_Camera3D_Destructor();
		g_Light3D_Destructor();
		g_Matrix3D_Destructor();
		g_Basic3D_Destructor();
		g_World3D_Destructor();
		g_Particles_Destructor();
		g_Multiplayer_Destructor();
		g_Transforms_Destructor();
		g_Bitmap_Destructor();
		g_Animation_Destructor();
		g_Memblocks_Destructor();
		g_FTP_Destructor();
		g_File_Destructor();
		g_System_Destructor();
		g_Input_Destructor();
		g_Sound_Destructor();
		g_Music_Destructor();

	#endif
}

DARKSDK void FreeExternalDisplayDLLFriends(void)
{
	// Apply Destructors before we leave (DLLs are unloaded later in EXE)
	if(g_Glob.g_Sprites && g_Sprites_Destructor) g_Sprites_Destructor();
	if(g_Glob.g_Image && g_Image_Destructor) g_Image_Destructor();
	
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_Text && g_Text_Destructor)
	#endif
		g_Text_Destructor();
	

	if(g_Glob.g_Basic2D && g_Basic2D_Destructor) g_Basic2D_Destructor();
}

DARKSDK void FreeExternalDisplayDLL(void)
{
	// Free main DirectX DLL
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_GFX && g_GFX_Destructor)
	#endif
		g_GFX_Destructor();

	// Reset Flags
	g_bExternalDisplayActive=false;
	g_bUseExternalDisplayLayer=false;
}

DARKSDK void PassCmdLineHandlerPtr(LPVOID pCmdLinePtr)
{
	// Store pointer to command line string passed into EXE
	g_pCommandLineString = (LPSTR)pCmdLinePtr;
}

DARKSDK void PassErrorHandlerPtr(LPVOID pErrorPtr)
{
	// Store position of runtime error DWORD (held in executable dataspace)
	g_ErrorHandler = pErrorPtr;
	g_pErrorHandler = (CRuntimeErrorHandler*)pErrorPtr;

	// LEEMOD - 150803 - Also store reference in GLOBSTRUCT for ThirdPartyDLLs
	g_Glob.g_pErrorHandlerRef = pErrorPtr;
}

DARKSDK void PassEscapePtr(LPVOID pEscapePtr)
{
	// Store position ofescape value DWORD (held in executable dataspace)
	g_EscapeValue = pEscapePtr;
}

DARKSDK void PassBreakOutPtr(LPVOID pBreakOutPtr)
{
	// Store position of breakout position value DWORD (held in executable dataspace)
	g_BreakOutPosition = pBreakOutPtr;
}

DARKSDK void PassDataStatementPtr(LPSTR pDataStatements, LPSTR pDataStatementsEnd)
{
	g_pDataLabelStart = pDataStatements;
	g_pDataLabelEnd = pDataStatementsEnd;
	g_pDataLabelPtr = pDataStatements;
}

DARKSDK void PassStructurePatterns(LPVOID pPtrToPatternStrings, DWORD dwQty)
{
	// stores structure patterns
	g_dwStructPatternQty		= dwQty;
	g_pStructPatternsPtr		= (LPSTR)pPtrToPatternStrings;
}

DARKSDK void ChangeMouse( DWORD dwCursorID )
{
	// Set Cursor Shape (0-31)
	if ( dwCursorID==0 ) g_ActiveCursor=g_hUseArrow;
	if ( dwCursorID==1 ) g_ActiveCursor=g_hUseHourglass;
	if ( dwCursorID>=2 && dwCursorID<=31 ) g_ActiveCursor=g_hCustomCursors[dwCursorID-2];
	if ( dwCursorID==32 ) g_ActiveCursor=NULL;
	if ( dwCursorID<=31 )
	{
		// change cursor
		SetCursor ( g_ActiveCursor );
	}

}


DARKSDK DWORD InitDisplayEx(DWORD dwDisplayType, DWORD dwWidth, DWORD dwHeight, DWORD dwDepth, HINSTANCE hInstance, LPSTR pApplicationName, HWND pParentHWND, DWORD dwInExStyle, DWORD dwInStyle)
{
	// Assign Function Ptrs to Glob (ensure initialized before any display callbacks)
	g_Glob.CreateDeleteString = CreateSingleString;
	g_Glob.ProcessMessageFunction = ProcessMessagesOnly;
	g_Glob.PrintStringFunction = PrintString;
	g_Glob.UpdateFilenameFromVirtualTable = UpdateFilenameFromVirtualTable;
	g_Glob.Decrypt = Decrypt;
	g_Glob.Encrypt = Encrypt;
	g_Glob.ChangeMouseFunction = ChangeMouse;

	// dwDisplayType
	// =============
	// 0=Hidden Mode
	// 1=Window Mode
	// 2=Desktop Fullscreen Mode
	// 3=Exclusive Fullscreen Mode
	// 4=Desktop Fullscreen Mode (No Taskbar)
	// 5=Use EX and STYLE from values passed in

	// System Settings
	g_dwScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	g_dwScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	// Window Default Settings
	bool bWindowIsDisplayable=true;
	g_Glob.dwWindowX = 0;
	g_Glob.dwWindowY = 0;

	// U75 - 260210 - to support Web Game Builder style ActiveX framing
	// if pass in window mode width and height of 1,2, place window offscreen
	bool bOverrideWindowCenteringToSupportActiveXFraming = false;
	if ( dwWidth==1 && dwHeight==2 )
	{
		bOverrideWindowCenteringToSupportActiveXFraming = true;
		g_Glob.dwWindowX = 5000;
		g_Glob.dwWindowY = 5000;
		dwWidth = 640;
		dwHeight = 480;
	}
	
	// Apply size of screen to global data
	g_Glob.dwWindowWidth = dwWidth;
	g_Glob.dwWindowHeight = dwHeight;
	g_Glob.iScreenWidth = dwWidth;
	g_Glob.iScreenHeight = dwHeight;
	g_Glob.iScreenDepth = dwDepth;

	if(g_Glob.g_GFX==NULL)
	{
		// Using GDI for Display
		if(dwDisplayType==2)
		{
			// Fullscreen Mode - With Taskbar
			RECT rc;
			SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
			g_Glob.dwWindowWidth = rc.right-rc.left;
			g_Glob.dwWindowHeight = rc.bottom-rc.top;
		}
		if(dwDisplayType>=3)
		{
			// Fullscreen Mode - Simply Resize Window
			g_Glob.dwWindowWidth = g_dwScreenWidth;
			g_Glob.dwWindowHeight = g_dwScreenHeight;
		}
	}
	
	// Window Settings
	DWORD dwWindowStyle=0;
	DWORD dwWindowExStyle=0;
	switch(dwDisplayType)
	{
		// leechange - 101004 - should be FULL DESKTOP FULLSCREEN if made visible
		case 0 :	dwWindowStyle = WS_POPUP; // HIDDEN APP   // was WS_MINIMIZE;
					bWindowIsDisplayable=false;
					break;

		case 1 :	dwWindowStyle = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU; // WINDOW APP
					break;

		case 2 :	dwWindowStyle = WS_POPUP; // DESKTOP FULLSCREEN (see taskbar)
					break;

		case 3 :	dwWindowStyle = WS_POPUP; // EXCLUSIVE FULLSCREEN
					break;

		case 4 :	dwWindowStyle = WS_POPUP; // FULL DESKTOP FULLSCREEN (no taskbar)
					break;

		case 5 :	dwWindowStyle = dwInStyle;	// PASSED IN USING DARKGDKINIT
					dwWindowExStyle = dwInExStyle;
					break;

		case 6 :	dwWindowStyle = dwInStyle;	// PASSED IN USING DARKGDKINIT (HIDDEN)
					dwWindowExStyle = dwInExStyle;
					bWindowIsDisplayable=false;
					break;
	}

	// Icons and Cursors
	#ifndef DARKSDK_COMPILE
		g_hUseIcon = (HICON)LoadImage(hInstance, "icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
	#else
		g_hUseIcon = LoadIcon(hInstance,  MAKEINTRESOURCE(101));

		DWORD dwErr = GetLastError ( );
	#endif
	
	// Load Custom Cursors (first slot is ARROW, second is WAIT and third onwards is own)
	HCURSOR hCursor = NULL;
	hCursor = (HCURSOR)LoadImage(hInstance, "arrow.cur", IMAGE_CURSOR, 32, 32, LR_LOADFROMFILE);
	if (hCursor) g_hUseArrow=hCursor;
	hCursor = (HCURSOR)LoadImage(hInstance, "hourglass.cur", IMAGE_CURSOR, 32, 32, LR_LOADFROMFILE);
	if (hCursor) g_hUseHourglass=hCursor;
	for ( DWORD c=2; c<32; c++)
	{
		char str[_MAX_PATH];
		snprintf(str, sizeof(str), "pointer%lu.cur", static_cast<unsigned long>(c));
		hCursor = (HCURSOR)LoadImage(hInstance, str, IMAGE_CURSOR, 32, 32, LR_LOADFROMFILE);
		g_hCustomCursors[c-2]=hCursor;
	}

	// Use Default Cursor otherwise
	if(g_hUseArrow==NULL) g_hUseArrow = LoadCursor(NULL, IDC_ARROW);
	if(g_hUseHourglass==NULL) g_hUseHourglass = LoadCursor(NULL, IDC_WAIT);

	// Vars
	WNDCLASS wc;

	// Appname
	char pAppName[256];
	char pAppNameUnique[256];
	if ( pApplicationName )
		strcpy( pAppName, pApplicationName );
	else
		strcpy( pAppName, "DB3 Application" );

	// leeadd - 280305 - this ensures no conflict between window class name and application class name
	strcpy ( pAppNameUnique, pAppName );
	strcat ( pAppNameUnique, "12345" );

	// Register window
	if ( g_Glob.hWnd==NULL )
	{
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInstance;
		wc.hIcon = g_hUseIcon;
		wc.hCursor = NULL;
		wc.hbrBackground = NULL;
		wc.lpszMenuName = NULL;
		wc.lpszClassName = pAppNameUnique;
		RegisterClass( &wc );
	}

	// Icon Set Manually (also in winproc too - for cursor restore control)
	g_ActiveCursor = g_hUseArrow;
	g_OldCursor = SetCursor ( g_ActiveCursor );

	// If running in window mode, start in center of screen
	if ( dwDisplayType==1 && bOverrideWindowCenteringToSupportActiveXFraming==false )
	{
		g_Glob.dwWindowX=(GetSystemMetrics(SM_CXSCREEN)-g_Glob.dwWindowWidth)/2;
		g_Glob.dwWindowY=(GetSystemMetrics(SM_CYSCREEN)-g_Glob.dwWindowHeight)/2;
	}

	// Create Window (if one not already created)
	g_Glob.hInstance = hInstance;
	if ( g_Glob.hWnd )
	{
		// override window handle with new winproc
		SetWindowLongPtrA ( g_Glob.hWnd, GWLP_WNDPROC, (LONG_PTR)WindowProc );
	}
	else
	{
		g_Glob.hWnd = CreateWindow(	pAppNameUnique,
									pAppName,
									dwWindowStyle,
									g_Glob.dwWindowX,
									g_Glob.dwWindowY,
									g_Glob.dwWindowWidth,
									g_Glob.dwWindowHeight,
									NULL,
									NULL,
									hInstance,
									NULL);
	}

	// Load External DLL Displayer (DirectX/OpenGL/SmegSoft)
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_GFX)
	#endif
	{
		// Construct internals of DisplayDLL First
		bool bDXFailed=false;
		if ( g_GFX_Constructor()==true )
		{
			if(g_GFX_PassCoreData) g_GFX_PassCoreData( (LPVOID)&g_Glob, 0 );
			if(g_GFX_SetErrorHandler) g_GFX_SetErrorHandler(g_ErrorHandler);

			#ifndef DARKSDK_COMPILE
			// If DXcheck failed, exit now
			if(g_GFX_GetDirect3D()==NULL) bDXFailed=true;
			#endif
		}
		else
			bDXFailed=true;


		// Release all if failed
		if ( bDXFailed==true )
		{
			// Direct3D 9 is unavailable (headless, remote desktop, or VM).
			// Fall back to the lightweight GDI display layer instead of
			// failing the whole application.
			g_Glob.g_GFX=NULL;
		}
		else
		{
			// Initialise DisplayDLL
			g_bUseExternalDisplayLayer=true;
			g_GFX_OverrideHWND(g_Glob.hWnd);
		}
	}
	#ifndef DARKSDK_COMPILE
	else
	{
		// not using external display (DX), must be GDI lightwight
		// leeadd - 080306 - u60 - still need to EMBED igLoader if there
		if ( g_pGlob->hwndIGLoader ) SendMessage ( g_pGlob->hwndIGLoader, WM_SETTEXT, 0, (LPARAM)"EMBEDME" );
	}
	#endif

	// Activate COM
	CoInitialize(NULL);

	// Create Display (dwAppDisplayModeUsing controls window handler)
	g_Glob.dwAppDisplayModeUsing=dwDisplayType;
	CreateDisplay(dwDisplayType);


	// Can fail to create starter resolution
	if(*(DWORD*)g_ErrorHandler>0)
	{
		return 1;
	}

	// Assign Function Ptrs to Glob (for other DLLs to use)
	g_Glob.CreateDeleteString = CreateSingleString;
	g_Glob.ProcessMessageFunction = ProcessMessagesOnly;
	g_Glob.PrintStringFunction = PrintString;
	g_Glob.UpdateFilenameFromVirtualTable = UpdateFilenameFromVirtualTable;
	g_Glob.Decrypt = Decrypt;
	g_Glob.Encrypt = Encrypt;
	g_Glob.ChangeMouseFunction = ChangeMouse;

	// Load External DLL Displayer (DirectX/OpenGL/SmegSoft)
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_GFX)
	#endif
		ConstructPostDisplayItems(hInstance);


	// Prepare Other DLLs
	ConstructPostDLLItems(hInstance);


	// Visible Window
	if(bWindowIsDisplayable)
	{
		// igLoader window requires re-positioning after subclassing WinProc
		if ( g_pGlob->hwndIGLoader )
		{
			// Prepare window within browser, zero update until first SYNC command
			g_GFX_Clear( 255, 255, 255 ); // white initial screen (until figure out better way)
			SetWindowPos ( g_Glob.hWnd, HWND_TOP, 0, 0, 640, 480, SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW );
		}
		else
		{
			// Clear Display Area
			ClearPrintArea();

			// Clear Screen
			InvalidateRect(g_Glob.hWnd, NULL, TRUE);
			UpdateWindow(g_Glob.hWnd);

			// Reveal Window
			ShowWindow(g_Glob.hWnd, SW_SHOW);
		}
	}

	// External DisplayDLL Fully Active
	#ifndef DARKSDK_COMPILE
	if(g_Glob.g_GFX)
	#endif
		g_bExternalDisplayActive=true;

	// Set Any After Display Properties (ink, font, etc)
	SetDefaultDisplayProperties();

	// Process any messages prior to program start (also for begin scene call)
	DWORD dwTimer=timeGetTime();
	while(timeGetTime()<dwTimer+100)
	{
		if(g_bUseExternalDisplayLayer) ExternalDisplaySync(0);

		InternalProcessMessages();
	}

	return 0;
}

DARKSDK DWORD InitDisplay(DWORD dwDisplayType, DWORD dwWidth, DWORD dwHeight, DWORD dwDepth, HINSTANCE hInstance, LPSTR pApplicationName)
{
	return InitDisplayEx(dwDisplayType, dwWidth, dwHeight, dwDepth, hInstance, pApplicationName, NULL, 0, 0);
}

DARKSDK void PassDLLs(void)
{
	// Make All External DLL Functions
	SetDBDLLExtCalls();
}

DARKSDK void ConstructDLLs(void)
{
	// Prepare Other DLLs
	ConstructPostDisplayItems(g_Glob.hInstance);
	ConstructPostDLLItems(g_Glob.hInstance);
}

DARKSDK int GetSecurityCode(void)
{	
	// gewnerate once
	srand((int)timeGetTime());
	if ( g_iSecurityCode!=-1 )
	{
		int iSecurityCode = rand()%1000000;
		if ( g_iSecurityCode==0 ) g_iSecurityCode = iSecurityCode;
	}
	return g_iSecurityCode;
}

DARKSDK void WipeSecurityCode(void)
{
	// clear forever
	g_iSecurityCode=-1;
}

#ifdef _WIN64
DARKSDK GlobStruct* GetGlobPtr(void)
{
	return &g_Glob;
}
#else
DARKSDK DWORD GetGlobPtr(void)
{
	return (DWORD_PTR)&g_Glob;
}
#endif

DARKSDK void FreeChecklistStrings(void)
{
	// Free checklist strings safely via dynamic string manager
	if (g_Glob.checklist)
	{
		for(DWORD c=0; c<g_Glob.dwChecklistArraySize; c++)
		{
			if(g_Glob.checklist[c].string)
			{
				if(IsDynamicHeapString(g_Glob.checklist[c].string))
					FreeDynamicString(g_Glob.checklist[c].string);
				g_Glob.checklist[c].string = nullptr;
			}
		}

		// Free main checklist array block
		if(g_pGlob && g_pGlob->CreateDeleteString)
		{
			g_pGlob->CreateDeleteString(reinterpret_cast<DWORD_PTR*>(&g_pGlob->checklist), 0);
		}
		g_Glob.checklist = nullptr;
	}
	g_Glob.dwChecklistArraySize = 0;
	g_Glob.checklistexists = false;
	g_Glob.checklisthasvalues = false;
	g_Glob.checklisthasstrings = false;
}

DARKSDK DWORD CloseDisplay(void)
{
	// Free checklist strings
	FreeChecklistStrings();

	// Close down MiscDLLs
	FreeExternalDLLItems();

	// Close down DisplayDLL Sublinks
	if(g_bUseExternalDisplayLayer) FreeExternalDisplayDLLFriends();

	// Restore Display to Windowed Mode
	DeleteDisplay();

	// Close down DisplayDLL Main
	if(g_bUseExternalDisplayLayer) FreeExternalDisplayDLL();

	// Free default input resources
	if(g_Glob.pWindowsTextEntry)
	{
		delete[] g_Glob.pWindowsTextEntry;
		g_Glob.pWindowsTextEntry=NULL;
	}

	// Free safe rects arrays
	if(g_Glob.pSafeRects)
	{
		delete[] g_Glob.pSafeRects;
		g_Glob.pSafeRects = nullptr;
	}
	g_Glob.dwSafeRectMax = 0;

	// Close Window
	if(g_Glob.hWnd)
	{
		ShowWindow ( g_Glob.hWnd, SW_HIDE );
		CloseWindow(g_Glob.hWnd);
		g_Glob.hWnd=NULL;
	}

	// Free Cursors and Icons
	if(g_hUseIcon) { DestroyIcon(g_hUseIcon); g_hUseIcon = NULL; }
	if(g_hUseArrow) { DestroyCursor(g_hUseArrow); g_hUseArrow = NULL; }
	if(g_hUseHourglass) { DestroyCursor(g_hUseHourglass); g_hUseHourglass = NULL; }

	// Free COM
	CoUninitialize();

	// Complete
	return 0;
}

// ABSOLUTE BASIC COMMANDS (PRINT and INPUT)

DARKSDK void Cls(void)
{
	ClearPrintArea();
	SetPrintCursor(0,0);
}
DARKSDK void SetCursor(int iX, int iY)
{
	SetPrintCursor(iX,iY);
}
DARKSDK void PrintR(LONGLONG lValue)
{
	PrintInteger(lValue, true);
}
DARKSDK void PrintO(double dValue)
{
	PrintFloat(dValue, true);
}
DARKSDK void PrintS(LPSTR pString)
{
	PrintString(pString, true);
}
DARKSDK void Print0(void)
{
	PrintNothing();
}
DARKSDK void PrintCR(LONGLONG lValue)
{
	PrintInteger(lValue, false);
}
DARKSDK void PrintCO(double dValue)
{
	PrintFloat(dValue, false);
}
DARKSDK void PrintCS(LPSTR pString)
{
	PrintString(pString, false);
}

DARKSDK LONGLONG PerformanceTimer ( void )
{
	LARGE_INTEGER large;
	if (!QueryPerformanceCounter ( &large ))
	{
		large.QuadPart = 0;
	};
	return large.QuadPart;
}

DARKSDK LONGLONG PerformanceFrequency ( void )
{
	LARGE_INTEGER large;
	if (! QueryPerformanceFrequency( &large ))
	{
		large.QuadPart = 0;
	}
	return large.QuadPart;
}

DARKSDK LONGLONG InputR(void)
{
	LONGLONG lValue;
	InputInteger(&lValue);
	return lValue;
}
DARKSDK double InputO(void)
{
	double dValue;
	InputFloat(&dValue);
	return dValue;
}
DARKSDK DWORD_PTR InputS(DWORD_PTR pDestStr)
{
	if (pDestStr) FreeDynamicString(reinterpret_cast<void*>(pDestStr));

	LPSTR pString = nullptr;
	InputString(&pString);
	if (!pString)
	{
		return reinterpret_cast<DWORD_PTR>(AllocateDynamicString(0));
	}
	size_t len = strlen(pString);
	char* pDyn = AllocateDynamicString(len);
	strcpy_s(pDyn, len + 1, pString);
	delete[] pString;
	return reinterpret_cast<DWORD_PTR>(pDyn);
}

// MEMORY MANAGEMENT FUNCTIONS

DARKSDK DWORD_PTR CreateVariableSpace(DWORD VariableSpaceSize)
{
	// Create Variable Space. Zero-initialize so every stored handle/pointer
	// (string, UDT, array) starts as NULL; generated code frees the previous
	// value on reassignment, so an uninitialized garbage pointer would be
	// passed to delete[] and corrupt the heap.
	g_pVarSpace = (LPSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, VariableSpaceSize);
	g_Glob.g_pVariableSpace = (LPVOID)g_pVarSpace;
	return (DWORD_PTR)g_pVarSpace;
}

DARKSDK DWORD_PTR CreateDataSpace(DWORD DataSpaceSize)
{
	// Create Data Space (same zero-init rationale as the variable space).
	g_pDataSpace = (LPSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, DataSpaceSize);
	return (DWORD_PTR)g_pDataSpace;
}

DARKSDK void DeleteVariableSpace(void)
{
	// Delete Variable Space Itself
	if(g_pVarSpace)
	{
		GlobalFree(g_pVarSpace);
		g_pVarSpace = nullptr;
		g_Glob.g_pVariableSpace = nullptr;
	}
}

DARKSDK void DeleteDataSpace(void)
{
	// Delete Data Space Itself
	if(g_pDataSpace)
	{
		GlobalFree(g_pDataSpace);
		g_pDataSpace = nullptr;
	}
}

DARKSDK void DeleteSingleVariableAllocation(DWORD_PTR* dwVariableSpaceAddress)
{
	// Delete Actual Allocation within Variable Space.
	if(dwVariableSpaceAddress && *dwVariableSpaceAddress)
	{
		char* ptr = (char*)*dwVariableSpaceAddress;
		if(IsDynamicHeapString(ptr))
		{
			// ptr points 8 bytes past the allocation base (past the string
			// header); delete[] on it corrupts the CRT heap block header.
			// FreeDynamicString rewinds to the base before releasing.
			FreeDynamicString(ptr);
		}
		*dwVariableSpaceAddress = 0;
	}
}

DARKSDK DWORD_PTR CreateArray(DWORD dwSizeOfArray, DWORD dwSizeOfOneDataItem, DWORD dwTypeValueOfOneDataItem)
{
	size_t dwTotalSize = GetArrayTotalAllocationBytes(dwSizeOfArray, dwSizeOfOneDataItem);
	char* pRawMem = new char[dwTotalSize];
	memset(pRawMem, 0, dwTotalSize);

	DBProArrayHeader* pHeader = reinterpret_cast<DBProArrayHeader*>(pRawMem);
	pHeader->magic = kDBProArrayMagic;
	pHeader->size = dwSizeOfArray;
	pHeader->itemSize = dwSizeOfOneDataItem;
	pHeader->typeId = dwTypeValueOfOneDataItem;
	pHeader->cursor = 0;

	// Return handle pointing to the first data element (direct layout)
	return reinterpret_cast<DWORD_PTR>(GetArrayDataPtr(pHeader));
}

constexpr size_t GetUdtFieldSize(char typeChar) noexcept
{
	switch (typeChar)
	{
		case 'B': case 'b': // boolean
		case 'Y': case 'y': // byte
			return 1;
		case 'W': case 'w': // word
			return 2;
		case 'L': case 'l': // integer
		case 'F': case 'f': // float
		case 'D': case 'd': // dword
			return 4;
		case 'O': case 'o': // double float
		case 'R': case 'r': // double integer
			return 8;
		case 'S': case 's': // string
			return sizeof(uintptr_t);
		default:
			return sizeof(uintptr_t);
	}
}

DARKSDK void FreeStringsFromArray(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr) return;
	char* pHead = reinterpret_cast<char*>(dwArrayPtr) - sizeof(DBProArrayHeader);
	if (!IsDynamicArrayMemory(pHead)) return;

	__try
	{
		auto* pHeader = reinterpret_cast<DBProArrayHeader*>(pHead);
		char* pData = GetArrayDataPtr(pHeader);

		if (pHeader->typeId == 2)
		{
			// String array: each element slot is a char* pointer in the data block
			size_t dwSizeOfTable = pHeader->size;
			for (size_t n = 0; n < dwSizeOfTable; n++)
			{
				char** ppStr = reinterpret_cast<char**>(pData + n * sizeof(char*));
				if (*ppStr)
				{
					if (IsDynamicHeapString(*ppStr))
					{
						FreeDynamicString(*ppStr);
					}
					*ppStr = nullptr;
				}
			}
		}
		else if (pHeader->typeId >= 9)
		{
			// UDT array: search pattern for 'S' fields
			LPSTR UdtFormat = GetTypePatternCore(nullptr, pHeader->typeId);
			if (UdtFormat)
			{
				bool ContainsString = false;
				for (const char* CurrentItem = UdtFormat; *CurrentItem; ++CurrentItem)
				{
					if (*CurrentItem == 'S' || *CurrentItem == 's')
					{
						ContainsString = true;
						break;
					}
				}

				if (ContainsString)
				{
					size_t ArraySize = pHeader->size;
					size_t ItemSize = pHeader->itemSize;

					for (size_t Position = 0; Position < ArraySize; ++Position)
					{
						char* pElementBase = pData + Position * ItemSize;
						size_t ItemOffset = 0;
						for (const char* CurrentItem = UdtFormat; *CurrentItem; ++CurrentItem)
						{
							if (*CurrentItem == 'S' || *CurrentItem == 's')
							{
								char** ppStr = reinterpret_cast<char**>(pElementBase + ItemOffset);
								if (*ppStr)
								{
									if (IsDynamicHeapString(*ppStr))
									{
										FreeDynamicString(*ppStr);
									}
									*ppStr = nullptr;
								}
							}
							ItemOffset += GetUdtFieldSize(*CurrentItem);
						}
					}
				}
				delete[] UdtFormat;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

DARKSDK void DeleteArray(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr)
	{
		char* pHead = reinterpret_cast<char*>(dwArrayPtr) - sizeof(DBProArrayHeader);
		if (IsDynamicArrayMemory(pHead))
		{
			auto* pHeader = reinterpret_cast<DBProArrayHeader*>(pHead);
			pHeader->magic = 0; // Clear magic to prevent double-free
			delete[] pHead;
		}
	}
}

DARKSDK DWORD_PTR ExpandArray(DWORD_PTR dwOldArrayPtr, DWORD dwAddElements)
{
	char* pOldHead = reinterpret_cast<char*>(dwOldArrayPtr) - sizeof(DBProArrayHeader);
	auto* pOldHeader = reinterpret_cast<DBProArrayHeader*>(pOldHead);

	uint32_t dwOldSizeOfArray = pOldHeader->size;
	uint32_t dwOldSizeOfOneDataItem = pOldHeader->itemSize;
	uint32_t dwOldTypeValueOfOneDataItem = pOldHeader->typeId;

	size_t dwOldDataSizeInBytes = static_cast<size_t>(dwOldSizeOfArray) * dwOldSizeOfOneDataItem;
	const char* pOldData = GetArrayDataPtr(pOldHeader);

	// Create New Size of Array
	uint32_t dwSizeOfArray = dwOldSizeOfArray + dwAddElements;
	DWORD_PTR dwNewArrayPtr = CreateArray(dwSizeOfArray, dwOldSizeOfOneDataItem, dwOldTypeValueOfOneDataItem);
	auto* pNewHeader = GetArrayHeader(dwNewArrayPtr);

	// Copy dimension multipliers
	memcpy(pNewHeader->dimensions, pOldHeader->dimensions, sizeof(pNewHeader->dimensions));

	char* pNewData = GetArrayDataPtr(pNewHeader);

	// Copy old data to beginning of new data block
	memcpy(pNewData, pOldData, dwOldDataSizeInBytes);

	// Destroy old array
	DeleteArray(dwOldArrayPtr);

	return dwNewArrayPtr;
}

DARKSDK void ClearDataBlock(DWORD_PTR dwArrayPtr, DWORD dwIndex, DWORD dwQuantity)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return;
	auto* pHeader = GetArrayHeader(dwArrayPtr);
	size_t dwSizeOfTable = pHeader->size;
	size_t dwDataItemSize = pHeader->itemSize;
	if (dwIndex >= dwSizeOfTable) return;
	size_t dwAvailable = dwSizeOfTable - dwIndex;
	size_t dwToClear = (static_cast<size_t>(dwQuantity) < dwAvailable) ? static_cast<size_t>(dwQuantity) : dwAvailable;
	char* pData = GetArrayDataPtr(pHeader);
	size_t dwDataOffset = static_cast<size_t>(dwIndex) * dwDataItemSize;
	memset(pData + dwDataOffset, 0, dwToClear * dwDataItemSize);
}

// ARRAY COMMANDS

DARKSDK DWORD_PTR DimCore(DWORD_PTR dwOldArrayPtr, DWORD dwTypeAndSizeOfElement, DWORD dwD1, DWORD dwD2, DWORD dwD3, DWORD dwD4, DWORD dwD5, DWORD dwD6, DWORD dwD7, DWORD dwD8, DWORD dwD9)
{
	// Increment all DBPro dimensions (+1 based)
	dwD1 += 1;
	if (dwD2 > 0) dwD2 += 1;
	if (dwD3 > 0) dwD3 += 1;
	if (dwD4 > 0) dwD4 += 1;
	if (dwD5 > 0) dwD5 += 1;
	if (dwD6 > 0) dwD6 += 1;
	if (dwD7 > 0) dwD7 += 1;
	if (dwD8 > 0) dwD8 += 1;
	if (dwD9 > 0) dwD9 += 1;

	// Work out array size (can be no bigger than DWORD)
	__int64 iiSize = dwD1;
	if (dwD2 > 0) iiSize *= dwD2;
	if (dwD3 > 0) iiSize *= dwD3;
	if (dwD4 > 0) iiSize *= dwD4;
	if (dwD5 > 0) iiSize *= dwD5;
	if (dwD6 > 0) iiSize *= dwD6;
	if (dwD7 > 0) iiSize *= dwD7;
	if (dwD8 > 0) iiSize *= dwD8;
	if (dwD9 > 0) iiSize *= dwD9;
	DWORD dwSizeOfArray = static_cast<DWORD>(iiSize);
	if (dwSizeOfArray != iiSize)
		return 0;

	DWORD dwSizeOfOneDataItem = dwTypeAndSizeOfElement / 4096;
	DWORD dwTypeValueOfOneDataItem = dwTypeAndSizeOfElement - (dwSizeOfOneDataItem * 4096);

	// Create New Array
	DWORD_PTR dwArrayPtr = CreateArray(dwSizeOfArray, dwSizeOfOneDataItem, dwTypeValueOfOneDataItem);
	if (!dwArrayPtr) return 0;

	// Fill array with dimension size data (D1-D9)
	auto* pHeader = GetArrayHeader(dwArrayPtr);
	DWORD dwDimOverallSize = dwD1;
	for (DWORD h = 0; h <= 8; h++)
	{
		pHeader->dimensions[h] = dwDimOverallSize;
		if (h == 0) dwDimOverallSize = dwDimOverallSize * dwD2;
		if (h == 1) dwDimOverallSize = dwDimOverallSize * dwD3;
		if (h == 2) dwDimOverallSize = dwDimOverallSize * dwD4;
		if (h == 3) dwDimOverallSize = dwDimOverallSize * dwD5;
		if (h == 4) dwDimOverallSize = dwDimOverallSize * dwD6;
		if (h == 5) dwDimOverallSize = dwDimOverallSize * dwD7;
		if (h == 6) dwDimOverallSize = dwDimOverallSize * dwD8;
		if (h == 7) dwDimOverallSize = dwDimOverallSize * dwD9;
	}

	return dwArrayPtr;
}

DARKSDK DWORD_PTR ReDimCore(DWORD_PTR dwOldArrayPtr, DWORD dwNewTypeAndSizeOfElement, DWORD dwOD1, DWORD dwOD2, DWORD dwOD3, DWORD dwOD4, DWORD dwOD5, DWORD dwOD6, DWORD dwOD7, DWORD dwOD8, DWORD dwOD9)
{
	DWORD dwD1 = dwOD1, dwD2 = dwOD2, dwD3 = dwOD3, dwD4 = dwOD4, dwD5 = dwOD5, dwD6 = dwOD6, dwD7 = dwOD7, dwD8 = dwOD8, dwD9 = dwOD9;
	dwD1 += 1;
	if (dwD2 > 0) dwD2 += 1;
	if (dwD3 > 0) dwD3 += 1;
	if (dwD4 > 0) dwD4 += 1;
	if (dwD5 > 0) dwD5 += 1;
	if (dwD6 > 0) dwD6 += 1;
	if (dwD7 > 0) dwD7 += 1;
	if (dwD8 > 0) dwD8 += 1;
	if (dwD9 > 0) dwD9 += 1;

	// Old Header Info
	auto* pOldHeader = GetArrayHeader(dwOldArrayPtr);
	DWORD dwSizeOfOneDataItem = pOldHeader->itemSize;
	if (dwSizeOfOneDataItem > 1024000)
		return 0;

	DWORD dwTypeValueOfOneDataItem = pOldHeader->typeId;
	DWORD dwNewSizeOfOneDataItem = dwNewTypeAndSizeOfElement / 4096;
	DWORD dwNewTypeValueOfOneDataItem = dwNewTypeAndSizeOfElement - (dwNewSizeOfOneDataItem * 4096);

	if (dwSizeOfOneDataItem != dwNewSizeOfOneDataItem ||
		dwTypeValueOfOneDataItem != dwNewTypeValueOfOneDataItem)
		return dwOldArrayPtr;

	// Create a New Array of new size
	DWORD_PTR dwNewArrayPtr = DimCore(dwOldArrayPtr, dwNewTypeAndSizeOfElement, dwOD1, dwOD2, dwOD3, dwOD4, dwOD5, dwOD6, dwOD7, dwOD8, dwOD9);
	if (!dwNewArrayPtr) return 0;
	auto* pNewHeader = GetArrayHeader(dwNewArrayPtr);

	// Old Array Offsets
	DWORD dwOld[9]; for (int i = 0; i < 9; i++) dwOld[i] = pOldHeader->dimensions[i];
	DWORD dwNew[9]; for (int i = 0; i < 9; i++) dwNew[i] = pNewHeader->dimensions[i];

	const char* pOldData = GetArrayDataPtr(pOldHeader);
	char* pNewData = GetArrayDataPtr(pNewHeader);

	// Work out old dim values from data chunk sizes
	DWORD dwOldDims[9];
	for (DWORD h = 0; h <= 8; h++)
	{
		DWORD dwDataChunkSize = (h == 0) ? 1 : dwOld[h - 1];
		dwOldDims[h] = (dwDataChunkSize > 0) ? (dwOld[h] / dwDataChunkSize) : 1;
	}

	DWORD dwNewDims[9];
	for (DWORD h = 0; h <= 8; h++)
	{
		DWORD dwDataChunkSize = (h == 0) ? 1 : dwNew[h - 1];
		dwNewDims[h] = (dwDataChunkSize > 0) ? (dwNew[h] / dwDataChunkSize) : 1;
	}

	for (int i = 0; i < 9; i++)
		if (dwOldDims[i] > dwNewDims[i])
			dwOldDims[i] = dwNewDims[i];

	for (int h = 0; h <= 8; h++)
		if (dwOldDims[h] == 0)
			dwOldDims[h] = 1;

	if (pOldHeader->size > 0 && pNewHeader->size > 0)
	{
		for (DWORD dwI1 = 0; dwI1 < dwOldDims[0]; dwI1++)
		for (DWORD dwI2 = 0; dwI2 < dwOldDims[1]; dwI2++)
		for (DWORD dwI3 = 0; dwI3 < dwOldDims[2]; dwI3++)
		for (DWORD dwI4 = 0; dwI4 < dwOldDims[3]; dwI4++)
		for (DWORD dwI5 = 0; dwI5 < dwOldDims[4]; dwI5++)
		for (DWORD dwI6 = 0; dwI6 < dwOldDims[5]; dwI6++)
		for (DWORD dwI7 = 0; dwI7 < dwOldDims[6]; dwI7++)
		for (DWORD dwI8 = 0; dwI8 < dwOldDims[7]; dwI8++)
		for (DWORD dwI9 = 0; dwI9 < dwOldDims[8]; dwI9++)
		{
			DWORD dwOldIndex = (dwI1)+(dwI2*dwOld[0])+(dwI3*dwOld[1])+(dwI4*dwOld[2])+(dwI5*dwOld[3])+(dwI6*dwOld[4])+(dwI7*dwOld[5])+(dwI8*dwOld[6])+(dwI9*dwOld[7]);
			DWORD dwNewIndex = (dwI1)+(dwI2*dwNew[0])+(dwI3*dwNew[1])+(dwI4*dwNew[2])+(dwI5*dwNew[3])+(dwI6*dwNew[4])+(dwI7*dwNew[5])+(dwI8*dwNew[6])+(dwI9*dwNew[7]);
			const char* pOldPtr = pOldData + static_cast<size_t>(dwOldIndex) * dwSizeOfOneDataItem;
			char* pNewPtr = pNewData + static_cast<size_t>(dwNewIndex) * dwSizeOfOneDataItem;
			memcpy(pNewPtr, pOldPtr, dwSizeOfOneDataItem);
		}
	}

	// Free Old Array
	DeleteArray(dwOldArrayPtr);

	return dwNewArrayPtr;
}

DARKSDK DWORD_PTR DimDDD(DWORD_PTR dwOldArrayPtr, DWORD dwTypeAndSizeOfElement, DWORD dwD1, DWORD dwD2, DWORD dwD3, DWORD dwD4, DWORD dwD5, DWORD dwD6, DWORD dwD7, DWORD dwD8, DWORD dwD9)
{
	bool bOldArrayFreed = false;
	try
	{
		if (dwOldArrayPtr)
		{
			char* pHead = reinterpret_cast<char*>(dwOldArrayPtr) - sizeof(DBProArrayHeader);
			if (IsDynamicArrayMemory(pHead))
			{
				DWORD_PTR dwNewArrPtr = ReDimCore(dwOldArrayPtr, dwTypeAndSizeOfElement, dwD1, dwD2, dwD3, dwD4, dwD5, dwD6, dwD7, dwD8, dwD9);
				if (dwNewArrPtr != 0) return dwNewArrPtr;

				FreeStringsFromArray(dwOldArrayPtr);
				DeleteArray(dwOldArrayPtr);
				bOldArrayFreed = true;
			}
		}

		return DimCore(0, dwTypeAndSizeOfElement, dwD1, dwD2, dwD3, dwD4, dwD5, dwD6, dwD7, dwD8, dwD9);
	}
	catch (...)
	{
		RunTimeError(RUNTIMEERROR_NOTENOUGHMEMORY);
		return bOldArrayFreed ? 0 : dwOldArrayPtr;
	}
}

DARKSDK DWORD_PTR UnDimDD(DWORD_PTR dwAllocation)
{
	FreeStringsFromArray(dwAllocation);
	DeleteArray(dwAllocation);
	return 0;
}

// ADVANCED UNIFIED ARRAY HANDLING

DARKSDK void ArrayIndexToBottom(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try { pHeader->cursor = (pHeader->size > 0) ? (pHeader->size - 1) : 0; } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

DARKSDK void ArrayIndexToTop(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try { pHeader->cursor = 0; } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

DARKSDK void NextArrayIndex(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try
		{
			pHeader->cursor++;
			if (pHeader->cursor > pHeader->size)
			{
				pHeader->cursor = pHeader->size;
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

DARKSDK void PreviousArrayIndex(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try
		{
			if (static_cast<int>(pHeader->cursor) > 0)
			{
				pHeader->cursor--;
			}
			else
			{
				pHeader->cursor = static_cast<uint32_t>(-1);
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

DARKSDK DWORD ArrayIndexValid(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try
		{
			return (pHeader->cursor < pHeader->size) ? 1 : 0;
		}
		__except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
	}
	return 0;
}

DARKSDK DWORD ArrayCount(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try
		{
			return (pHeader->size > 0) ? (pHeader->size - 1) : static_cast<DWORD>(-1);
		}
		__except(EXCEPTION_EXECUTE_HANDLER) { return static_cast<DWORD>(-1); }
	}
	return static_cast<DWORD>(-1);
}

DARKSDK DWORD_PTR ArrayInsertAtBottom(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return dwArrayPtr;

	try
	{
		if (!IsArraySingleDim(dwArrayPtr))
		{
			RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM);
			return dwArrayPtr;
		}

		DWORD_PTR dwAllocation = ExpandArray(dwArrayPtr, 1);
		auto* pHeader = GetArrayHeader(dwAllocation);
		int iIndex = static_cast<int>(pHeader->size) - 1;
		if (iIndex < 0) iIndex = 0;
		pHeader->cursor = static_cast<uint32_t>(iIndex);

		return dwAllocation;
	}
	catch (...)
	{
		RunTimeError(RUNTIMEERROR_NOTENOUGHMEMORY);
		return dwArrayPtr;
	}
}

DARKSDK DWORD_PTR ArrayInsertAtBottom(DWORD_PTR dwArrayPtr, int iQuantity)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return dwArrayPtr;

	try
	{
		if (!IsArraySingleDim(dwArrayPtr))
		{
			RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM);
			return dwArrayPtr;
		}

		if (iQuantity < 1) iQuantity = 1;
		DWORD_PTR dwAllocation = ExpandArray(dwArrayPtr, iQuantity);
		auto* pHeader = GetArrayHeader(dwAllocation);
		int iIndex = static_cast<int>(pHeader->size) - iQuantity;
		if (iIndex < 0) iIndex = 0;
		pHeader->cursor = static_cast<uint32_t>(iIndex);

		return dwAllocation;
	}
	catch (...)
	{
		RunTimeError(RUNTIMEERROR_NOTENOUGHMEMORY);
		return dwArrayPtr;
	}
}

DARKSDK DWORD_PTR ArrayInsertAtTop(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return dwArrayPtr;

	try
	{
		if (!IsArraySingleDim(dwArrayPtr))
		{
			RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM);
			return dwArrayPtr;
		}

		DWORD_PTR dwAllocation = ExpandArray(dwArrayPtr, 1);
		auto* pHeader = GetArrayHeader(dwAllocation);
		DWORD dwSizeOfTable = pHeader->size;
		DWORD dwItemSize = pHeader->itemSize;
		char* pData = GetArrayDataPtr(pHeader);

		// The new empty slot is at index (dwSizeOfTable-1).
		// We want it at index 0, shifting all old elements up by 1.
		if (dwSizeOfTable > 1)
		{
			// Save the new empty slot content (zeroed)
			auto pTempSlot = std::make_unique<char[]>(dwItemSize);
			memcpy(pTempSlot.get(), pData + (dwSizeOfTable - 1) * dwItemSize, dwItemSize);
			// Shift existing elements [0..size-2] up to [1..size-1]
			memmove(pData + dwItemSize, pData, (dwSizeOfTable - 1) * dwItemSize);
			// Place the empty slot at index 0
			memcpy(pData, pTempSlot.get(), dwItemSize);
		}

		pHeader->cursor = 0;

		return dwAllocation;
	}
	catch (...)
	{
		RunTimeError(RUNTIMEERROR_NOTENOUGHMEMORY);
		return dwArrayPtr;
	}
}

DARKSDK DWORD_PTR ArrayInsertAtTop(DWORD_PTR dwArrayPtr, int iQuantity)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return dwArrayPtr;

	try
	{
		if (!IsArraySingleDim(dwArrayPtr))
		{
			RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM);
			return dwArrayPtr;
		}

		if (iQuantity < 1) iQuantity = 1;
		DWORD_PTR dwAllocation = ExpandArray(dwArrayPtr, iQuantity);
		auto* pHeader = GetArrayHeader(dwAllocation);
		DWORD dwSizeOfTable = pHeader->size;
		DWORD dwItemSize = pHeader->itemSize;
		char* pData = GetArrayDataPtr(pHeader);
		DWORD dwIndexOfFirstNewSlot = dwSizeOfTable - iQuantity;

		// Save the new empty slots content
		auto pStoreSlots = std::make_unique<char[]>(static_cast<size_t>(iQuantity) * dwItemSize);
		memcpy(pStoreSlots.get(), pData + dwIndexOfFirstNewSlot * dwItemSize, static_cast<size_t>(iQuantity) * dwItemSize);

		size_t dwAmountToShuffle = 0;
		if (dwSizeOfTable > static_cast<DWORD>(iQuantity))
			dwAmountToShuffle = (dwSizeOfTable - iQuantity) * dwItemSize;
		if (dwAmountToShuffle > 0)
			memmove(pData + static_cast<size_t>(iQuantity) * dwItemSize, pData, dwAmountToShuffle);

		memcpy(pData, pStoreSlots.get(), static_cast<size_t>(iQuantity) * dwItemSize);
		pHeader->cursor = 0;

		return dwAllocation;
	}
	catch (...)
	{
		RunTimeError(RUNTIMEERROR_NOTENOUGHMEMORY);
		return dwArrayPtr;
	}
}

DARKSDK DWORD_PTR ArrayInsertAtElement(DWORD_PTR dwArrayPtr, int iIndex)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return dwArrayPtr;

	try
	{
		if (!IsArraySingleDim(dwArrayPtr))
		{
			RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM);
			return dwArrayPtr;
		}

		auto* pOldHeader = GetArrayHeader(dwArrayPtr);
		DWORD dwSizeOfTable = pOldHeader->size;
		if (iIndex < 0 || iIndex >= static_cast<int>(dwSizeOfTable))
		{
			RunTimeError(RUNTIMEERROR_ARRAYINDEXINVALID);
			return dwArrayPtr;
		}

		int iQuantity = 1;
		DWORD_PTR dwAllocation = ExpandArray(dwArrayPtr, iQuantity);
		auto* pHeader = GetArrayHeader(dwAllocation);
		DWORD dwItemSize = pHeader->itemSize;
		char* pData = GetArrayDataPtr(pHeader);

		// The new empty slot is at index dwSizeOfTable (which is old size).
		// Save it, shift elements [iIndex..dwSizeOfTable-1] up by one, place empty at iIndex.
		auto pStoreSlot = std::make_unique<char[]>(dwItemSize);
		memcpy(pStoreSlot.get(), pData + dwSizeOfTable * dwItemSize, dwItemSize);

		size_t dwSizeOfLaterChunk = 0;
		if (dwSizeOfTable > static_cast<DWORD>(iIndex))
			dwSizeOfLaterChunk = dwSizeOfTable - iIndex;
		if (dwSizeOfLaterChunk > 0)
			memmove(pData + (iIndex + iQuantity) * dwItemSize, pData + iIndex * dwItemSize, dwSizeOfLaterChunk * dwItemSize);

		memcpy(pData + iIndex * dwItemSize, pStoreSlot.get(), dwItemSize);
		pHeader->cursor = static_cast<uint32_t>(iIndex);

		return dwAllocation;
	}
	catch (...)
	{
		RunTimeError(RUNTIMEERROR_NOTENOUGHMEMORY);
		return dwArrayPtr;
	}
}

DARKSDK void ArrayDeleteElement(DWORD_PTR dwArrayPtr, int iIndex)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return;

	__try
	{
		if (!IsArraySingleDim(dwArrayPtr))
		{
			RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM);
			return;
		}

		auto* pHeader = GetArrayHeader(dwArrayPtr);
		DWORD dwSizeOfTable = pHeader->size;
		if (dwSizeOfTable == 0) return;

		if (iIndex < 0 || iIndex >= static_cast<int>(dwSizeOfTable))
		{
			RunTimeError(RUNTIMEERROR_ARRAYINDEXINVALID);
			return;
		}

		DWORD dwDataItemSize = pHeader->itemSize;
		char* pData = GetArrayDataPtr(pHeader);
		size_t dwOffset = static_cast<size_t>(iIndex) * dwDataItemSize;

		// If UDT array, free strings inside deleted element
		DWORD dwInternalTypeIndex = pHeader->typeId;
		if (dwInternalTypeIndex >= 9)
		{
			LPSTR pPattern = GetTypePatternCore(nullptr, dwInternalTypeIndex);
			if (pPattern)
			{
				size_t dwTypeInternalOffset = 0;
				size_t patternLen = strlen(pPattern);
				for (size_t n = 0; n < patternLen; n++)
				{
					if (pPattern[n] == 'S')
					{
						char** pStringData = reinterpret_cast<char**>(pData + dwOffset + dwTypeInternalOffset);
						if (*pStringData)
						{
							if (IsDynamicHeapString(*pStringData))
							{
								FreeDynamicString(*pStringData);
							}
							*pStringData = nullptr;
						}
					}
					dwTypeInternalOffset += sizeof(uintptr_t);
				}
				delete[] pPattern;
			}
		}
		else if (dwInternalTypeIndex == 2)
		{
			char** pStringData = reinterpret_cast<char**>(pData + dwOffset);
			if (*pStringData)
			{
				if (IsDynamicHeapString(*pStringData))
				{
					FreeDynamicString(*pStringData);
				}
				*pStringData = nullptr;
			}
		}

		// Shift elements after iIndex down by one slot
		size_t dwElementsAfter = dwSizeOfTable - iIndex - 1;
		if (dwElementsAfter > 0)
			memmove(pData + dwOffset, pData + dwOffset + dwDataItemSize, dwElementsAfter * dwDataItemSize);

		// Clear the now-unused last slot
		memset(pData + (dwSizeOfTable - 1) * dwDataItemSize, 0, dwDataItemSize);

		dwSizeOfTable = dwSizeOfTable - 1;
		pHeader->size = dwSizeOfTable;

		if (pHeader->cursor >= dwSizeOfTable)
			pHeader->cursor = (dwSizeOfTable > 0) ? (dwSizeOfTable - 1) : 0;
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {}
}

DARKSDK void ArrayDeleteElement(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return;
	if (!IsArraySingleDim(dwArrayPtr)) { RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM); return; }

	__try
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		if (pHeader->size == 0) return;
		ArrayDeleteElement(dwArrayPtr, static_cast<int>(pHeader->cursor));
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {}
}

DARKSDK void EmptyArray(DWORD_PTR dwAllocation)
{
	if (!dwAllocation || !IsValidArrayHandle(dwAllocation)) return;

	__try
	{
		auto* pHeader = GetArrayHeader(dwAllocation);
		if (pHeader->size == 0) return;

		DWORD dwSizeOfArray = pHeader->size;
		DWORD dwSizeOfOneDataItem = pHeader->itemSize;
		size_t dwDataSizeInBytes = static_cast<size_t>(dwSizeOfArray) * dwSizeOfOneDataItem;

		char* pData = GetArrayDataPtr(pHeader);

		FreeStringsFromArray(dwAllocation);

		memset(pData, 0, dwDataSizeInBytes);

		pHeader->size = 0;
		pHeader->cursor = 0;
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {}
}

DARKSDK DWORD_PTR PushToStack(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return dwArrayPtr;
	if (!IsArraySingleDim(dwArrayPtr)) { RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM); return dwArrayPtr; }

	dwArrayPtr = ArrayInsertAtBottom(dwArrayPtr);
	auto* pHeader = GetArrayHeader(dwArrayPtr);
	pHeader->cursor = (pHeader->size > 0) ? (pHeader->size - 1) : 0;
	return dwArrayPtr;
}

DARKSDK void PopFromStack(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return;
	if (!IsArraySingleDim(dwArrayPtr)) { RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM); return; }

	__try
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		int iIndexAtEnd = static_cast<int>(pHeader->size) - 1;
		if (iIndexAtEnd >= 0)
		{
			ArrayDeleteElement(dwArrayPtr, iIndexAtEnd);
		}
		pHeader = GetArrayHeader(dwArrayPtr);
		pHeader->cursor = (pHeader->size > 0) ? (pHeader->size - 1) : static_cast<uint32_t>(-1);
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {}
}

DARKSDK DWORD_PTR AddToQueue(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return dwArrayPtr;
	if (!IsArraySingleDim(dwArrayPtr)) { RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM); return dwArrayPtr; }

	dwArrayPtr = ArrayInsertAtBottom(dwArrayPtr);
	auto* pHeader = GetArrayHeader(dwArrayPtr);
	pHeader->cursor = (pHeader->size > 0) ? (pHeader->size - 1) : 0;
	return dwArrayPtr;
}

DARKSDK void RemoveFromQueue(DWORD_PTR dwArrayPtr)
{
	if (!dwArrayPtr || !IsValidArrayHandle(dwArrayPtr)) return;
	if (!IsArraySingleDim(dwArrayPtr)) { RunTimeError(RUNTIMEERROR_ARRAYMUSTBESINGLEDIM); return; }

	__try
	{
		ArrayDeleteElement(dwArrayPtr, 0);
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		pHeader->cursor = (pHeader->size == 0) ? static_cast<uint32_t>(-1) : 0;
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {}
}

DARKSDK void ArrayIndexToStack(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try { pHeader->cursor = (pHeader->size > 0) ? (pHeader->size - 1) : 0; } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

DARKSDK void ArrayIndexToQueue(DWORD_PTR dwArrayPtr)
{
	if (dwArrayPtr && IsValidArrayHandle(dwArrayPtr))
	{
		auto* pHeader = GetArrayHeader(dwArrayPtr);
		__try { pHeader->cursor = 0; } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

// HARDCODE COMMANDS

// 32-bit address pool allocator for MakeByteMemory / DeleteByteMemory.
// DarkBASIC variables (and arrays like refmap) store pointers as 32-bit DWORDs,
// and x64 JIT dereferences pointers with 32-bit zero-extended instructions (e.g. mov eax, [ptr]; mov [rax], ecx).
// Therefore, memory allocated by MakeByteMemory MUST be within the 32-bit virtual address range (< 4GB).
namespace {

struct ByteMemChunk {
	uint32_t magic;         // 0x504F4F4C ('POOL')
	uint32_t payload_size;  // User-requested byte size
	uint32_t is_free;       // 1 if free, 0 if allocated
	uint32_t total_size;    // Total size of this chunk including header (16-byte aligned)
	ByteMemChunk* next_phys;// Physically adjacent next chunk in pool
	ByteMemChunk* prev_phys;// Physically adjacent prev chunk in pool
};

static constexpr uint32_t kByteMemMagic = 0x504F4F4C;
static constexpr size_t kByteMemPoolSize = 64 * 1024 * 1024; // 64 MB
static void* g_pByteMemPool = nullptr;
static ByteMemChunk* g_pFirstByteMemChunk = nullptr;
static CRITICAL_SECTION g_ByteMemLock;
static bool g_bByteMemLockInit = false;

static void InitByteMemPool()
{
	if (!g_bByteMemLockInit)
	{
		InitializeCriticalSection(&g_ByteMemLock);
		g_bByteMemLockInit = true;
	}

	EnterCriticalSection(&g_ByteMemLock);
	if (!g_pByteMemPool)
	{
		// Probe 32-bit user space addresses (< 2GB / < 4GB)
		for (uintptr_t addr = 0x20000000; addr <= 0x70000000; addr += 0x01000000)
		{
			void* p = VirtualAlloc(reinterpret_cast<void*>(addr), kByteMemPoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (p)
			{
				g_pByteMemPool = p;
				g_pFirstByteMemChunk = reinterpret_cast<ByteMemChunk*>(p);
				g_pFirstByteMemChunk->magic = kByteMemMagic;
				g_pFirstByteMemChunk->payload_size = 0;
				g_pFirstByteMemChunk->is_free = 1;
				g_pFirstByteMemChunk->total_size = static_cast<uint32_t>(kByteMemPoolSize);
				g_pFirstByteMemChunk->next_phys = nullptr;
				g_pFirstByteMemChunk->prev_phys = nullptr;
				break;
			}
		}
	}
	LeaveCriticalSection(&g_ByteMemLock);
}

} // anonymous namespace

DARKSDK DWORD_PTR MakeByteMemory(int iSize)
{
	if (iSize <= 0) return 0;
	InitByteMemPool();

	EnterCriticalSection(&g_ByteMemLock);
	if (g_pByteMemPool)
	{
		uint32_t aligned_payload = (static_cast<uint32_t>(iSize) + 15u) & ~15u;
		uint32_t needed_total = static_cast<uint32_t>(sizeof(ByteMemChunk)) + aligned_payload;

		ByteMemChunk* cur = g_pFirstByteMemChunk;
		while (cur)
		{
			if (cur->is_free && cur->total_size >= needed_total)
			{
				uint32_t remaining = cur->total_size - needed_total;
				if (remaining >= sizeof(ByteMemChunk) + 16u)
				{
					ByteMemChunk* split = reinterpret_cast<ByteMemChunk*>(reinterpret_cast<char*>(cur) + needed_total);
					split->magic = kByteMemMagic;
					split->payload_size = 0;
					split->is_free = 1;
					split->total_size = remaining;
					split->next_phys = cur->next_phys;
					split->prev_phys = cur;
					if (cur->next_phys)
						cur->next_phys->prev_phys = split;
					cur->next_phys = split;
					cur->total_size = needed_total;
				}
				cur->is_free = 0;
				cur->payload_size = static_cast<uint32_t>(iSize);
				LeaveCriticalSection(&g_ByteMemLock);
				return reinterpret_cast<DWORD_PTR>(reinterpret_cast<char*>(cur) + sizeof(ByteMemChunk));
			}
			cur = cur->next_phys;
		}
	}
	LeaveCriticalSection(&g_ByteMemLock);

	// Fallback: Individual page allocation below 4GB if 64MB pool ever runs out
	for (uintptr_t addr = 0x20000000; addr <= 0x7FFF0000; addr += 0x00010000)
	{
		size_t alloc_sz = (static_cast<size_t>(iSize) + 4095u) & ~4095u;
		void* p = VirtualAlloc(reinterpret_cast<void*>(addr), alloc_sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (p) return reinterpret_cast<DWORD_PTR>(p);
	}

	// Ultimate fallback
	LPSTR pMem = new char[iSize];
	return reinterpret_cast<DWORD_PTR>(pMem);
}

DARKSDK void DeleteByteMemory(DWORD_PTR dwMem)
{
	if (!dwMem) return;

	if (g_pByteMemPool && dwMem >= reinterpret_cast<DWORD_PTR>(g_pByteMemPool) &&
		dwMem < reinterpret_cast<DWORD_PTR>(g_pByteMemPool) + kByteMemPoolSize)
	{
		EnterCriticalSection(&g_ByteMemLock);
		ByteMemChunk* cur = reinterpret_cast<ByteMemChunk*>(reinterpret_cast<char*>(dwMem) - sizeof(ByteMemChunk));
		if (cur->magic == kByteMemMagic && !cur->is_free)
		{
			cur->is_free = 1;
			// Coalesce forward
			if (cur->next_phys && cur->next_phys->is_free)
			{
				cur->total_size += cur->next_phys->total_size;
				cur->next_phys = cur->next_phys->next_phys;
				if (cur->next_phys)
					cur->next_phys->prev_phys = cur;
			}
			// Coalesce backward
			if (cur->prev_phys && cur->prev_phys->is_free)
			{
				cur->prev_phys->total_size += cur->total_size;
				cur->prev_phys->next_phys = cur->next_phys;
				if (cur->next_phys)
					cur->next_phys->prev_phys = cur->prev_phys;
			}
		}
		LeaveCriticalSection(&g_ByteMemLock);
		return;
	}

	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(reinterpret_cast<const void*>(dwMem), &mbi, sizeof(mbi)) == sizeof(mbi))
	{
		if (mbi.AllocationBase == reinterpret_cast<void*>(dwMem))
		{
			VirtualFree(reinterpret_cast<void*>(dwMem), 0, MEM_RELEASE);
			return;
		}
	}

	if (IsDynamicArrayMemory(reinterpret_cast<const void*>(dwMem)))
	{
		delete[] reinterpret_cast<char*>(dwMem);
	}
}

DARKSDK void FillByteMemory(DWORD_PTR dwDest, int iValue, int iSize)
{
	if (dwDest && iSize > 0)
		memset(reinterpret_cast<char*>(dwDest), iValue, iSize);
}

DARKSDK void CopyByteMemory(DWORD_PTR dwDest, DWORD_PTR dwSrc, int iSize)
{
	if (dwDest && dwSrc && iSize > 0)
		memcpy(reinterpret_cast<char*>(dwDest), reinterpret_cast<const char*>(dwSrc), iSize);
}

// DATA STATEMENT COMMAND FUNCTIONS
DARKSDK void Restore(void)
{
	g_pDataLabelPtr = g_pDataLabelStart;		
}
DARKSDK void RestoreD(DWORD_PTR dwDataLabel)
{
	if ( dwDataLabel==0 )
		g_pDataLabelPtr = g_pDataLabelStart;
	else
		g_pDataLabelPtr = (LPSTR)dwDataLabel;
		
}
DARKSDK DWORD ReadL(void)
{
	double dData=0;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==1)
			dData = *(double*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	int iValue = (int)dData;

	return *(DWORD*)&iValue;
}
DARKSDK DWORD ReadF(void)
{
	double dData=0;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==1)
			dData = *(double*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	float fValue = (float)dData;

	return *(DWORD*)&fValue;
}
DARKSDK DWORD_PTR ReadS(DWORD_PTR pDestStr)
{
	if (pDestStr) FreeDynamicString(reinterpret_cast<void*>(pDestStr));

	LPSTR pDatStr = NULL;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==2)
			pDatStr = (LPSTR)*(uintptr_t*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	char* pString;
	if (pDatStr)
	{
		size_t dwLength = strlen(pDatStr);
		pString = AllocateDynamicString(dwLength);
		strcpy_s(pString, dwLength + 1, pDatStr);
	}
	else
	{
		pString = AllocateDynamicString(0);
	}

	return reinterpret_cast<DWORD_PTR>(pString);
}
DARKSDK BYTE ReadB(void)
{
	double dData=0;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==1)
			dData = *(double*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	uint8_t dwValue = static_cast<uint8_t>(dData);

	return dwValue;
}
DARKSDK WORD ReadW(void)
{
	double dData=0;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==1)
			dData = *(double*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	uint16_t dwValue = static_cast<uint16_t>(dData);

	return dwValue;
}
DARKSDK DWORD ReadD(void)
{
	double dData=0;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==1)
			dData = *(double*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	DWORD dwValue = (DWORD)dData;

	return dwValue;
}
DARKSDK LONGLONG ReadR(void)
{
	double dData=0;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==1)
			dData = *(double*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	LONGLONG lValue = (LONGLONG)dData;

	return lValue;
}
DARKSDK double ReadO(void)
{
	double dValue=0;
	if(g_pDataLabelPtr && g_pDataLabelPtr<g_pDataLabelEnd)
	{
		if(*(g_pDataLabelPtr+0)==1)
			dValue = *(double*)(g_pDataLabelPtr+2);

		// Advance After Read, but only to end of data
		g_pDataLabelPtr+=10;
	}

	return dValue;
}

// DWORD POINTER MATHS

/*
DARKDLL DWORD MulDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA*dwValueB;
	return result;
}
DARKDLL DWORD DivDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA/dwValueB;
	return result;
}
DARKDLL DWORD AddDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA+dwValueB;
	return result;
}
DARKDLL DWORD SubDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA-dwValueB;
	return result;
}
*/

// lee - 240306 - u6b4 - re-introduced so that 0xFF < 0x00 can be false

DARKSDK DWORD EqualDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA==dwValueB;
	return result;
}
DARKSDK DWORD GreaterDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA>dwValueB;
	return result;
}
DARKSDK DWORD LessDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA<dwValueB;
	return result;
}
DARKSDK DWORD NotEqualDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA!=dwValueB;
	return result;
}
DARKSDK DWORD GreaterEqualDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA>=dwValueB;
	return result;
}
DARKSDK DWORD LessEqualDDD(DWORD dwValueA, DWORD dwValueB)
{
	int result = dwValueA<=dwValueB;
	return result;
}

// EXTERNAL SUPPORT MATHS

DARKSDK DWORD PowerLLL(int iValueA, int iValueB)
{
	// do not know the ASM version of this
	// mike - 020206 - addition for vs8
	//int result = (int)pow(iValueA,iValueB);
	int result = (int)pow((double)iValueA,(double)iValueB);
	return *((DWORD*)&result);
}
DARKSDK DWORD PowerBBB(DWORD dwValueA, DWORD dwValueB)
{
	// mike - 020206 - addition for vs8
	//DWORD result = (unsigned char)pow((unsigned char)dwValueA,(unsigned char)dwValueB);
	DWORD result = (unsigned char)pow((double)dwValueA,(double)dwValueB);
	return result;
}
DARKSDK DWORD PowerWWW(DWORD dwValueA, DWORD dwValueB)
{
	// mike - 020206 - addition for vs8
	//DWORD result = (WORD)pow((WORD)dwValueA,(WORD)dwValueB);
	DWORD result = (WORD)pow((double)dwValueA,(double)dwValueB);
	return result;
}
DARKSDK DWORD PowerDDD(DWORD dwValueA, DWORD dwValueB)
{
	DWORD result = static_cast<DWORD>((DWORD_PTR)pow(( double ) dwValueA,( double ) dwValueB));
	return result;
}

/*
DARKDLL DWORD MulLLL(int iValueA, int iValueB)
{
	int result = iValueA*iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD DivLLL(int iValueA, int iValueB)
{
	if(iValueB==0) return 0;
	int result = iValueA/iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD AddLLL(int iValueA, int iValueB)
{
	int result = iValueA+iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD SubLLL(int iValueA, int iValueB)
{
	int result = iValueA-iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD ModLLL(int iValueA, int iValueB)
{
	if(iValueB==0) return 0;
	int result = iValueA % iValueB;
	return *((DWORD*)&result);
}
*/

// BITWISE COMMANDS
/*
DARKDLL DWORD ShiftLeftLLL(int iValueA, int iValueB)
{
	int result = iValueA<<iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD ShiftRightLLL(int iValueA, int iValueB)
{
	int result = iValueA>>iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD BitANDLLL(int iValueA, int iValueB)
{
	int result = iValueA & iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD BitORLLL(int iValueA, int iValueB)
{
	int result = iValueA | iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD BitXORLLL(int iValueA, int iValueB)
{
	int result = iValueA ^ iValueB;
	return *((DWORD*)&result);
}
*/

// LOGIC OPERATION MATHS

/*
DARKDLL DWORD OrLLL(int iValueA, int iValueB)
{
	int result = iValueA || iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD AndLLL(int iValueA, int iValueB)
{
	int result = iValueA && iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD NotLLL(int iValueA, int iValueB)
{
	// iValueA ignored
	int result = !iValueB;
	return *((DWORD*)&result);
}
*/

// COMPARISON MATH

/*
DARKDLL DWORD EqualLLL(int iValueA, int iValueB)
{
	int result = iValueA==iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD GreaterLLL(int iValueA, int iValueB)
{
	int result = iValueA>iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD LessLLL(int iValueA, int iValueB)
{
	int result = iValueA<iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD NotEqualLLL(int iValueA, int iValueB)
{
	int result = iValueA!=iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD GreaterEqualLLL(int iValueA, int iValueB)
{
	int result = iValueA>=iValueB;
	return *((DWORD*)&result);
}
DARKDLL DWORD LessEqualLLL(int iValueA, int iValueB)
{
	int result = iValueA<=iValueB;
	return *((DWORD*)&result);
}
*/

// FLOAT MATHS

DARKSDK DWORD PowerFFF(float fValueA, float fValueB)
{
	float result = (float)pow(fValueA,fValueB);
	return *((DWORD*)&result);
}
DARKSDK DWORD MulFFF(float fValueA, float fValueB)
{
	float result = fValueA*fValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD DivFFF(float fValueA, float fValueB)
{
	if(fValueB==0) return 0;
	float result = fValueA/fValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD AddFFF(float fValueA, float fValueB)
{
	float result = fValueA+fValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD SubFFF(float fValueA, float fValueB)
{
	float result = fValueA-fValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD ModFFF(float fValueA, float fValueB)
{
	// lee - 150206 - u60 -floating point MOD added
	if(fValueB==0) return 0;
	double w = (double)fValueA;
	double x = (double)fValueB;
	double z = fmod( w, x );
	float result = (float)z;
	return *((DWORD*)&result);
}

// FLOAT COMPARE MATHS

DARKSDK DWORD EqualLFF(float fValueA, float fValueB)
{
	return fValueA==fValueB;
}
DARKSDK DWORD GreaterLFF(float fValueA, float fValueB)
{
	return fValueA>fValueB;
}
DARKSDK DWORD LessLFF(float fValueA, float fValueB)
{
	return fValueA<fValueB;
}
DARKSDK DWORD NotEqualLFF(float fValueA, float fValueB)
{
	return fValueA!=fValueB;
}
DARKSDK DWORD GreaterEqualLFF(float fValueA, float fValueB)
{
	return fValueA>=fValueB;
}
DARKSDK DWORD LessEqualLFF(float fValueA, float fValueB)
{
	return fValueA<=fValueB;
}

// STRING COMPARE MATHS

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
	__try {
		const char* s = reinterpret_cast<const char*>(ptr);
		return strlen(s);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

static inline void SafeStrCopy(char* dest, DWORD_PTR src, size_t maxLen)
{
	if (!dest || maxLen == 0) return;
	dest[0] = '\0';
	if (!IsReadablePointer(src))
		return;
	__try {
		const char* s = reinterpret_cast<const char*>(src);
		size_t len = strlen(s);
		if (len >= maxLen) len = maxLen - 1;
		memcpy(dest, s, len);
		dest[len] = '\0';
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		dest[0] = '\0';
	}
}

static inline bool IsValidStringPointer(DWORD_PTR ptr)
{
	return SafeStrLen(ptr) > 0;
}

// Range-check and probe a candidate string pointer; NULL and
// invalid/uncommitted addresses are treated as the empty string (DBPro convention).
static inline const char* StringPtrOrEmpty(DWORD_PTR ptr)
{
	if (!IsReadablePointer(ptr))
		return "";
	return reinterpret_cast<const char*>(ptr);
}

DARKSDK DWORD EqualLSS(DWORD_PTR dwSrcStr, DWORD_PTR dwDestStr)
{
	// Single pass: one strcmp under SEH. NULL/invalid on both sides maps to
	// ""=="" and returns equal, matching SafeStrLen-based semantics.
	__try {
		return (strcmp(StringPtrOrEmpty(dwSrcStr), StringPtrOrEmpty(dwDestStr)) == 0) ? 1 : 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

DARKSDK DWORD GreaterLSS(DWORD_PTR dwSrcStr, DWORD_PTR dwDestStr)
{
	__try {
		return (strcmp(StringPtrOrEmpty(dwSrcStr), StringPtrOrEmpty(dwDestStr)) > 0) ? 1 : 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

DARKSDK DWORD LessLSS(DWORD_PTR dwSrcStr, DWORD_PTR dwDestStr)
{
	__try {
		return (strcmp(StringPtrOrEmpty(dwSrcStr), StringPtrOrEmpty(dwDestStr)) < 0) ? 1 : 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

DARKSDK DWORD NotEqualLSS(DWORD_PTR dwSrcStr, DWORD_PTR dwDestStr)
{
	return EqualLSS(dwSrcStr, dwDestStr) ? 0 : 1;
}

DARKSDK DWORD GreaterEqualLSS(DWORD_PTR dwSrcStr, DWORD_PTR dwDestStr)
{
	return LessLSS(dwSrcStr, dwDestStr) ? 0 : 1;
}

DARKSDK DWORD LessEqualLSS(DWORD_PTR dwSrcStr, DWORD_PTR dwDestStr)
{
	return GreaterLSS(dwSrcStr, dwDestStr) ? 0 : 1;
}


// STRING MATHS

DARKSDK DWORD_PTR AddSSS(DWORD_PTR dwRetStr, DWORD_PTR dwSrcStrA, DWORD_PTR dwSrcStrB)
{
	size_t lenA = SafeStrLen(dwSrcStrA);
	size_t lenB = SafeStrLen(dwSrcStrB);
	char* lpNewStr = AllocateDynamicString(lenA + lenB);
	SafeStrCopy(lpNewStr, dwSrcStrA, lenA + 1);
	SafeStrCopy(lpNewStr + lenA, dwSrcStrB, lenB + 1);
	lpNewStr[lenA + lenB] = '\0';
	if (dwRetStr) FreeDynamicString(reinterpret_cast<void*>(dwRetStr));
	return reinterpret_cast<DWORD_PTR>(lpNewStr);
}

DARKSDK DWORD_PTR EquateSS(DWORD_PTR dwDestStr, DWORD_PTR dwSrcStr)
{
	// Always produce a valid allocation, even for an empty source: classic
	// DBPro semantics guarantee a usable (empty) string object after
	// assignment, and downstream code/plugins may dereference the handle
	// without a NULL check.
	size_t len = SafeStrLen(dwSrcStr);
	char* lpNewStr = AllocateDynamicString(len);
	if (len > 0)
		SafeStrCopy(lpNewStr, dwSrcStr, len + 1);
	if (dwDestStr) FreeDynamicString(reinterpret_cast<void*>(dwDestStr));
	return reinterpret_cast<DWORD_PTR>(lpNewStr);
}
DARKSDK DWORD_PTR FreeSS(DWORD_PTR dwDestStr)
{
	if (dwDestStr) FreeDynamicString(reinterpret_cast<void*>(dwDestStr));
	return 0;
}
DARKSDK DWORD_PTR FreeStringSS(DWORD_PTR dwDestStr)
{
	if (dwDestStr) FreeDynamicString(reinterpret_cast<void*>(dwDestStr));
	return 0;
}

// DOUBLE FLOAT MATHS

DARKSDK double PowerOOO(double dValueA, double dValueB)
{
	double result = (float)pow(dValueA,dValueB);
	return result;
}
DARKSDK double MulOOO(double dValueA, double dValueB)
{
	double result = dValueA*dValueB;
	return result;
}
DARKSDK double DivOOO(double dValueA, double dValueB)
{
	if(dValueB==0) return 0;
	double result = dValueA/dValueB;
	return result;
}
DARKSDK double AddOOO(double dValueA, double dValueB)
{
	double result = dValueA+dValueB;
	return result;
}
DARKSDK double SubOOO(double dValueA, double dValueB)
{
	double result = dValueA-dValueB;
	return result;
}

// DOUBLE FLOAT COMPARISONS

DARKSDK DWORD EqualLOO(double dValueA, double dValueB)
{
	int result = dValueA==dValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD GreaterLOO(double dValueA, double dValueB)
{
	int result = dValueA>dValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD LessLOO(double dValueA, double dValueB)
{
	int result = dValueA<dValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD NotEqualLOO(double dValueA, double dValueB)
{
	int result = dValueA!=dValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD GreaterEqualLOO(double dValueA, double dValueB)
{
	int result = dValueA>=dValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD LessEqualLOO(double dValueA, double dValueB)
{
	int result = dValueA<=dValueB;
	return *((DWORD*)&result);
}

// DOUBLE INTEGER MATHS

DARKSDK LONGLONG PowerRRR(LONGLONG dValueA, LONGLONG dValueB)
{
	LONGLONG result = (LONGLONG)pow((double)dValueA,(double)dValueB);
	return result;
}
DARKSDK LONGLONG MulRRR(LONGLONG dValueA, LONGLONG dValueB)
{
	LONGLONG result = dValueA*dValueB;
	return result;
}
DARKSDK LONGLONG DivRRR(LONGLONG dValueA, LONGLONG dValueB)
{
	if(dValueB==0) return 0;
	LONGLONG result = dValueA/dValueB;
	return result;
}
DARKSDK LONGLONG AddRRR(LONGLONG dValueA, LONGLONG dValueB)
{
	LONGLONG result = dValueA+dValueB;
	return result;
}
DARKSDK LONGLONG SubRRR(LONGLONG dValueA, LONGLONG dValueB)
{
	LONGLONG result = dValueA-dValueB;
	return result;
}

// DOUBLE INTEGER COMPARISONS

DARKSDK DWORD EqualLRR(LONGLONG lValueA, LONGLONG lValueB)
{
	int result = lValueA==lValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD GreaterLRR(LONGLONG lValueA, LONGLONG lValueB)
{
	int result = lValueA>lValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD LessLRR(LONGLONG lValueA, LONGLONG lValueB)
{
	int result = lValueA<lValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD NotEqualLRR(LONGLONG lValueA, LONGLONG lValueB)
{
	int result = lValueA!=lValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD GreaterEqualLRR(LONGLONG lValueA, LONGLONG lValueB)
{
	int result = lValueA>=lValueB;
	return *((DWORD*)&result);
}
DARKSDK DWORD LessEqualLRR(LONGLONG lValueA, LONGLONG lValueB)
{
	int result = lValueA<=lValueB;
	return *((DWORD*)&result);
}

// CASTING MATHS

DARKSDK DWORD CastLtoF(int iValue)
{
	float result = static_cast<float>(iValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastLtoB(int iValue)
{
	uint8_t result = static_cast<uint8_t>(iValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastLtoY(int iValue)
{
	uint8_t result = static_cast<uint8_t>(iValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastLtoW(int iValue)
{
	uint16_t result = static_cast<uint16_t>(iValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastLtoD(int iValue)
{
	DWORD result = static_cast<DWORD>(iValue);
	return *((DWORD*)&result);
}
DARKSDK double CastLtoO(int iValue)
{
	return static_cast<double>(iValue);
}
DARKSDK LONGLONG CastLtoR(int iValue)
{
	return static_cast<LONGLONG>(iValue);
}
DARKSDK DWORD CastFtoL(float fValue)
{
	int result = static_cast<int>(fValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastFtoB(float fValue)
{
	uint8_t result = static_cast<uint8_t>(fValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastFtoW(float fValue)
{
	uint16_t result = static_cast<uint16_t>(fValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastFtoD(float fValue)
{
	// a maxed out DWORD produces wrong float, so keep it within 4bytes 
	LONGLONG Long = static_cast<LONGLONG>(fValue);
	if(Long>4294967295) Long=4294967295;

	DWORD result = static_cast<DWORD>(Long);
	return *((DWORD*)&result);
}
DARKSDK double CastFtoO(float fValue)
{
	return static_cast<double>(fValue);
}
DARKSDK LONGLONG CastFtoR(float fValue)
{
	return static_cast<LONGLONG>(fValue);
}
DARKSDK DWORD CastBtoL(unsigned char cValue)
{
	int result = static_cast<int>(cValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastBtoF(unsigned char cValue)
{
	float result = static_cast<float>(cValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastBtoW(unsigned char cValue)
{
	uint16_t result = static_cast<uint16_t>(cValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastBtoD(unsigned char cValue)
{
	DWORD result = static_cast<DWORD>(cValue);
	return *((DWORD*)&result);
}
DARKSDK double CastBtoO(unsigned char cValue)
{
	return static_cast<double>(cValue);
}
DARKSDK LONGLONG CastBtoR(unsigned char cValue)
{
	return static_cast<LONGLONG>(cValue);
}
DARKSDK DWORD CastWtoL(WORD wValue)
{
	int result = static_cast<int>(wValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastWtoF(WORD wValue)
{
	float result = static_cast<float>(wValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastWtoB(WORD wValue)
{
	uint8_t result = static_cast<uint8_t>(wValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastWtoD(WORD wValue)
{
	DWORD result = static_cast<DWORD>(wValue);
	return *((DWORD*)&result);
}
DARKSDK double CastWtoO(WORD wValue)
{
	return static_cast<double>(wValue);
}
DARKSDK LONGLONG CastWtoR(WORD wValue)
{
	return static_cast<LONGLONG>(wValue);
}
DARKSDK DWORD CastDtoL(DWORD dwValue)
{
	int result = static_cast<int>(dwValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastDtoF(DWORD dwValue)
{
	float result = static_cast<float>(dwValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastDtoB(DWORD dwValue)
{
	uint8_t result = static_cast<uint8_t>(dwValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastDtoW(DWORD dwValue)
{
	uint16_t result = static_cast<uint16_t>(dwValue);
	return *((DWORD*)&result);
}
DARKSDK double CastDtoO(DWORD dwValue)
{
	return static_cast<double>(dwValue);
}
DARKSDK LONGLONG CastDtoR(DWORD dwValue)
{
	return static_cast<LONGLONG>(dwValue);
}
DARKSDK DWORD CastOtoL(double dValue)
{
	int result = static_cast<int>(dValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastOtoF(double dValue)
{
	float result = static_cast<float>(dValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastOtoB(double dValue)
{
	uint8_t result = static_cast<uint8_t>(dValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastOtoW(double dValue)
{
	uint16_t result = static_cast<uint16_t>(dValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastOtoD(double dValue)
{
	DWORD result = static_cast<DWORD>(dValue);
	return *((DWORD*)&result);
}
DARKSDK LONGLONG CastOtoR(double dValue)
{
	LONGLONG result = static_cast<LONGLONG>(dValue);
	return result;
}
DARKSDK DWORD CastRtoL(LONGLONG lValue)
{
	int result = static_cast<int>(lValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastRtoF(LONGLONG lValue)
{
	float result = static_cast<float>(lValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastRtoB(LONGLONG lValue)
{
	uint8_t result = static_cast<uint8_t>(lValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastRtoW(LONGLONG lValue)
{
	uint16_t result = static_cast<uint16_t>(lValue);
	return *((DWORD*)&result);
}
DARKSDK DWORD CastRtoD(LONGLONG lValue)
{
	DWORD result = static_cast<DWORD>(lValue);
	return *((DWORD*)&result);
}
DARKSDK double CastRtoO(LONGLONG lValue)
{
	return static_cast<double>(lValue);
}

// MATHEMATICAL COMMANDS
DBPRO_GLOBAL double gDegToRad = 3.141592654f/180.0f;
DBPRO_GLOBAL double gRadToDeg = 180.0f/3.141592654f;

DB_EXPORT dbReturnFloat_t AbsFF(float fValue)
{
	/*
	float result = (float)fabs(fValue);
	return *((DWORD*)&result);
	*/
	return dbReturnFloat( db3::Abs( fValue ) );
}

DARKSDK DWORD IntLF(float fValue)
{
	int result = static_cast<int>(fValue);
	return *((DWORD*)&result);
}

DARKSDK DWORD AcosFF(float fValue)
{
	float result = static_cast<float>(std::acos(fValue) * gRadToDeg);
	return *((DWORD*)&result);
}

DARKSDK DWORD AsinFF(float fValue)
{
	float result = static_cast<float>(std::asin(fValue) * gRadToDeg);
	return *((DWORD*)&result);
}

DARKSDK DWORD AtanFF(float fValue)
{
	float result = static_cast<float>(std::atan(fValue) * gRadToDeg);
	return *((DWORD*)&result);
}

DARKSDK DWORD Atan2FFF(float fA, float fB)
{
	float result = static_cast<float>(std::atan2(fA, fB) * gRadToDeg);
	return *((DWORD*)&result);
}

DARKSDK dbReturnFloat_t CosFF(float fAngle)
{
	return dbReturnFloat( db3::Cos(fAngle) );
}

DARKSDK dbReturnFloat_t SinFF(float fAngle)
{
	return dbReturnFloat( db3::Sin(fAngle) );
}

DARKSDK dbReturnFloat_t TanFF(float fAngle)
{
	return dbReturnFloat( db3::Tan(fAngle) );
}

DARKSDK DWORD HcosFF(float fAngle)
{
	float result = static_cast<float>(std::cosh(fAngle * gDegToRad));
	return *((DWORD*)&result);
}

DARKSDK DWORD HsinFF(float fAngle)
{
	float result = static_cast<float>(std::sinh(fAngle * gDegToRad));
	return *((DWORD*)&result);
}

DARKSDK DWORD HtanFF(float fAngle)
{
	float result = static_cast<float>(std::tanh(fAngle * gDegToRad));
	return *((DWORD*)&result);
}

DARKSDK DWORD SqrtFF(float fValue)
{
	float result = static_cast<float>(std::sqrt(fValue));
	return *((DWORD*)&result);
}

DARKSDK DWORD ExpFF(float fExp)
{
	float result = static_cast<float>(std::exp(fExp));
	return *((DWORD*)&result);
}

DB_EXPORT dbReturnFloat_t SignF(float a) {
	return dbReturnFloat( db3::Sign( a ) );
}
DB_EXPORT dbReturnFloat_t CopySign(float a, float b) {
	return dbReturnFloat( db3::CopySign( a, b ) );
}

DB_EXPORT int FloatToIntFast(float x) {
	return db3::FloatToIntFast( x );
}
DB_EXPORT DWORD FloatToDwordFast(float x) {
	return db3::FloatToUIntFast( x );
}

DB_EXPORT dbReturnFloat_t SqrtFast(float x) {
	return dbReturnFloat( db3::SqrtFast( x ) );
}
DB_EXPORT dbReturnFloat_t InvSqrtFast(float x) {
	return dbReturnFloat( db3::InvSqrtFast( x ) );
}

DB_EXPORT dbReturnFloat_t Lerp(float x, float y, float t) {
	return dbReturnFloat( db3::Lerp(x, y, t) );
}
	
DARKSDK void Randomize(int iSeed)
{
	srand(iSeed);
}

/* LEEFIX = 121102 - Should return a float A=RND(20)+B should make INT result
DARKDLL DWORD RndFL(int r)
{
	float result=0.0f;
	if(r>0) result = (float)(rand()%(r+1));
	return *((DWORD*)&result);
}
*/
DARKSDK int RndLL(int r)
{
	int result=0;
	if(r>0)
	{
		// leefix - 250604 - u54 - 0 to 22 million now
		if ( r>1000 )  result += (rand()*1000);
		if ( r>100 ) result += (rand()*100);
		if ( r>10 ) result += (rand()*10);
		result += rand();
		result %= r+1;
	}
	return result;
}

// New MATH FUNCTIONS

DARKSDK DWORD CeilFF(float x)
{
	float value = ceilf ( x );
	return *((DWORD*)&value);
}

DARKSDK DWORD FloorFF(float x)
{
	float value = floorf ( x );
	return *((DWORD*)&value);
}

// 3D MATH EXPRESSIONS

DARKSDK float wrapangleoffset(float da)
{
	// leefix - 250604 - u54 - resolve LARGE wrap input values
	// leefix - 090704 - u55 - and minus wrap fix too
	/*
	int iChunkOut = (int)da;
	iChunkOut = iChunkOut - (iChunkOut % 360);
	da = da - iChunkOut;
	int breakout=10000;
	while(da<0.0f || da>=360.0f)
	{
		if(da<0.0f) da=da+360.0f;
		if(da>=360.0f) da=da-360.0f;
		breakout--;
		if(breakout==0) break;
	}
	if(breakout==0) da=0.0f;
	return da;
	*/
	// aaron - 20120811 - Faster version from NormalizeAngle360
	return db3::NormalizeAngle360(da);
}

DARKSDK DWORD CurveValueFFFF(float a, float da, float sp)
{
	if(sp<1.0f) sp=1.0f;
	float diff = a-da;
	da=da+(diff/sp);
	return *((DWORD*)&da);
}

DARKSDK DWORD WrapValueFF(float da)
{
	da = wrapangleoffset(da);
	return *((DWORD*)&da);
}

DARKSDK DWORD NewXValueFFFF(float x, float a, float b)
{
	float da = x + ((float)sin(D3DXToRadian(a))*b);
	return *((DWORD*)&da);
}

DARKSDK DWORD NewZValueFFFF(float z, float a, float b)
{
	float da = z + ((float)cos(D3DXToRadian(a))*b);
	return *((DWORD*)&da);
}

DARKSDK DWORD NewYValueFFFF(float y, float a, float b)
{
	float da = y - ((float)sin(D3DXToRadian(a))*b);
	return *((DWORD*)&da);
}

DARKSDK DWORD CurveAngleFFFF(float a, float da, float sp)
{
	if(sp<1.0f) sp=1.0f;
	a = wrapangleoffset(a);
	da = wrapangleoffset(da);
	float diff = a-da;
	if(diff<-180.0f) diff=(a+360.0f)-da;
	if(diff>180.0f) diff=a-(da+360.0f);
	da=da+(diff/sp);
	da = wrapangleoffset(da);
	return *((DWORD*)&da);
}

DB_EXPORT int NextPowerOfTwo1(int x) {
	return db3::NextPowerOfTwo(x);
}
DB_EXPORT int NextPowerOfTwo2(int x, int y) {
	return db3::NextSquarePowerOfTwo(x, y);
}

DB_EXPORT dbReturnFloat_t Clamp(float x, float l, float h) {
	return dbReturnFloat( db3::Clamp(x, l,h) );
}
DB_EXPORT dbReturnFloat_t ClampSNorm(float x) {
	return dbReturnFloat( db3::ClampSNorm(x) );
}
DB_EXPORT dbReturnFloat_t ClampUNorm(float x) {
	return dbReturnFloat( db3::ClampUNorm(x) );
}

DB_EXPORT dbReturnFloat_t Min(float x, float y) {
	return dbReturnFloat( db3::Min(x, y) );
}
DB_EXPORT dbReturnFloat_t Max(float x, float y) {
	return dbReturnFloat( db3::Max(x, y) );
}


// MISCLANIOUS CORE COMMANDS

DARKSDK int TimerL(void)
{
	// leefix - 230606 - u62 - timeBeginPeriod/timeEndPeriod added
	timeBeginPeriod(1);
	DWORD dwTimer = timeGetTime();
	timeEndPeriod(1);
	// davefix - 240615 - casting from dword to int can cause the number to be negative
	int iTimer = (int)dwTimer;
	// if negative, turn that frown upside down
	if ( iTimer < 0 ) iTimer = INT_MAX-iTimer;
	return iTimer;
}

DARKSDK void SleepL(int iDelay)
{
	DWORD dwTimeNow=timeGetTime();
	while(timeGetTime()<=dwTimeNow+iDelay)
	{
		if(InternalProcessMessages()==1) break;
	}
}

DARKSDK void WaitL(int iDelay)
{
	DWORD dwTimeNow=timeGetTime();
	while(timeGetTime()<=dwTimeNow+iDelay)
	{
		if(InternalProcessMessages()==1) break;
	}
}

DARKSDK void MemorySnapshot(int iMode)
{
	// Ask MM system to make snapshot of core
	#ifdef  __USE_MEMORY_MANAGER__
	mm_SnapShot();
	#endif

	// also go through all DLLs in DBP and ask them to make a snapshot prior to this report
	for ( int iDLL=0; iDLL<1; iDLL++ )
	{
		HINSTANCE hThis = NULL;
		if ( iDLL==0 ) hThis = g_Glob.g_Basic3D;
		if ( hThis )
		{
			typedef void ( *MM_SNAPSHOT ) ( void );
			MM_SNAPSHOT pSnapShotFunc;
			pSnapShotFunc = ( MM_SNAPSHOT ) GetProcAddress ( hThis, "?mm_SnapShot@@YAXXZ" );
			if ( pSnapShotFunc ) pSnapShotFunc();
		}
	}
	//g_Glob.g_Text;
	//g_Glob.g_Basic2D;
	//g_Glob.g_Sprites;
	//g_Glob.g_Image;
	//g_Glob.g_Input;
	//g_Glob.g_System;
	//g_Glob.g_File;
	//g_Glob.g_FTP;
	//g_Glob.g_Memblocks;
	//g_Glob.g_Bitmap;
	//g_Glob.g_Animation;
	//g_Glob.g_Multiplayer;
	//g_Glob.g_Camera3D;
	//g_Glob.g_Matrix3D;
	//g_Glob.g_Light3D;
	//g_Glob.g_World3D;
	//g_Glob.g_Particles;
	//g_Glob.g_PrimObject;
	//g_Glob.g_Vectors;
	//g_Glob.g_XObject;
	//g_Glob.g_3DSObject;
	//g_Glob.g_MDLObject;
	//g_Glob.g_MD2Object;
	//g_Glob.g_MD3Object;
	//g_Glob.g_Sound;
	//g_Glob.g_Music;
	//g_Glob.g_LODTerrain;
	//g_Glob.g_Q2BSP;
	//g_Glob.g_OwnBSP;
	//g_Glob.g_BSPCompiler;
	//g_Glob.g_CSG;
	//g_Glob.g_igLoader;
	//g_Glob.g_GameFX;
	//g_Glob.g_Transforms;

	// then produce a unified report
	#ifdef  __USE_MEMORY_MANAGER__
	mm_Report();
	#endif
}

DARKSDK void WaitForKey(void)
{
	while(g_wWinKey!=0)
	{
		if(InternalProcessMessages()==1) break;
	}
	while(g_wWinKey==0)
	{
		if(InternalProcessMessages()==1) break;
	}
}

DARKSDK void WaitForMouse(void)
{
	while(g_Glob.iWindowsMouseClick!=0)
	{
		if(InternalProcessMessages()==1) break;
	}
	while(g_Glob.iWindowsMouseClick==0)
	{
		if(InternalProcessMessages()==1) break;
	}
}

DARKSDK DWORD_PTR Cl$(DWORD_PTR pDestStr)
{
	// get command line from main program...
	char* lpNewStr = nullptr;
	if (g_pCommandLineString)
	{
		size_t len = strlen(g_pCommandLineString);
		lpNewStr = AllocateDynamicString(len);
		strcpy_s(lpNewStr, len + 1, g_pCommandLineString);
	}
	else
	{
		lpNewStr = AllocateDynamicString(0);
	}
	if (pDestStr) FreeDynamicString(reinterpret_cast<void*>(pDestStr));
	return reinterpret_cast<DWORD_PTR>(lpNewStr);
}

DARKSDK DWORD_PTR GetDate$(DWORD_PTR pDestStr)
{
	char buf[256];
	_strdate(buf);
	return reinterpret_cast<DWORD_PTR>(dbReturnString(reinterpret_cast<char *>(pDestStr), buf));
}

DARKSDK DWORD_PTR GetTime$(DWORD_PTR pDestStr)
{
	char buf[256];
	_strtime(buf);
	return reinterpret_cast<DWORD_PTR>(dbReturnString(reinterpret_cast<char *>(pDestStr), buf));
}

DARKSDK DWORD_PTR InkeyS(DWORD_PTR pDestStr)
{
	char buf[2];

	buf[0] = g_cInkeyCodeKey;
	buf[1] = '\0';

	return reinterpret_cast<DWORD_PTR>(dbReturnString(reinterpret_cast<char *>(pDestStr), buf));
}

DARKSDK void SyncOn(void)
{
	g_bSyncOff = false;
	g_bProcessorFriendly = false;
	g_bCanRenderNow = false;
}

DARKSDK void SyncOff(void)
{
	g_bSyncOff = true;
	g_bProcessorFriendly = true;
	g_bCanRenderNow = true;
}

DARKSDK void Sync(void)
{
	ExternalDisplaySync(0);
	ProcessMessagesOnly();
	ConstantNonDisplayUpdate();
	g_bCanRenderNow = true;
}

DARKSDK void Sync(int iProcessMessages)
{
	ExternalDisplaySync(0);
	if ( iProcessMessages==1 ) ProcessMessagesOnly();
	ConstantNonDisplayUpdate();
	g_bCanRenderNow = true;
}

DARKSDK void FastSync(void)
{
	ExternalDisplaySync(1);
	//V111 - 110608 - no need for this as FASTSYNC just used to render cameras (main SYNC handles proper update per cycle)
	//ConstantNonDisplayUpdate(); // lee - 100208 - moved from ConstantNonDisplayUpdate(), added by Mike in 2005
	g_bCanRenderNow = true;
}

DARKSDK void FastSync ( int iNonDisplayUpdates )
{
	ExternalDisplaySync(1);
	if ( iNonDisplayUpdates==1 )
	{
		// leeadd - 061108 - reinstated for U71 by request under parameter
		ConstantNonDisplayUpdate();
	}
	g_bCanRenderNow = true;
}

DARKSDK void SyncRate(int iRate)
{
	// Reset everything to run full speed
	SAFE_DELETE_ARRAY( g_pdwSyncRateSetting);
	g_dwSyncRateSettingSize = 0;
	g_dwManualSuperStepSetting = 0;

	// Zero is full speed
	// Anything over 1000 can't be measured, so treat that as full speed too
	if (iRate == 0 || iRate > 1000)
		return;

	// Negative is super stepping mode
	if (iRate < 0)
	{
		g_dwManualSuperStepSetting = abs(iRate);
		return;
	}

	// What's left can be dealt with.
	// Generate a table that covers 1 second of frames and fill it out with
	// the basic MS-per-frame value. Any milliseconds dropped using the calculation
	// will be evenly distributed within the table.
	g_dwSyncRateSettingSize = iRate;
	g_pdwSyncRateSetting = new uint32_t[ iRate ];

	DWORD RoundedMS                 =   1000 / iRate;
	DWORD DroppedTotalMS            =   1000 - (RoundedMS * iRate);
	float DroppedPerFrameMS         =   (float)(DroppedTotalMS) / iRate;
	float AccumulatedDroppedMS      =   0.0;

	for (int i = 0; i < iRate; ++i)
	{
		if (AccumulatedDroppedMS >= 1.0)
		{
			g_pdwSyncRateSetting[i] = RoundedMS + 1;
			AccumulatedDroppedMS -= 1.0;
			--DroppedTotalMS;
		}
		else
		{
			g_pdwSyncRateSetting[i] = RoundedMS;
		}
		AccumulatedDroppedMS += DroppedPerFrameMS;
	}

	// Any further dropped milliseconds, just use them against entries that haven't
	// already had them added previously until they are all used up.
	// This needed because of float (in)accuracy.
	for (int i = 0; i < iRate && DroppedTotalMS > 0; ++i)
	{
		if (g_pdwSyncRateSetting[i] == RoundedMS)
		{
			++g_pdwSyncRateSetting[i];
			--DroppedTotalMS;
		}
	}
}

DWORD GetNextSyncDelay()
{
	// If there is no table, then run return a 'no delay'
	if (g_dwSyncRateSettingSize == 0)
		return 0;

	// Advance the index, reset to start if gone beyond the end of the table
	++g_dwSyncRateCurrent;
	if (g_dwSyncRateCurrent >= g_dwSyncRateSettingSize)
		g_dwSyncRateCurrent = 0;

	// Return the current delay
	return g_pdwSyncRateSetting[ g_dwSyncRateCurrent ];
}

DARKSDK void SyncDisableQuad(void)
{
	g_bDrawQuadInSync = false;
}

DARKSDK void SyncEnableQuad(void)
{
	g_bDrawQuadInSync = true;
}

DARKSDK void SyncRenderQuad(void)
{
	if(g_Basic3D_RenderQuad)
	{
		if(g_Basic3D_RenderQuad(0)==1)
		{
			g_Camera3D_RunCode ( 0 );
			g_Basic3D_RenderQuad(1);
		}
	}
}

DARKSDK void DrawToBack(void)
{
	g_bDrawAutoStuffFirst = false;
}

DARKSDK void DrawToFront(void)
{
	g_bDrawAutoStuffFirst = true;
}

DARKSDK void DrawToCamera(void)
{
	g_bDrawEntirelyToCamera = true;
}

DARKSDK void DrawToScreen(void)
{
	g_bDrawEntirelyToCamera = false;
}

DARKSDK void DrawSpritesFirst(void)
{
	g_bDrawSpritesFirst=true;
}

DARKSDK void DrawSpritesLast(void)
{
	g_bDrawSpritesFirst=false;
}

DARKSDK void SaveArray(LPSTR pFilename, DWORD_PTR dwAllocation)
{
	DWORD written = 0;
	if (dwAllocation && IsValidArrayHandle(dwAllocation))
	{
		auto* pHeader = GetArrayHeader(dwAllocation);
		DWORD dwSizeOfArray = pHeader->size;
		DWORD dwElementSize = pHeader->itemSize;
		DWORD dwExistingElementType = pHeader->typeId;

		if (dwExistingElementType < 9)
		{
			ScopedFileHandle file(CreateFile(pFilename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
			if (file.IsValid())
			{
				char* pData = GetArrayDataPtr(pHeader);

				if (dwExistingElementType == 2)
				{
					for (DWORD n = 0; n < dwSizeOfArray; n++)
					{
						char** ppStr = reinterpret_cast<char**>(pData + n * sizeof(char*));
						LPSTR pStr = *ppStr;
						DWORD dwStringSize = 0;
						if (pStr && IsValidStringPointer(reinterpret_cast<DWORD_PTR>(pStr)))
							dwStringSize = static_cast<DWORD>(strlen(pStr));
						if (dwStringSize > 0)
							WriteFile(file, pStr, dwStringSize, &written, FALSE);

						char CR[2] = { 13, 10 };
						WriteFile(file, CR, 2, &written, FALSE);
					}
				}
				else
				{
					WriteFile(file, &dwExistingElementType, 4, &written, FALSE);
					WriteFile(file, &dwSizeOfArray, 4, &written, FALSE);

					for (DWORD n = 0; n < dwSizeOfArray; n++)
					{
						int indexn = static_cast<int>(n);
						WriteFile(file, &indexn, 4, &written, FALSE);
						WriteFile(file, pData + n * dwElementSize, dwElementSize, &written, FALSE);
					}

					int endn = -1;
					WriteFile(file, &endn, 4, &written, FALSE);
				}
			}
			else
			{
				char pErrStr[1024];
				sprintf_s(pErrStr, "Failed to CreateFile with: %s", pFilename ? pFilename : "<null>");
				Message(0, pErrStr, "");
				RunTimeError(RUNTIMEERROR_INVALIDFILE);
			}
		}
		else
		{
			RunTimeError(RUNTIMEERROR_ARRAYTYPEINVALID);
		}
	}
}

DARKSDK void LoadArrayCore(LPSTR pFilename, DWORD_PTR dwAllocation)
{
	DWORD readen = 0;
	if (dwAllocation && IsValidArrayHandle(dwAllocation))
	{
		auto* pHeader = GetArrayHeader(dwAllocation);
		DWORD dwExistingSizeOfArray = pHeader->size;
		DWORD dwElementSize = pHeader->itemSize;
		DWORD dwExistingElementType = pHeader->typeId;

		if (dwExistingElementType < 9)
		{
			ScopedFileHandle file(CreateFile(pFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
			if (file.IsValid())
			{
				char* pData = GetArrayDataPtr(pHeader);

				if (dwExistingElementType == 2)
				{
					DWORD dwDataSize = GetFileSize(file, nullptr);
					if (dwDataSize != INVALID_FILE_SIZE && dwDataSize > 0 && dwDataSize < 0x20000000u)
					{
						auto pFileData = std::make_unique<char[]>(static_cast<size_t>(dwDataSize) + 2);
						if (ReadFile(file, pFileData.get(), dwDataSize, &readen, FALSE) && readen > 0)
						{
							pFileData[readen] = 0;
							pFileData[readen + 1] = 0;

							TextLineCursor lineCursor;
							TextLineCursorInit(&lineCursor, pFileData.get(), readen);
							int arrindex = 0;
							const char* pLineStart = nullptr;
							DWORD dwStringSize = 0;
							while (arrindex < static_cast<int>(dwExistingSizeOfArray) && TextLineCursorNext(&lineCursor, &pLineStart, &dwStringSize))
							{
								if (dwStringSize < 0x10000000u)
								{
									char** ppStr = reinterpret_cast<char**>(pData + arrindex * sizeof(char*));
									if (*ppStr && IsDynamicHeapString(*ppStr)) FreeDynamicString(*ppStr);
									*ppStr = nullptr;
									char* pNewStr = AllocateDynamicString(dwStringSize);
									if (pLineStart && dwStringSize > 0)
										memcpy(pNewStr, pLineStart, dwStringSize);
									pNewStr[dwStringSize] = 0;
									*ppStr = pNewStr;
								}
								arrindex++;
							}
						}
					}
				}
				else
				{
					DWORD dwElementType = 0;
					ReadFile(file, &dwElementType, 4, &readen, FALSE);
					DWORD dwSizeOfArray = 0;
					ReadFile(file, &dwSizeOfArray, 4, &readen, FALSE);

					if (dwElementType == dwExistingElementType && dwSizeOfArray == dwExistingSizeOfArray)
					{
						DWORD dwDataBlockSizeInBytes = dwSizeOfArray * dwElementSize;
						ZeroMemory(pData, dwDataBlockSizeInBytes);

						int arrindex = 0;
						ReadFile(file, &arrindex, 4, &readen, FALSE);
						while (arrindex != -1 && arrindex >= 0 && arrindex < static_cast<int>(dwExistingSizeOfArray))
						{
							ReadFile(file, pData + arrindex * dwElementSize, dwElementSize, &readen, FALSE);
							ReadFile(file, &arrindex, 4, &readen, FALSE);
						}
					}
				}
			}
			else
			{
				RunTimeError(RUNTIMEERROR_FILENOTEXIST);
			}
		}
		else
		{
			RunTimeError(RUNTIMEERROR_ARRAYTYPEINVALID);
		}
	}
}

DARKSDK void LoadArray( LPSTR szFilename, DWORD_PTR dwAllocation )
{

	// Uses actual or virtual file..
	char VirtualFilename[_MAX_PATH];
	strcpy(VirtualFilename, szFilename);
	g_pGlob->UpdateFilenameFromVirtualTable( VirtualFilename );

	CheckForWorkshopFile ( VirtualFilename );

	// Decrypt and use media, re-encrypt
	g_pGlob->Decrypt( VirtualFilename );
	LoadArrayCore ( VirtualFilename, dwAllocation );
	g_pGlob->Encrypt( VirtualFilename );
}

//
// DX Detect Check (from globstruct filled in DarkEXE)
//

DARKSDK DWORD_PTR GetDXVer$(DWORD_PTR pDestStr)
{
	char buf[256];

	buf[0] = '\0';

	if (g_pGlob && g_pGlob->lpDirectXVersionString)
	{
		if (g_pGlob->lpDirectXVersionString[ 0 ])
		{
#if __STDC_WANT_SECURE_LIB__
			strcpy_s(buf, sizeof(buf), g_pGlob->lpDirectXVersionString);
#else
			strncpy(buf, sizeof(buf) - 1, g_pGlob->lpDirectXVersionString);
			buf[sizeof(buf) - 1] = '\0';
#endif
		}
	}

	if (!buf[0])
	{
		// Could get the DX version the 'proper' way using the DXDiag library,
		// except that it always fetches the latest version of DX on the system,
		// ie 10 on Vista, 11 on Windows 7.
		// Luckily those don't overwrite the DX version held in the registry so
		// we can collect the value from there instead.
		// If the exe is running in compatibility mode for XP or prior, then this
		// will never be executed anyway.
		// (TBH, I don't really understand why we even bother as if the required
		//  version is not installed, we won't be able to run the exe anyway...)

		HKEY Key;
		LONG Status;

		Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\DirectX", NULL, KEY_READ, &Key);
		if (Status == ERROR_SUCCESS)
		{
			DWORD DataType = REG_SZ, DataSize = sizeof(buf);

			Status = RegQueryValueEx( Key, "Version", NULL, &DataType, (LPBYTE)buf, &DataSize );
			if (Status != ERROR_SUCCESS)
				buf[0] = 0;
			else
			{
				// Please excuse this rather nasty bodge...
				if (strncmp(buf, "4.09.00.0904", 12) == 0)
					strcpy(buf, "9.0c");
				else if (strncmp(buf, "4.09.00.0903", 11) == 0)
				{
					char Letter = buf[11] - '1' + 'a';
					strcpy(buf, "9.0?");
					buf[3] = Letter;
				}
				else
					buf[0] = 0;
			}

			RegCloseKey( Key );
		}
	}

	if (!buf[0])
		strcpy( buf, "DirectX 9.0 not installed" );

	return reinterpret_cast<DWORD_PTR>(dbReturnString(reinterpret_cast<char *>(pDestStr), buf));
}

//
// Suspend App - used when multiple apps running, want to shut one down
//

DARKSDK void AlwaysActiveOff ( void )
{
	// Will shut down all 3D, sound and music processing (plus any secondary monitoring)
	// Will keep input and general program execution naturally
	g_bAlwaysActiveOff = true;
	g_bAlwaysActiveOneOff = false;
}

DARKSDK void AlwaysActiveOn ( void )
{
	// Restores systems previously shutdown with AlwaysActiveOff
	g_bAlwaysActiveOff = false;
}

DARKSDK void EarlyEnd ( void )
{
	// Report an error
	MessageBox ( NULL, "You have hit a FUNCTION declaration mid-program!", "Early Exit Error", MB_OK );
}

DARKSDK void SyncSleep ( int iFlag )
{
	// controls process friendly flag
	if ( iFlag==1 ) 
		g_bProcessorFriendly = true;
	else
		g_bProcessorFriendly = false;
}

DARKSDK void SyncMask ( DWORD dwMask )
{
	// copy to master sync mask
	g_dwSyncMask = dwMask;
}

DARKSDK DWORD GetArrayType(DWORD_PTR dwArrayPtr)
{
	// return array type index
	if(dwArrayPtr)
	{
		DWORD dwTypeIndex = (*((DWORD*)dwArrayPtr-2));
		return dwTypeIndex;
	}
	else
		return 0;
}

LPSTR GetTypePatternCore ( LPSTR dwTypeName, DWORD dwTypeIndex )
{
	// U73 - 210309 - if basic string, return simple STRING pattern
	if ( dwTypeIndex==2 )
	{
		LPSTR pSimplePattern = new char[2];
		strcpy ( pSimplePattern, "S" );
		return pSimplePattern;
	}

	// U73 - 210309 - if no structures, exit now as rest is structure type stuff only
	if ( g_dwStructPatternQty==0 || !g_pStructPatternsPtr )
		return NULL;

	// look for type that matches name
	DWORD dwPatternDataBeginsAt = 0;
	if ( dwTypeName )
	{
		size_t nameLen = strlen((LPSTR)dwTypeName);
		LPSTR pFindName = new char[nameLen + 2];
		strcpy ( pFindName, (LPSTR)dwTypeName );
		strcat ( pFindName, ":" );
		size_t dwFindLength = strlen ( pFindName );
		if (g_dwStructPatternQty >= dwFindLength)
		{
			for ( DWORD dwI=0; dwI<=g_dwStructPatternQty-dwFindLength; dwI++ )
			{
				if ( strnicmp ( g_pStructPatternsPtr+dwI, pFindName, dwFindLength )==NULL )
				{
					dwPatternDataBeginsAt = static_cast<DWORD>( dwI+dwFindLength );
					break;
				}
			}
		}
		delete[] pFindName;
	}
	if ( dwTypeIndex>0 && dwPatternDataBeginsAt == 0 )
	{
		LPSTR pFindName = new char[g_dwStructPatternQty+32];
		snprintf ( pFindName, g_dwStructPatternQty+32, ":%lu:", static_cast<unsigned long>(dwTypeIndex) );
		size_t dwFindLength = strlen ( pFindName );
		if (g_dwStructPatternQty >= dwFindLength)
		{
			for ( DWORD dwI=0; dwI<=g_dwStructPatternQty-dwFindLength; dwI++ )
			{
				if ( strnicmp ( g_pStructPatternsPtr+dwI, pFindName, dwFindLength )==NULL )
				{
					dwPatternDataBeginsAt = static_cast<DWORD>( dwI+dwFindLength );
					break;
				}
			}
		}
		delete[] pFindName;
	}

	// copy pattern to return string, or null
	size_t structLen = strlen(g_pStructPatternsPtr);
	if ( dwPatternDataBeginsAt > structLen ) dwPatternDataBeginsAt = static_cast<DWORD>(structLen);
	size_t patternBufSize = (structLen - dwPatternDataBeginsAt) + 1;
	LPSTR lpNewStr = new char[patternBufSize]();
	if ( dwPatternDataBeginsAt > 0 )
	{
		// get type index, then go to get pattern
		if ( dwTypeName )
		{
			LPSTR lpNum = new char[patternBufSize]();
			LPSTR pSourceStr = g_pStructPatternsPtr + dwPatternDataBeginsAt;
			strcpy_s ( lpNum, patternBufSize, pSourceStr );
			size_t dwI = 0;
			for (; dwI < strlen(pSourceStr); dwI++ )
			{
				if ( lpNum[dwI] == ':' )
				{
					lpNum[dwI] = 0;
					break;
				}
			}
			delete[] lpNum;
			dwPatternDataBeginsAt += static_cast<DWORD>(dwI + 1);
		}

		// get pattern, then cut off at : colon
		if (dwPatternDataBeginsAt < structLen)
		{
			LPSTR pSourceStr = g_pStructPatternsPtr + dwPatternDataBeginsAt;
			strcpy_s ( lpNewStr, patternBufSize, pSourceStr );
			for ( size_t dwI = 0; dwI < strlen(pSourceStr); dwI++ )
			{
				if ( lpNewStr[dwI] == ':' )
				{
					lpNewStr[dwI] = 0;
					break;
				}
			}
		}
	}

	// return pattern from type found
	return lpNewStr;
}

DARKSDK DWORD_PTR GetTypePattern$(DWORD_PTR pDestStr,DWORD_PTR dwTypeName,DWORD dwTypeIndex)
{
	DWORD_PTR r;

	// determine if type name string passed in has contents
	LPSTR pTypeName = NULL;
	if ( dwTypeName )
		if ( strlen ( (LPSTR)dwTypeName ) > 0 )
			pTypeName = (LPSTR)dwTypeName;

	// get pattern from type name
	LPSTR lpNewStr = GetTypePatternCore( pTypeName, dwTypeIndex );

	r = reinterpret_cast<DWORD_PTR>(dbReturnString(reinterpret_cast<char *>(pDestStr), lpNewStr));
	delete [] lpNewStr;

	return r;
}




/*
 *
 * Additions for plug-ins only
 *
 */

// Get/Set data pointers
DARKSDK void GetDataPointers(LPSTR* Start, LPSTR* End, LPSTR* Current)
{
	if (Start)      *Start      = g_pDataLabelStart;
	if (End)        *End        = g_pDataLabelEnd;
	if (Current)    *Current    = g_pDataLabelPtr;
}

DARKSDK void SetDataPointer(LPSTR Current)
{
	if (Current < g_pDataLabelStart)
		Current = g_pDataLabelStart;
	if (Current > g_pDataLabelEnd)
		Current = g_pDataLabelEnd;
	g_pDataLabelPtr = Current;
}

