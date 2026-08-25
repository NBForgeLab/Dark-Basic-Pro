//
// DBDLLCore Header
//

#ifndef DBDLLCORE_H
#define DBDLLCORE_H

// Common Includes
#include <cstdint>
#include "windows.h"

// Custom Includes
#include "globstruct.h"
#include "..\error\cerror.h"
#include "macros.h"

// Additional CoreHeader Includes
#include "DBDLLExtCalls.h"

#include "..\global.h"

#include <DB3Math.h>

// mike - 100405
#define DBPRO_GLOBAL 

// Global Core Functions
extern DARKSDK void InternalClearWindowsEntry(void);
extern DARKSDK DWORD InternalProcessMessages();

// Global Internal Data
extern HWND				g_hWnd						;
extern char*			g_pVarSpace					;

// Global Performance Flag Vars
extern bool				g_bProcessorFriendly		;
extern bool				g_bUseExternalDisplayLayer	;

// Global Display Vars
extern HBITMAP			g_hDisplayBitmap			;
extern HDC				g_hdcDisplay				;
extern int				g_iX						;
extern int				g_iY						;
extern COLORREF			g_colFore					;
extern COLORREF			g_colBack					;
extern HBRUSH			g_hBrush					;
extern uint32_t			g_dwScreenWidth				;
extern uint32_t			g_dwScreenHeight			;
extern uint32_t			g_dwWindowWidth				;
extern uint32_t			g_dwWindowHeight			;

// Global Input Vars
extern uint32_t			g_dwWindowsTextEntrySize	;
extern char*			g_pWindowsTextEntry			;
extern uint32_t			g_dwWindowsTextEntryPos		;
extern uint8_t	g_cKeyPressed				;
extern int				g_iEntryCursorState			;

// Global Data Vars
extern char*			g_pDataLabelPtr				;
extern char*			g_pDataLabelEnd				;

#endif
