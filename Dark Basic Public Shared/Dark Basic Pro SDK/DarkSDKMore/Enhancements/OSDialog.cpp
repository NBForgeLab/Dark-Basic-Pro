#include "stdafx.h"
#include <commdlg.h>
#include "core.h"
#include "Enchancements.h"
#include <cstdint>

#define DARKSDK	

DWORD_PTR OpenFileDialog ( DWORD_PTR dwReturn, DWORD_PTR dwDir, DWORD_PTR dwFilter, DWORD_PTR dwTitle )
{
	OPENFILENAMEA	ofn{};
	char			szFile [ 260 ] = {};
	
	ofn.lStructSize     = sizeof ( ofn );
	ofn.hwndOwner       = nullptr;
	ofn.lpstrFile       = szFile;
	ofn.nMaxFile        = sizeof ( szFile );
	ofn.lpstrFilter     = IsReadablePointer(dwFilter) ? reinterpret_cast<LPCSTR>(dwFilter) : nullptr;
	ofn.nFilterIndex    = 1;
	ofn.lpstrTitle      = IsReadablePointer(dwTitle) ? reinterpret_cast<LPCSTR>(dwTitle) : nullptr;
	ofn.nMaxFileTitle   = 0;
	ofn.lpstrInitialDir = IsReadablePointer(dwDir) ? reinterpret_cast<LPCSTR>(dwDir) : nullptr;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	
	GetOpenFileNameA ( &ofn );

	return reinterpret_cast<DWORD_PTR>( SetupString ( ofn.lpstrFile ) );
}

DWORD_PTR SaveFileDialog ( DWORD_PTR dwReturn, DWORD_PTR dwDir, DWORD_PTR dwFilter, DWORD_PTR dwTitle )
{
	OPENFILENAMEA	ofn{};
	char			szFile [ 260 ] = {};
	
	ofn.lStructSize     = sizeof ( ofn );
	ofn.hwndOwner       = nullptr;
	ofn.lpstrFile       = szFile;
	ofn.nMaxFile        = sizeof ( szFile );
	ofn.lpstrFilter     = IsReadablePointer(dwFilter) ? reinterpret_cast<LPCSTR>(dwFilter) : nullptr;
	ofn.nFilterIndex    = 1;
	ofn.lpstrTitle      = IsReadablePointer(dwTitle) ? reinterpret_cast<LPCSTR>(dwTitle) : nullptr;
	ofn.nMaxFileTitle   = 0;
	ofn.lpstrInitialDir = IsReadablePointer(dwDir) ? reinterpret_cast<LPCSTR>(dwDir) : nullptr;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	
	GetSaveFileNameA ( &ofn );

	return reinterpret_cast<DWORD_PTR>( SetupString ( ofn.lpstrFile ) );
}
