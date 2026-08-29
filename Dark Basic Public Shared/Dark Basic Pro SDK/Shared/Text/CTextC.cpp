#include "ctextc.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <bit>
#include <cctype>
#include <algorithm>
#include <string>
#include <string_view>
#include ".\..\error\cerror.h"
#include ".\..\core\globstruct.h"

#ifdef DARKSDK_COMPILE
	#include ".\..\..\..\DarkGDK\Code\Include\DarkSDKDisplay.h"
	//#include "cpositionc.cpp"
#endif

//////////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES //////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

typedef IDirect3DDevice9* ( *GFX_GetDirect3DDevicePFN ) ( void );

// main globals
DBPRO_GLOBAL LPDIRECT3D9				m_pDX								= nullptr;				// pointer to dx
DBPRO_GLOBAL LPDIRECT3DDEVICE9			m_pD3D								= nullptr;				// pointer to direct3d interface
DBPRO_GLOBAL D3DFORMAT					m_FontFormat						= D3DFMT_UNKNOWN;		// default format
DBPRO_GLOBAL TCHAR						m_strFontName [ 80 ]				= {};					// font name
DBPRO_GLOBAL uint32_t					m_dwFontHeight						= 0;					// font height
DBPRO_GLOBAL uint32_t					m_dwFontFlags						= 0;					// font flags
DBPRO_GLOBAL LPDIRECT3DTEXTURE9			m_pTexture							= nullptr;				// texture
DBPRO_GLOBAL LPDIRECT3DVERTEXBUFFER9	m_pVB								= nullptr;				// vertex buffer
DBPRO_GLOBAL uint32_t					m_dwTexWidth						= 0;					// texture width
DBPRO_GLOBAL uint32_t					m_dwTexHeight						= 0;					// texture height
DBPRO_GLOBAL FLOAT						m_fTextScale						= 0.0f;					// text scale
DBPRO_GLOBAL FLOAT						m_fTexCoords [ 256 - 32 ] [ 4 ]		= {};					// texture coods
DBPRO_GLOBAL int						m_szTexWidth [ 256 - 32 ]			= {};					// letter sizes
DBPRO_GLOBAL int						m_szTexHeight [ 256 - 32 ]			= {};					// letter sizes
DBPRO_GLOBAL IDirect3DStateBlock9*		m_pSavedStateBlock					= nullptr;				// dx8->dx9
DBPRO_GLOBAL IDirect3DStateBlock9*		m_pDrawTextStateBlock				= nullptr;				// state block
DBPRO_GLOBAL tagObjectPos*				m_pPosText							= nullptr;				// position pointer
DBPRO_GLOBAL bool						GDI_TEXT							= false;				// text type
DBPRO_GLOBAL PTR_FuncCreateStr			g_pCreateDeleteStringFunction		= nullptr;				// delete string
DBPRO_GLOBAL uint32_t					dwDEFAULTCHARSET					= ANSI_CHARSET;			// character set
DBPRO_GLOBAL HFONT						g_hRetainRawTextWriteFont			= nullptr;				// raw font
DBPRO_GLOBAL bool						g_bWideCharacterSet					= false;				// unicode

// font properties
DBPRO_GLOBAL uint32_t					m_dwColor							= 0;					// colour
DBPRO_GLOBAL uint32_t					m_dwBKColor							= 0;					// bk colour
DBPRO_GLOBAL bool						m_bTextBold							= false;				// bold flag
DBPRO_GLOBAL bool						m_bTextItalic						= false;				// italic flag
DBPRO_GLOBAL bool						m_bTextOpaque						= false;				// bold flag
DBPRO_GLOBAL int						m_iTextCharSet						= 0;					// text char set
DBPRO_GLOBAL int						m_iX								= 0;					// x pos
DBPRO_GLOBAL int						m_iY								= 0;					// y pos

// local checklist work vars
DBPRO_GLOBAL bool						g_bCreateChecklistNow				= false;				// create checklist
DBPRO_GLOBAL uint32_t					g_dwMaxStringSizeInEnum				= 0;					// maximum string size
DBPRO_GLOBAL uint32_t					m_dwWorkStringSize					= 0;					// string size
DBPRO_GLOBAL char*						m_pWorkString						= nullptr;				// work string
DBPRO_GLOBAL char*						m_szTokenString                     = nullptr;              // splitting by token
DBPRO_GLOBAL uint32_t					m_dwTokenStringSize                 = 0;

// function pointers
DBPRO_GLOBAL HINSTANCE					g_GFX								= nullptr;				// for dll loading
DBPRO_GLOBAL GFX_GetDirect3DDevicePFN	g_GFX_GetDirect3DDevice				= nullptr;				// get pointer to D3D device

DBPRO_GLOBAL GlobStruct*				g_pGlob								= nullptr;				// glob struct

//////////////////////////////////////////////////////////////////////////


DARKSDK bool UpdatePtr ( int iID )
{
	return true;
}

bool UpdateTextPtr ( int iID )
{
	return UpdatePtr ( iID );
}

DARKSDK void ValidateWorkStringBySize ( uint32_t dwSize )
{
	// free string that is too small
	if ( m_pWorkString )
	{
		if ( m_dwWorkStringSize < dwSize )
		{
			delete[] m_pWorkString;
			m_pWorkString = nullptr;
		}
	}

	// create new work string of good size
	if ( m_pWorkString == nullptr )
	{
		m_dwWorkStringSize = dwSize + 1;
		m_pWorkString = new char[m_dwWorkStringSize];
		memset ( m_pWorkString, 0, m_dwWorkStringSize );
	}
}

static inline bool IsValidStringPointer(const void* ptr)
{
	if (!ptr) return false;
	uintptr_t u = reinterpret_cast<uintptr_t>(ptr);
	return u > 0x10000 && u < 0x00007FFFFFFFFFFFULL;
}

static inline const char* SafeString(DWORD_PTR ptr)
{
	if (IsValidStringPointer(reinterpret_cast<const void*>(ptr)))
	{
		__try {
			const char* s = reinterpret_cast<const char*>(ptr);
			(void)strlen(s);
			return s;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return "";
		}
	}
	return "";
}

DARKSDK void ValidateWorkString(const char* pString)
{
	// Size from string
	if ( pString && IsValidStringPointer(pString) )
	{
		__try {
			ValidateWorkStringBySize ( static_cast<uint32_t>(strlen(pString) + 1) );
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}
}

//////////////////////////////////////////////////////////////////////////////////
// MODERN C++20 SUPPORT //////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

namespace dbp::text
{
    // Bounds-checked replacement for the unsafe strcpy/sprintf writes into the
    // legacy engine work buffer (m_pWorkString). Guarantees a null-terminated
    // string and never overruns the allocated capacity. ABI/behavior preserved.
    inline void set_work_string( std::string_view value ) noexcept
    {
        ValidateWorkStringBySize( static_cast<uint32_t>( value.size() + 1 ) );
        if ( m_pWorkString )
        {
            if ( !value.empty() )
                std::memcpy( m_pWorkString, value.data(), value.size() );
            m_pWorkString[ value.size() ] = '\0';
        }
    }

    // Single tiny scope-exit guard.
    template <typename F>
    struct ScopeGuard
    {
        F m_cleanup;
        explicit ScopeGuard( F cleanup ) noexcept : m_cleanup( cleanup ) {}
        ~ScopeGuard() noexcept { m_cleanup(); }
        ScopeGuard( const ScopeGuard& ) = delete;
        ScopeGuard& operator=( const ScopeGuard& ) = delete;
    };
    template <typename F> ScopeGuard( F ) -> ScopeGuard<F>;

    // Windows character-set identifiers. Enumerator values mirror the Win32
    // *_CHARSET macros exactly, so comparing against the engine's stored charset
    // integers (dwDEFAULTCHARSET / m_iTextCharSet) stays ABI-safe.
    enum class CharSet : uint32_t
    {
        Ansi        = ANSI_CHARSET,         // 0
        ShiftJis    = SHIFTJIS_CHARSET,     // 128
        Hangul      = HANGUL_CHARSET,       // 129
        Gb2312      = GB2312_CHARSET,       // 134
        ChineseBig5 = CHINESEBIG5_CHARSET,  // 136
    };

    // Packed text-style bits returned by TextStyle()/dbTextStyle().
    // Italic = 1, Bold = 2, matching the legacy bit layout exactly.
    enum class TextStyleFlag : int
    {
        None   = 0,
        Italic = 1,
        Bold   = 2,
    };

    // Tokenizer continuation state for FirstToken/NextToken. The engine drives
    // these commands single-threaded, so a single shared scan pointer matches
    // strtok's original contract without using the C-runtime's hidden global.
    static char* g_tokenCtx = nullptr;

    // strtok-equivalent tokenizer with explicit continuation state.
    static char* next_token( char* str, const char* delim )
    {
        if ( str ) g_tokenCtx = str;
        if ( !g_tokenCtx || !*g_tokenCtx ) { g_tokenCtx = nullptr; return nullptr; }

        // skip leading delimiters
        while ( *g_tokenCtx && std::strchr( delim, *g_tokenCtx ) ) ++g_tokenCtx;
        if ( !*g_tokenCtx ) { g_tokenCtx = nullptr; return nullptr; }

        char* start = g_tokenCtx;
        while ( *g_tokenCtx && !std::strchr( delim, *g_tokenCtx ) ) ++g_tokenCtx;
        if ( *g_tokenCtx ) { *g_tokenCtx = '\0'; ++g_tokenCtx; }
        else { g_tokenCtx = nullptr; }
        return start;
    }
}

DARKSDK void ValidateDefaultTextureForFont(void)
{
	// Would be a good idea to have a format-collector function for what surface, depth, texture formats we can use from the card and put into glob..
	// (taken form image DLL - perhaps merge them at some point in setup DLL)

	// Get default d3dformat from backbuffer
	D3DSURFACE_DESC backdesc;
	LPDIRECT3DSURFACE9 pBackBuffer = g_pGlob->pCurrentBitmapSurface;
	if(pBackBuffer) pBackBuffer->GetDesc(&backdesc);
	m_FontFormat = D3DFMT_A4R4G4B4;
	
	// Ensure textureformat is valid, else choose next valid..
	HRESULT hRes = m_pDX->CheckDeviceFormat(	D3DADAPTER_DEFAULT,
												D3DDEVTYPE_HAL,
												backdesc.Format,
												0, D3DRTYPE_TEXTURE,
												m_FontFormat);
	if ( FAILED( hRes ) )
	{
		// Need another texture format with an alpha
		for(DWORD t=0; t<12; t++)
		{
			switch(t)
			{
				case 0  : m_FontFormat = D3DFMT_A4R4G4B4;		break;
				case 1  : m_FontFormat = D3DFMT_A8R3G3B2;		break;
				case 2 : m_FontFormat = D3DFMT_A1R5G5B5;		break;
				case 3 : m_FontFormat = D3DFMT_X1R5G5B5;		break;
				case 4 : m_FontFormat = D3DFMT_A8R8G8B8;		break;
				case 5 : m_FontFormat = D3DFMT_A2B10G10R10;		break;
				case 6 : m_FontFormat = D3DFMT_X8R8G8B8;		break;
				case 7 : m_FontFormat = D3DFMT_R8G8B8;			break;
				case 8 : m_FontFormat = D3DFMT_R5G6B5;			break;
				case 9 : m_FontFormat = D3DFMT_R3G3B2;			break;
				case 10 : m_FontFormat = D3DFMT_X4R4G4B4;		break;
				case 11 : m_FontFormat = D3DFMT_G16R16;			break;
			}
			HRESULT hRes = m_pDX->CheckDeviceFormat(	D3DADAPTER_DEFAULT,
														D3DDEVTYPE_HAL,
														backdesc.Format,
														0, D3DRTYPE_TEXTURE,
														m_FontFormat);
			if ( SUCCEEDED( hRes ) )
			{
				// Found a texture we can use
				return;
			}
		}
	}
}

DARKSDK void TextConstructor ( HINSTANCE hSetup )
{
	#ifndef DARKSDK_COMPILE
	if ( !hSetup )
	{
		// attempt to load the DLLs manually
		hSetup = LoadLibrary ( "DBProSetupDebug.dll" );
	}
	#endif

	#ifndef DARKSDK_COMPILE
	{
		// Assign Setup DLL Handle
		g_GFX = hSetup;

		// check the dll was loaded
		if ( !g_GFX )
			Error ( "Cannot load setup library for text.dll" );
	}
	#endif

	// setup the function pointer and then call it to get a pointer to the direct3d interface
	#ifndef DARKSDK_COMPILE
	{
		g_GFX_GetDirect3DDevice = ( GFX_GetDirect3DDevicePFN ) GetProcAddress ( g_GFX, "?GetDirect3DDevice@@YAPEAUIDirect3DDevice9@@XZ" );
	}
	#else
	{
		g_GFX_GetDirect3DDevice = dbGetDirect3DDevice;
	}
	#endif

	m_pD3D = g_GFX_GetDirect3DDevice ? g_GFX_GetDirect3DDevice ( ) : nullptr;

	if ( m_pD3D )
	{
		// get reference to DX
		m_pD3D->GetDirect3D(&m_pDX);

		// create state blocks
		m_pD3D->CreateStateBlock  ( D3DSBT_ALL, &m_pSavedStateBlock );
		m_pD3D->CreateStateBlock  ( D3DSBT_ALL, &m_pDrawTextStateBlock );
	}

	m_bTextBold			   = false;									// turn bold off
	m_bTextItalic		   = false;									// turn italic off
   	m_bTextOpaque		   = false;									// text opaque
	m_dwColor			   = D3DCOLOR_ARGB ( 255, 255, 255, 255 );	// set colour to white
	m_dwBKColor			   = D3DCOLOR_ARGB ( 255, 0, 0, 0 );		// set colour to black
	m_iX			       = 0;										// initial x pos
	m_iY			       = 0;										// initial y pos

	// setup position
	delete m_pPosText;
	m_pPosText = new tagObjectPos();

	if ( !m_pPosText )
		Error ( "Unable to allocate memory for positional data in text library" );

	// now setup the font to be rendered - dbpro does it in passcore
	// leefix - 300305 - moved to passcore when we have the glob (do not do in DLL)
//	SetupFont   ( );
//	SetupStates ( );

	// prepare work string
	ValidateWorkStringBySize ( 256 );

    // u74b7 - if already have a token temp area, leave it alone.
    if (! m_szTokenString )
    {
        m_dwTokenStringSize = 100;
        m_szTokenString = new char[ 100 ];
		memset ( m_szTokenString, 0, m_dwTokenStringSize );
    }
}

DARKSDK int RgbR ( DWORD iRGB )
{
	return static_cast<int>((iRGB & 0x00FF0000) >> 16);
}
DARKSDK  int RgbG ( DWORD iRGB )
{
	return static_cast<int>((iRGB & 0x0000FF00) >> 8);
}
DARKSDK  int RgbB ( DWORD iRGB )
{
	return static_cast<int>((iRGB & 0x000000FF) );
}

DARKSDK int GetBitDepthFromFormat(D3DFORMAT Format)
{
	switch(Format)
	{
		case D3DFMT_R8G8B8 :		return 24;	break;
		case D3DFMT_A8R8G8B8 :		return 32;	break;
		case D3DFMT_X8R8G8B8 :		return 32;	break;
		case D3DFMT_R5G6B5 :		return 16;	break;
		case D3DFMT_X1R5G5B5 :		return 16;	break;
		case D3DFMT_A1R5G5B5 :		return 16;	break;
		case D3DFMT_A4R4G4B4 :		return 16;	break;
		case D3DFMT_A8	:			return 8;	break;
		case D3DFMT_R3G3B2 :		return 8;	break;
		case D3DFMT_A8R3G3B2 :		return 16;	break;
		case D3DFMT_X4R4G4B4 :		return 16;	break;
		case D3DFMT_A2B10G10R10 :	return 32;	break;
		case D3DFMT_G16R16 :		return 32;	break;
		case D3DFMT_A8P8 :			return 8;	break;
		case D3DFMT_P8 :			return 8;	break;
		case D3DFMT_L8 :			return 8;	break;
		case D3DFMT_A8L8 :			return 16;	break;
		case D3DFMT_A4L4 :			return 8;	break;
	}
	return 0;
}

// Realtext Functions
DARKSDK HFONT DB_SetRealTextFont(HDC hdc, DWORD textstyle, DWORD bItalicFlag, int fontsize, char* fontname, int inter)
{
	HFONT hFont = nullptr;
	if(inter==0)
	{
		// Get Real(Available) Font Height Used
		if(dwDEFAULTCHARSET==ANSI_CHARSET)
		{
			hFont = CreateFont( static_cast<int>(fontsize*m_fTextScale), 0, 0, 0, textstyle, bItalicFlag, false, false, dwDEFAULTCHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,VARIABLE_PITCH, fontname);
		}
		else
		{
			int FontHeight=0;
			FontHeight=-MulDiv(fontsize, static_cast<int>(GetDeviceCaps(hdc, LOGPIXELSY) * m_fTextScale), 72); 
			hFont = CreateFont( FontHeight, 0, 0, 0, textstyle, bItalicFlag, false, false, dwDEFAULTCHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,VARIABLE_PITCH, fontname);
			
		}
	}
	else
	{
		hFont = CreateFont(	fontsize, 0, 0, 0,
									textstyle, bItalicFlag, false, false,
									inter,
									OUT_DEFAULT_PRECIS,
									CLIP_DEFAULT_PRECIS,
									NONANTIALIASED_QUALITY,
									VARIABLE_PITCH,
									fontname);

		if(dwDEFAULTCHARSET!=static_cast<DWORD>(inter))
		{
			dwDEFAULTCHARSET=inter;
		}
	}

	// work out if wide character text (japanese, korean, chinese/trad)
	if ( dwDEFAULTCHARSET == static_cast<uint32_t>( dbp::text::CharSet::ShiftJis )    ||
	     dwDEFAULTCHARSET == static_cast<uint32_t>( dbp::text::CharSet::Hangul )     ||
	     dwDEFAULTCHARSET == static_cast<uint32_t>( dbp::text::CharSet::Gb2312 )     ||
	     dwDEFAULTCHARSET == static_cast<uint32_t>( dbp::text::CharSet::ChineseBig5 ) )
		g_bWideCharacterSet = true;
	else
		g_bWideCharacterSet = false;

	// return font handle
	return hFont;
}

DARKSDK void SetupFont ( void )
{
	if ( GDI_TEXT == false )
	{
		// variable definitions
		HRESULT		hr;				// used for error checking
		D3DCAPS9	d3dCaps;		// device capabilities structure
		
		DWORD*      pBitmapBits;	// pointer to bitmap data
		BITMAPINFO	bmi;			// bitmap info structure
		HFONT		hFont;			// handle to font (retained in g_hRetainRawTextWriteFont)

		DWORD		x = 0;
		DWORD		y = 0;
		TCHAR		str [ 2 ] = ( "x" );
		SIZE		size;

		D3DLOCKED_RECT	d3dlr;
		WORD*			pDst16;
		BYTE			bAlpha;

		// leefix - 300205 - texture plate size isdependant on the size of the font
		HDC hDC = CreateCompatibleDC ( nullptr );
		dbp::text::ScopeGuard hdcGuard( [hDC]{ if ( hDC ) ::DeleteDC( hDC ); } );
		SetMapMode ( hDC, MM_TEXT );																		// set mapping mode x, y
		DWORD textstyle		= ( m_bTextBold   ) ? FW_BOLD : FW_NORMAL;		// set bold flag on / off
		DWORD bItalicFlag	= ( m_bTextItalic ) ? true    : false;			// set italic flag on / off
		hFont = DB_SetRealTextFont ( hDC, textstyle, bItalicFlag, m_dwFontHeight, m_strFontName, m_iTextCharSet );
		HGDIOBJ hbmOldFont = ::SelectObject ( hDC, hFont );
		dbp::text::ScopeGuard fontGuard( [hDC, hbmOldFont]{ ::SelectObject ( hDC, hbmOldFont ); } );
		SetTextAlign ( hDC, TA_TOP );
		GetTextExtentPoint32 ( hDC, "g", 1, &size );

		// leefix - 300305 40, 20, X was not right (extended chars left 256 texture plate)
		DWORD dwActualUsageHeight = size.cy;
		if ( dwActualUsageHeight > 40 )
			m_dwTexWidth = m_dwTexHeight = 1024;
		else 
			if ( dwActualUsageHeight > 15 )
				m_dwTexWidth = m_dwTexHeight = 512;
			else
				m_dwTexWidth = m_dwTexHeight = 256;

		// now check that it's ok for us to create
		// a texture at the height and width, get
		// the device caps to do this
		m_pD3D->GetDeviceCaps ( &d3dCaps );

		// if the texture is too big we simply
		// create a smaller texture and scale
		// the final font
		m_fTextScale = 1.0f;
		if ( m_dwTexWidth > d3dCaps.MaxTextureWidth )
		{
			m_fTextScale = ( float ) d3dCaps.MaxTextureWidth / ( float ) m_dwTexWidth;
			m_dwTexWidth = m_dwTexHeight = d3dCaps.MaxTextureWidth;
		}

		// create a blank texture for the font
		D3DPOOL pool = D3DPOOL_MANAGED;
		DWORD usage = 0;
		if ( FAILED ( hr = m_pD3D->CreateTexture ( m_dwTexWidth, m_dwTexHeight, 1, usage, m_FontFormat, pool, &m_pTexture, nullptr ) ) )
		{
			// Fallback for Direct3D9Ex (which forbids D3DPOOL_MANAGED)
			pool = D3DPOOL_DEFAULT;
			usage = D3DUSAGE_DYNAMIC;
			if ( FAILED ( hr = m_pD3D->CreateTexture ( m_dwTexWidth, m_dwTexHeight, 1, usage, m_FontFormat, pool, &m_pTexture, nullptr ) ) )
			{
				pool = D3DPOOL_SYSTEMMEM;
				usage = 0;
				m_pD3D->CreateTexture ( m_dwTexWidth, m_dwTexHeight, 1, usage, m_FontFormat, pool, &m_pTexture, nullptr );
			}
		}
    
		// and now create a bitmap, what we do is draw
		// a font onto the bitmap and then later on
		// copy it onto the texture. by doing this
		// we draw loads of 3d objects at runtime 
		// instead of having to lock buffers and copy
		// text which is faster and enables us to have
		// 3d text
		memset ( &bmi.bmiHeader, 0, sizeof ( BITMAPINFOHEADER ) );		// clear out the header
		bmi.bmiHeader.biSize        = sizeof ( BITMAPINFOHEADER );		// set the size
		bmi.bmiHeader.biWidth       =  ( int ) m_dwTexWidth;			// set the bmp width
		bmi.bmiHeader.biHeight      = -( int ) m_dwTexHeight;			// set the bmp height
		bmi.bmiHeader.biPlanes      = 1;								// set planes ( always 1 )
		bmi.bmiHeader.biCompression = BI_RGB;							// no compression
		bmi.bmiHeader.biBitCount    = 32;								// bit depth

		// create a device context and a bitmap for the font
		HBITMAP hbmBitmap = CreateDIBSection ( hDC, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pBitmapBits), nullptr, 0 );
		dbp::text::ScopeGuard bmpGuard( [hbmBitmap]{ if ( hbmBitmap ) ::DeleteObject( hbmBitmap ); } );		// create bmp

		// create the font
		DWORD dwBold   = ( m_bTextBold   ) ? FW_BOLD : FW_NORMAL;		// set bold flag on / off
		DWORD dwItalic = ( m_bTextItalic ) ? TRUE    : FALSE;			// set italic flag on / off
		hFont = DB_SetRealTextFont ( hDC, dwBold, dwItalic, m_dwFontHeight, m_strFontName, m_iTextCharSet );

		// and select into device context
		HGDIOBJ hbmOldBitmap = SelectObject ( hDC, hbmBitmap );
		SelectObject ( hDC, hFont );

		// set character colours
		SetTextColor ( hDC, RGB ( 255, 255, 255 ) );	// text colour is always white
		SetBkColor   ( hDC, RGB ( 0, 0, 0 ) );			// background is black

		// loop through all printable character and output them to the bitmap
		// also keep track of the corresponding tex coords for each character
		// LEEFIX - 141102 - Increased font chars to 256 (to include special ascii chars)
		for ( unsigned short c = 32; c < 256; c++ )
		{
			str [ 0 ] = static_cast<unsigned char>(c);

			GetTextExtentPoint32 ( hDC, str, 1, &size );

			DWORD dwPush = 0;
			DWORD dwAdditional = 0;
			if(m_bTextItalic) 
			{
				dwPush = (size.cy/8);
				dwAdditional = (size.cy/4);//little edge of italic text

				// mike - 250604 - adjust for italics
				size.cx+=m_dwFontHeight / 3 + 5;
			}

			if ( ( DWORD ) ( x + size.cx + 1 ) > m_dwTexWidth )
			{
				x  = 0;
				y += size.cy + 2;
			}

			ExtTextOut ( hDC, x + dwPush + 0, y + 0, ETO_OPAQUE, nullptr, str, 1, nullptr );

			m_fTexCoords [ c - 32 ] [ 0 ] = ( ( float ) ( x + 0           ) ) / m_dwTexWidth;
			m_fTexCoords [ c - 32 ] [ 1 ] = ( ( float ) ( y + 0           ) ) / m_dwTexHeight;
			m_fTexCoords [ c - 32 ] [ 2 ] = ( ( float ) ( x + 0 + size.cx + 1 ) ) / m_dwTexWidth;
			m_fTexCoords [ c - 32 ] [ 3 ] = ( ( float ) ( y + 0 + size.cy + 1 ) ) / m_dwTexHeight;
			m_szTexWidth [ c - 32 ] = static_cast<int>(static_cast<float>(size.cx - dwAdditional) / m_fTextScale);
			m_szTexHeight [ c - 32 ] = size.cy;

			// mike - 250704 - use this for foreign language DB Pro text
			if ( m_iTextCharSet != 0 )
				x += size.cx + 2;
			else
				x += size.cx + 16;
		}

		// lock the surface (for 16bit format - 4444)
		DWORD dwDepth=GetBitDepthFromFormat(m_FontFormat);
		if(m_pTexture && dwDepth==16 && pBitmapBits)
		{
			DWORD dwUseBack = m_dwBKColor;
			if(m_bTextOpaque==false) dwUseBack=0;

			if ( SUCCEEDED( m_pTexture->LockRect ( 0, &d3dlr, 0, 0 ) ) && d3dlr.pBits )
			{
				pDst16 = ( uint16_t* ) d3dlr.pBits;
				uint16_t wFore = 0x0fff;
				uint16_t wBack = 0x0000;
				if(m_dwBKColor>0)
				{
					uint8_t bFR = static_cast<uint8_t>(RgbR(m_dwColor)/16); if(bFR>15) bFR=15;
					uint8_t bFG = static_cast<uint8_t>(RgbG(m_dwColor)/16); if(bFG>15) bFG=15;
					uint8_t bFB = static_cast<uint8_t>(RgbB(m_dwColor)/16); if(bFB>15) bFB=15;
					uint8_t bBR = static_cast<uint8_t>(RgbR(dwUseBack)/16); if(bFR>15) bFR=15;
					uint8_t bBG = static_cast<uint8_t>(RgbG(dwUseBack)/16); if(bBG>15) bBG=15;
					uint8_t bBB = static_cast<uint8_t>(RgbB(dwUseBack)/16); if(bBB>15) bBB=15;
					wFore = (bFR<<8) + (bFG<<4) + (bFB);
					wBack = (bBR<<8) + (bBG<<4) + (bBB);
				}
				for ( y = 0; y < m_dwTexHeight; y++ )
				{
					for ( x = 0; x < m_dwTexWidth; x++ )
					{
						bAlpha = ( BYTE ) ( ( pBitmapBits [ m_dwTexWidth * y + x ] & 0xff ) >> 4 );
						
						if ( bAlpha > 0 )
							*pDst16++ = ( bAlpha << 12 ) | wFore;
						else
							*pDst16++ = wBack;
					}
				}
				// unlock the texture
				m_pTexture->UnlockRect ( 0 );
			}
		}

		// select out objects (bitmap)
		SelectObject ( hDC, hbmOldBitmap );

		// store font, deleting any old one
		if ( g_hRetainRawTextWriteFont ) DeleteObject ( g_hRetainRawTextWriteFont );
		g_hRetainRawTextWriteFont = hFont;
	}

	if ( GDI_TEXT == true )
	{
		HFONT		hFont;			// handle to font (retained in g_hRetainRawTextWriteFont)
		SIZE		size;

		LPDIRECT3DSURFACE9 pBackBuffer = (g_pGlob != nullptr) ? g_pGlob->pCurrentBitmapSurface : nullptr;
		HDC hDC = nullptr;
		bool bReleaseSurfaceDC = false;
		if ( pBackBuffer != nullptr && SUCCEEDED( pBackBuffer->GetDC ( &hDC ) ) && hDC != nullptr )
		{
			bReleaseSurfaceDC = true;
		}
		else
		{
			hDC = CreateCompatibleDC ( nullptr );
		}

		if ( hDC != nullptr )
		{
			SetMapMode ( hDC, MM_TEXT );

			DWORD textstyle   = ( m_bTextBold   ) ? FW_BOLD : FW_NORMAL;		// set bold flag on / off
			DWORD bItalicFlag = ( m_bTextItalic ) ? true    : false;			// set italic flag on / off
			hFont = DB_SetRealTextFont ( hDC, textstyle, bItalicFlag, m_dwFontHeight, m_strFontName, m_iTextCharSet );

			SelectObject ( hDC, hFont );
			
			// store font, deleting any old one
			if ( g_hRetainRawTextWriteFont ) DeleteObject ( g_hRetainRawTextWriteFont );
			g_hRetainRawTextWriteFont = hFont;

			TCHAR str [ 2 ] = ( "x" );

			m_fTextScale = 1.0f;

			for ( unsigned short c = 32; c < 256; c++ )
			{
				str [ 0 ] = static_cast<unsigned char>(c);

				GetTextExtentPoint32 ( hDC, str, 1, &size );

				m_szTexWidth  [ c - 32 ] = static_cast<int>(static_cast<float>(size.cx) / m_fTextScale);
				m_szTexHeight [ c - 32 ] = m_dwFontHeight;
			}

			if ( bReleaseSurfaceDC && pBackBuffer )
			{
				pBackBuffer->ReleaseDC ( hDC );
			}
			else if ( !bReleaseSurfaceDC )
			{
				DeleteDC ( hDC );
			}
		}
	}
}

DARKSDK void SetErrorHandler ( LPVOID pErrorHandlerPtr )
{
	// Update error handler pointer
	g_pErrorHandler = (CRuntimeErrorHandler*)pErrorHandlerPtr;
}

DARKSDK void PassCoreData( LPVOID pGlobPtr )
{
	// Held in Core, used here..
	g_pGlob = (GlobStruct*)pGlobPtr;
	if ( !g_pGlob ) return;
	g_pCreateDeleteStringFunction = g_pGlob->CreateDeleteString;

	#ifndef DARKSDK_COMPILE
	if ( g_pGlob->g_GFX )
	{
		g_GFX = g_pGlob->g_GFX;
		g_GFX_GetDirect3DDevice = ( GFX_GetDirect3DDevicePFN ) GetProcAddress ( g_GFX, "?GetDirect3DDevice@@YAPEAUIDirect3DDevice9@@XZ" );
	}
	#else
	g_GFX_GetDirect3DDevice = dbGetDirect3DDevice;
	#endif

	if ( g_GFX_GetDirect3DDevice )
	{
		m_pD3D = g_GFX_GetDirect3DDevice();
		if ( m_pD3D && !m_pDX )
		{
			m_pD3D->GetDirect3D(&m_pDX);
		}
	}

	if ( m_pD3D )
	{
		// Choose best texture and create font and state now
		ValidateDefaultTextureForFont();
		SetupFont   ( );
		SetupStates ( );
	}
}

DARKSDK void DeleteFonts ( void )
{
	if ( m_pTexture )
	{
		if ( m_pTexture ) { m_pTexture->Release(); m_pTexture = nullptr; }
	}
}

DARKSDK void DeleteStates ( void )
{
	if ( m_pVB )
	{
		if ( m_pVB ) { m_pVB->Release(); m_pVB = nullptr; }
	}
}

DARKSDK void TextDestructor ( void )
{
	delete[] m_szTokenString;
	m_szTokenString = nullptr;
	delete[] m_pWorkString;
	m_pWorkString = nullptr;

	delete m_pPosText;
	m_pPosText = nullptr;
	DeleteFonts();
	DeleteStates();

	// delete widecharacter font
	if ( g_hRetainRawTextWriteFont )
	{
		DeleteObject ( g_hRetainRawTextWriteFont );
		g_hRetainRawTextWriteFont = nullptr;
	}

	// Release state blocks
	if ( m_pSavedStateBlock ) { m_pSavedStateBlock->Release(); m_pSavedStateBlock = nullptr; }
	if ( m_pDrawTextStateBlock ) { m_pDrawTextStateBlock->Release(); m_pDrawTextStateBlock = nullptr; }

	// Release references
	if ( m_pDX ) { m_pDX->Release(); m_pDX = nullptr; }
}

DARKSDK void RefreshD3D ( int iMode )
{
	if(iMode==0)
	{
		// Remove all traces of old D3D usage
		TextDestructor();
	}
	if(iMode==1)
	{
		// Get new D3D and recreate everything D3D related
		TextConstructor ( g_pGlob->g_GFX );
		PassCoreData ( g_pGlob );
	}
}

DARKSDK void SetTextColor ( int iAlpha, int iRed, int iGreen, int iBlue )
{
	m_dwColor = D3DCOLOR_ARGB ( iAlpha, iRed, iGreen, iBlue );
}

DARKSDK void SetupStates ( void )
{
	if ( !m_pD3D ) return;
	HRESULT hr;

    // create vertex buffer
    if ( FAILED ( hr = m_pD3D->CreateVertexBuffer ( MAX_NUM_VERTICES * sizeof ( FONT2DVERTEX ), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT, &m_pVB, nullptr ) ) )
        Error ( "Unable to create vertex buffer for text library" );
    
	// leefix-060803-release stateblocks
	if ( m_pSavedStateBlock ) { m_pSavedStateBlock->Release(); m_pSavedStateBlock = nullptr; }
	if ( m_pDrawTextStateBlock ) { m_pDrawTextStateBlock->Release(); m_pDrawTextStateBlock = nullptr; }

	// create the state blocks for rendering text
    for ( UINT which = 0; which < 2; which++ )
    {
        m_pD3D->BeginStateBlock ( );

        m_pD3D->SetTexture ( 0, m_pTexture );

		// Text can be transparent
		if(m_bTextOpaque==true)
		{
			m_pD3D->SetRenderState ( D3DRS_ALPHABLENDENABLE,			false );
		}
		else
		{
			m_pD3D->SetRenderState ( D3DRS_ALPHABLENDENABLE,			true );
		}
        m_pD3D->SetRenderState ( D3DRS_SRCBLEND,					D3DBLEND_SRCALPHA );
        m_pD3D->SetRenderState ( D3DRS_DESTBLEND,					D3DBLEND_INVSRCALPHA );
        m_pD3D->SetRenderState ( D3DRS_FILLMODE,					D3DFILL_SOLID );
        m_pD3D->SetRenderState ( D3DRS_CULLMODE,					D3DCULL_CCW );
        m_pD3D->SetRenderState ( D3DRS_ZENABLE,						false );
        m_pD3D->SetRenderState ( D3DRS_STENCILENABLE,				false );
        m_pD3D->SetRenderState ( D3DRS_CLIPPING,					true );
        m_pD3D->SetRenderState ( D3DRS_VERTEXBLEND,					false );
        m_pD3D->SetRenderState ( D3DRS_INDEXEDVERTEXBLENDENABLE,	false );
        m_pD3D->SetRenderState ( D3DRS_FOGENABLE,					false );

        m_pD3D->SetTextureStageState ( 0, D3DTSS_COLOROP,				D3DTOP_MODULATE );
        m_pD3D->SetTextureStageState ( 0, D3DTSS_COLORARG1,				D3DTA_TEXTURE );
        m_pD3D->SetTextureStageState ( 0, D3DTSS_COLORARG2,				D3DTA_DIFFUSE );
		m_pD3D->SetTextureStageState ( 0, D3DTSS_ALPHAOP,				D3DTOP_MODULATE );
		m_pD3D->SetTextureStageState ( 0, D3DTSS_ALPHAARG1,				D3DTA_TEXTURE );
		m_pD3D->SetTextureStageState ( 0, D3DTSS_ALPHAARG2,				D3DTA_DIFFUSE );

		m_pD3D->SetSamplerState ( 0, D3DSAMP_MINFILTER,					D3DTEXF_LINEAR );
		m_pD3D->SetSamplerState ( 0, D3DSAMP_MAGFILTER,					D3DTEXF_LINEAR );
		m_pD3D->SetSamplerState ( 0, D3DSAMP_MIPFILTER,					D3DTEXF_POINT );//D3DTEXF_NONE );

        m_pD3D->SetTextureStageState ( 0, D3DTSS_TEXCOORDINDEX,			0 );
        m_pD3D->SetTextureStageState ( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
        m_pD3D->SetTextureStageState ( 1, D3DTSS_COLOROP,				D3DTOP_DISABLE );

		// leefix-060803-recreate stateblocks
        if ( which == 0 )
            m_pD3D->EndStateBlock ( &m_pSavedStateBlock );
        else
            m_pD3D->EndStateBlock ( &m_pDrawTextStateBlock );
    }
}

DARKSDK void Recreate ( void )
{
	if ( GDI_TEXT == false )
	{
		DeleteFonts();
		DeleteStates();
		SetupFont ( );
		SetupStates ( );
	}
	

	if ( GDI_TEXT == true )
	{
		DeleteFonts();
		SetupFont ( );
	}

	/*
	DeleteFonts();
	DeleteStates();
	SetupFont ( );
	SetupStates ( );
	*/
}

DARKSDK void GetCullDataFromModel(int)
{
}

DARKSDK int CALLBACK EnumFontFamProc(ENUMLOGFONT FAR *lpelf, NEWTEXTMETRIC FAR *lpntm, int FontType, LPARAM lParam )
{
	LPSTR pFontName=lpelf->elfLogFont.lfFaceName;
	if(pFontName)
	{
		DWORD dwSize=static_cast<DWORD>(strlen(pFontName))+1;
		if(dwSize>g_dwMaxStringSizeInEnum) g_dwMaxStringSizeInEnum=dwSize;
		if(g_bCreateChecklistNow)
		{
			// New checklist item
			// Bounded copy into the engine-managed checklist string buffer.
			// Capacity (dwStringSize) is guaranteed >= name length by GlobExpandChecklist.
			LPSTR pCheckStr = g_pGlob->checklist[g_pGlob->checklistqty].string;
			DWORD dwCap = g_pGlob->checklist[g_pGlob->checklistqty].dwStringSize;
			if ( pCheckStr && dwCap > 0 )
			{
				std::strncpy( pCheckStr, pFontName, dwCap - 1 );
				pCheckStr[ dwCap - 1 ] = 0;
			}
		}
		g_pGlob->checklistqty++;
	}
	return 1;
}

DARKSDK void Text ( int iX, int iY, char* szText )
{
	// only if string given
	if(szText==nullptr)
		return;

	if ( GDI_TEXT == true )
	{
		LPDIRECT3DSURFACE9 pBackBuffer = g_pGlob->pCurrentBitmapSurface;
		HDC hDC = nullptr;
		pBackBuffer->GetDC ( &hDC );
		dbp::text::ScopeGuard dcGuard( [pBackBuffer, hDC]{ pBackBuffer->ReleaseDC ( hDC ); } );
		// select the retained font for this draw; scope guard restores the previous object
		HGDIOBJ hbmOldFont = ::SelectObject ( hDC, g_hRetainRawTextWriteFont );
		dbp::text::ScopeGuard fontGuard( [hDC, hbmOldFont]{ ::SelectObject ( hDC, hbmOldFont ); } );
		SetTextColor ( hDC, RGB ( RgbR(m_dwColor),RgbG(m_dwColor),RgbB(m_dwColor) ) );
		SetBkColor   ( hDC, RGB ( RgbR(m_dwBKColor),RgbG(m_dwBKColor),RgbB(m_dwBKColor) ) );
		SetTextAlign ( hDC, TA_LEFT );
		DWORD dwOpaqueMode = ETO_OPAQUE;
		if ( m_bTextOpaque==false )
		{
			SetBkMode ( hDC, TRANSPARENT );
			dwOpaqueMode=0;
		}
		else
			SetBkMode ( hDC, OPAQUE );

        ExtTextOut ( hDC, iX, iY, dwOpaqueMode, nullptr, szText, static_cast<UINT>(strlen(szText)), nullptr );
		// font and hDC restored/released automatically by their scope guards

		// complete
		return;
	}

	// leeadd - 160204 - use simple GDI textwrite if using wide character (for now)
	// if ( g_bWideCharacterSet && g_hRetainRawTextWriteFont )
	// U73 - 240309 - widecharacter text not drawing to camera render targets, so only do code below for direct
	bool bRenderingToCamera = false;
	if ( g_pGlob->iCurrentBitmapNumber==0 && g_pGlob->pCurrentBitmapSurface!=g_pGlob->pHoldBackBufferPtr ) bRenderingToCamera = true;
	if ( g_bWideCharacterSet && g_hRetainRawTextWriteFont && bRenderingToCamera==false )
	{
		// direct write of wide character text
		LPDIRECT3DSURFACE9 pBackBuffer = g_pGlob->pCurrentBitmapSurface;
		HDC hDC = nullptr;
		pBackBuffer->GetDC ( &hDC );
		dbp::text::ScopeGuard dcGuard( [pBackBuffer, hDC]{ pBackBuffer->ReleaseDC ( hDC ); } );
	    HGDIOBJ hbmOldFont = ::SelectObject ( hDC, g_hRetainRawTextWriteFont );
	    dbp::text::ScopeGuard fontGuard( [hDC, hbmOldFont]{ ::SelectObject ( hDC, hbmOldFont ); } );
		SetTextColor ( hDC, RGB ( RgbR(m_dwColor),RgbG(m_dwColor),RgbB(m_dwColor) ) );
		SetBkColor   ( hDC, RGB ( RgbR(m_dwBKColor),RgbG(m_dwBKColor),RgbB(m_dwBKColor) ) );
		SetTextAlign ( hDC, TA_TOP );
		DWORD dwOpaqueMode = ETO_OPAQUE;
		if ( m_bTextOpaque==false )
		{
			SetBkMode ( hDC, TRANSPARENT );
			dwOpaqueMode=0;
		}
		else
			SetBkMode ( hDC, OPAQUE );

        ExtTextOut ( hDC, iX, iY, dwOpaqueMode, nullptr, szText, static_cast<UINT>(strlen(szText)), nullptr );
		// font and hDC restored/released automatically by their scope guards

		// complete
		return;
	}

	float			fStartX			= static_cast<float>(iX);
	float			fX				= static_cast<float>(iX);
	float			fY				= static_cast<float>(iY);

	FONT2DVERTEX*	pVertices		= nullptr;
    DWORD			dwNumTriangles	= 0;

	// setup renderstates
//    m_pD3D->CaptureStateBlock ( m_dwSavedStateBlock );
//    m_pD3D->ApplyStateBlock   ( m_dwDrawTextStateBlock );
	m_pSavedStateBlock->Capture();
	m_pDrawTextStateBlock->Apply();

    m_pD3D->SetVertexShader   ( nullptr );
    m_pD3D->SetFVF   ( D3DFVF_FONT2DVERTEX );

    m_pD3D->SetStreamSource   ( 0, m_pVB, 0, sizeof ( FONT2DVERTEX ) );

	// fill vertex buffer
    m_pVB->Lock ( 0, 0, (void**)&pVertices, D3DLOCK_DISCARD );

    while ( *szText )
    {
        uint8_t c = *szText++;

        if ( c == ('\n') )
        {
            fX  = fStartX;
            fY += ( m_fTexCoords [ 0 ] [ 3 ] - m_fTexCoords [ 0 ] [ 1 ] ) * m_dwTexHeight;
        }
        
		if ( c < (' ') )
            continue;

        FLOAT tx1 = m_fTexCoords [ c - 32 ] [ 0 ];
        FLOAT ty1 = m_fTexCoords [ c - 32 ] [ 1 ];
        FLOAT tx2 = m_fTexCoords [ c - 32 ] [ 2 ];
        FLOAT ty2 = m_fTexCoords [ c - 32 ] [ 3 ];

        FLOAT w = ( tx2 - tx1 ) * m_dwTexWidth  / m_fTextScale;
        FLOAT h = ( ty2 - ty1 ) * m_dwTexHeight / m_fTextScale;

		DWORD dwUseActualVertexColour=m_dwColor;
		if(m_dwBKColor>0) dwUseActualVertexColour=D3DCOLOR_ARGB ( 255, 255, 255, 255 );
		if(m_dwBKColor==0 && m_dwColor==0) dwUseActualVertexColour=D3DCOLOR_ARGB ( 255, 1, 1, 1 );
        *pVertices++ = InitFont2DVertex ( D3DXVECTOR4 ( fX + 0 - 0.5f, fY + h -0.5f, 0.9f, 1.0f ), dwUseActualVertexColour, tx1, ty2 );
        *pVertices++ = InitFont2DVertex ( D3DXVECTOR4 ( fX + 0 - 0.5f, fY + 0 -0.5f, 0.9f, 1.0f ), dwUseActualVertexColour, tx1, ty1 );
        *pVertices++ = InitFont2DVertex ( D3DXVECTOR4 ( fX + w - 0.5f, fY + h -0.5f, 0.9f, 1.0f ), dwUseActualVertexColour, tx2, ty2 );
        *pVertices++ = InitFont2DVertex ( D3DXVECTOR4 ( fX + w - 0.5f, fY + 0 -0.5f, 0.9f, 1.0f ), dwUseActualVertexColour, tx2, ty1 );
        *pVertices++ = InitFont2DVertex ( D3DXVECTOR4 ( fX + w - 0.5f, fY + h -0.5f, 0.9f, 1.0f ), dwUseActualVertexColour, tx2, ty2 );
        *pVertices++ = InitFont2DVertex ( D3DXVECTOR4 ( fX + 0 - 0.5f, fY + 0 -0.5f, 0.9f, 1.0f ), dwUseActualVertexColour, tx1, ty1 );

        dwNumTriangles += 2;

        if ( dwNumTriangles * 3 > ( MAX_NUM_VERTICES - 6 ) )
        {
            // unlock, render, and relock the vertex buffer
            m_pVB->Unlock ( );

            m_pD3D->DrawPrimitive ( D3DPT_TRIANGLELIST, 0, dwNumTriangles );

            pVertices = nullptr;

            m_pVB->Lock ( 0, 0, (void**)&pVertices, D3DLOCK_DISCARD );
            dwNumTriangles = 0L;
        }

        fX += m_szTexWidth [ c - 32 ];
    }

	// unlock and render the vertex buffer
    m_pVB->Unlock();

    if ( dwNumTriangles > 0 )
        m_pD3D->DrawPrimitive ( D3DPT_TRIANGLELIST, 0, dwNumTriangles );

    // restore the modified renderstates
//    m_pD3D->ApplyStateBlock ( m_dwSavedStateBlock );
	m_pSavedStateBlock->Apply();
}

DARKSDK void SetTextFont ( char* szTypeface, int iCharacterSet )
{
	// If not setup, exit
	if ( GDI_TEXT == false )
	{
		if(m_pTexture==nullptr) return;
	}


	const size_t nameLen = std::strlen( szTypeface );
	const size_t copyLen = ( nameLen < ( sizeof( m_strFontName ) - 1 ) ) ? nameLen : ( sizeof( m_strFontName ) - 1 );
	memset ( m_strFontName, 0, sizeof ( m_strFontName ) );
	memcpy ( m_strFontName, szTypeface, copyLen );
	m_iTextCharSet = iCharacterSet;
	Recreate ( );
}

/*
void SetCursor ( int iX, int iY )
{
	m_iX = iX;
	m_iY = iY;
}

void Print ( char* szText )
{
	if(szText)
	{
		Text ( m_iX, m_iY, szText );
	}
}
*/

DARKSDK  LPSTR GetReturnStringFromWorkString(void)
{
	LPSTR pReturnString=nullptr;
	if(m_pWorkString)
	{
		DWORD dwSize=static_cast<DWORD>(strlen(m_pWorkString));
		g_pCreateDeleteStringFunction((DWORD_PTR*)&pReturnString, dwSize+1);
		// pReturnString was allocated to exactly dwSize+1 above; copy incl. null terminator
		memcpy( pReturnString, m_pWorkString, dwSize + 1 );
	}
	return pReturnString;
}

DARKSDK int GetTextWidth ( char* szString )
{
	int iWidth=0;
	if(szString)
	{
		while ( *szString )
		{
			uint8_t c = *szString++;
			if ( c>=32 ) iWidth+=m_szTexWidth [ c - 32 ];
		}
	}
	return iWidth;
}

DARKSDK int GetTextHeight ( char* szString )
{
	int iHeight=0;
	if(szString)
	{
		while ( *szString )
		{
			uint8_t c = *szString++;
			if ( c>=32 )
			{
				int iThisH = m_szTexHeight [ c - 32 ];
				if(iThisH>iHeight) iHeight=iThisH;
			}
		}
	}
	return iHeight;
}

//
// General String Command Functions
//

DARKSDK int	  Asc	( DWORD_PTR dwSrcStr )
{
	if(dwSrcStr)
		return static_cast<int>(*reinterpret_cast<unsigned char*>(dwSrcStr));
	else
		return 0;
}

DARKSDK DWORD_PTR Bin( DWORD_PTR pDestStr, int iValue )
{
	std::string text;
	text.reserve( 32 );
	for ( int bit = 31; bit >= 0; --bit )
	{
		const unsigned int mask = 1u << bit;
		text += ( iValue & static_cast<int>( mask ) ) ? '1' : '0';
	}

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( text );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Chr( DWORD_PTR pDestStr, int iValue )
{
	const char buf[2] = { static_cast<char>( iValue ), '\0' };

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( std::string_view( buf, 1 ) );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Hex( DWORD_PTR pDestStr, int iValue )
{
	char buf[16];
	const int n = std::snprintf( buf, sizeof( buf ), "%X", iValue );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( std::string_view( buf, n > 0 ? static_cast<size_t>( n ) : 0 ) );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Left( DWORD_PTR pDestStr, DWORD_PTR szText, int iValue )
{
	std::string text = SafeString( szText );

	if ( iValue > 0 && iValue <= static_cast<int>( text.size() ) )
		text.resize( iValue );
	else if ( iValue <= 0 )
		text.clear();

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( text );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK int	  Len	( DWORD_PTR dwSrcStr )
{
	const char* s = SafeString( dwSrcStr );
	return static_cast<int>(strlen(s));
}

DARKSDK DWORD_PTR Lower( DWORD_PTR pDestStr, DWORD_PTR szText )
{
	std::string text = SafeString( szText );
	for ( char& ch : text )
		ch = static_cast<char>( std::tolower( static_cast<unsigned char>( ch ) ) );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( text );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Mid( DWORD_PTR pDestStr, DWORD_PTR szText, int iValue )
{
	const std::string src = SafeString( szText );
	std::string text;

	const unsigned int index = static_cast<unsigned int>( iValue );
	if ( index > 0 && index <= src.size() )
		text = src[ index - 1 ];

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( text );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Right( DWORD_PTR pDestStr, DWORD_PTR szText, int iValue )
{
	std::string text = SafeString( szText );

	const int length = static_cast<int>( text.size() );
	const int rightmost = length - iValue;
	if ( rightmost >= 0 && rightmost <= length )
		text = text.substr( static_cast<size_t>( rightmost ) );
	else if ( rightmost > length )
		text.clear();

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( text );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Str( DWORD_PTR pDestStr, float fValue )
{
	char buf[64];
	const int n = std::snprintf( buf, sizeof( buf ), "%.12g", fValue );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( std::string_view( buf, n > 0 ? static_cast<size_t>( n ) : 0 ) );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR StrEx( DWORD_PTR pDestStr, float fValue, int iDecPlaces )
{
	char buf[64];
	const int n = std::snprintf( buf, sizeof( buf ), "%.*f", iDecPlaces, fValue );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( std::string_view( buf, n > 0 ? static_cast<size_t>( n ) : 0 ) );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Str( DWORD_PTR pDestStr, int iValue )
{
	char buf[32];
	const int n = std::snprintf( buf, sizeof( buf ), "%d", iValue );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( std::string_view( buf, n > 0 ? static_cast<size_t>( n ) : 0 ) );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR Upper( DWORD_PTR pDestStr, DWORD_PTR szText )
{
	std::string text = SafeString( szText );
	for ( char& ch : text )
		ch = static_cast<char>( std::toupper( static_cast<unsigned char>( ch ) ) );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( text );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

/*
int	  ValL	( DWORD dwSrcStr )
{
	if(dwSrcStr)
		return std::atoi(dwSrcStr);
	else
		return 0;
}
*/

DARKSDK DWORD ValF	( DWORD_PTR dwSrcStr )
{
	float fValue = 0.0f;
	if(dwSrcStr) fValue = static_cast<float>(std::atof((const char*)dwSrcStr));
	return std::bit_cast<DWORD>(fValue);
}

//LEEFIX - 191102 - Added DOUBLE INTEGER Return for bigger numbers
/*LEEFIX - 060303 - Removed until compiler can handle expression selection
//					from multiple output types ( I dont want to start adding commands!)
*/
// mike - 220107 - add this back in for gdk
DARKSDK LONGLONG ValR	( DWORD_PTR dwSrcStr )
{
	LONGLONG lValue = 0;
	if(dwSrcStr) lValue = std::atoll((const char*)dwSrcStr);
	return lValue;
}


DARKSDK DWORD_PTR StrDouble( DWORD_PTR pDestStr, double dValue )
{
	char buf[64];
	const int n = std::snprintf( buf, sizeof( buf ), "%.16g", dValue );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( std::string_view( buf, n > 0 ? static_cast<size_t>( n ) : 0 ) );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK DWORD_PTR StrDoubleInt( DWORD_PTR pDestStr, LONGLONG lValue )
{
	char buf[32];
	const int n = std::snprintf( buf, sizeof( buf ), "%lld", static_cast<long long>( lValue ) );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( std::string_view( buf, n > 0 ? static_cast<size_t>( n ) : 0 ) );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

//
// Text Command Functions
//

DARKSDK void PerformChecklistForFonts ( void )
{
	// Generate Checklist
	g_pGlob->checklisthasvalues=false;
	g_pGlob->checklisthasstrings=true;

	g_dwMaxStringSizeInEnum=0;
	g_bCreateChecklistNow=false;
	for(int pass=0; pass<2; pass++)
	{
		if(pass==1)
		{
			// Ensure checklist is large enough
			g_bCreateChecklistNow=true;
			for(int c=0; c<g_pGlob->checklistqty; c++)
				GlobExpandChecklist(c, g_dwMaxStringSizeInEnum);
		}

		// Run through total list of fonts
		g_pGlob->checklistqty=0;
		HDC hDC = CreateCompatibleDC ( nullptr );
		dbp::text::ScopeGuard hdcGuard( [hDC]{ if ( hDC ) ::DeleteDC( hDC ); } );															// create dc
		DWORD SearchData=0;
		EnumFontFamilies(hDC, nullptr, (FONTENUMPROC)EnumFontFamProc, SearchData); 
		// hDC released automatically by its scope guard
	}
 
	// Determine if checklist has any contents
	if(g_pGlob->checklistqty>0)
		g_pGlob->checklistexists=true;
	else
		g_pGlob->checklistexists=false;
}

DARKSDK void BasicText ( int iX, int iY, DWORD_PTR szText )
{
	// External Ink Color Control
	if(m_dwColor!=g_pGlob->dwForeColor)
	{
		m_dwColor=g_pGlob->dwForeColor;
		if(m_dwBKColor>0) Recreate();
	}
	
	if(m_dwBKColor!=g_pGlob->dwBackColor) { m_dwBKColor=g_pGlob->dwBackColor; Recreate(); }
	if(szText) Text ( iX, iY, (char*)szText );
}

DARKSDK void CenterText ( int iX, int iY, DWORD_PTR szText )
{
	// External Ink Color Control
	if(m_dwColor!=g_pGlob->dwForeColor)
	{
		m_dwColor=g_pGlob->dwForeColor;
		if(m_dwBKColor>0) Recreate();
	}
	if(m_dwBKColor!=g_pGlob->dwBackColor) { m_dwBKColor=g_pGlob->dwBackColor; Recreate(); }
	int iHalfWidth=GetTextWidth((char*)szText)/2;
	if(szText) Text ( iX-iHalfWidth, iY, (char*)szText );
}

DARKSDK void SetBasicTextFont ( DWORD_PTR szTypeface )
{
	if(szTypeface)
	{
		const char* pTypeface = (const char*)szTypeface;
		memset ( m_strFontName, 0, sizeof ( m_strFontName ) );
		memcpy ( m_strFontName, pTypeface, sizeof ( char ) * strlen ( pTypeface ) );
		Recreate ( );
	}
}

DARKSDK void SetBasicTextFont ( DWORD_PTR szTypeface, int iCharacterSet )
{
	if(szTypeface)
	{
		const char* pTypeface = (const char*)szTypeface;
		memset ( m_strFontName, 0, sizeof ( m_strFontName ) );
		memcpy ( m_strFontName, pTypeface, sizeof ( char ) * strlen ( pTypeface ) );
		m_iTextCharSet = iCharacterSet;
		Recreate ( );
	}
}

DARKSDK void SetTextSize ( int iSize )
{
	if ( static_cast<DWORD>(iSize) != m_dwFontHeight )
	{
		m_dwFontHeight = iSize;

		// mike - 250604 - size cannot be greater than 100

		if ( m_dwFontHeight > 100 )
			m_dwFontHeight = 100;

		Recreate ( );
	}
}

DARKSDK void SetTextToNormal ( void )
{
	m_bTextBold   = false;
	m_bTextItalic = false;
	Recreate ( );
}

DARKSDK void SetTextToItalic ( void )
{
	m_bTextBold   = false;
	m_bTextItalic = true;
	Recreate ( );
}

DARKSDK void SetTextToBold ( void )
{
	m_bTextBold = true;
	m_bTextItalic = false;
	Recreate ( );
}

DARKSDK void SetTextToBoldItalic ( void )
{
	m_bTextBold   = true;
	m_bTextItalic = true;
	Recreate ( );
}

DARKSDK void SetTextToOpaque ( void )
{
	m_bTextOpaque = true;
	Recreate ( );
}

DARKSDK void SetTextToTransparent ( void )
{
	m_bTextOpaque = false;
	Recreate ( );
}

//
// Command Expression Functions
//

DARKSDK DWORD_PTR TextFont( DWORD_PTR pDestStr )
{
	dbp::text::set_work_string( std::string_view( m_strFontName, std::strlen( m_strFontName ) ) );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	LPSTR pReturnString=GetReturnStringFromWorkString();
	return (DWORD_PTR)pReturnString;
}

DARKSDK int TextSize ( void )
{
	return m_dwFontHeight;
}

DARKSDK int TextStyle ( void )
{
	int iStyle = 0;
	if ( m_bTextItalic ) iStyle |= static_cast<int>( dbp::text::TextStyleFlag::Italic );
	if ( m_bTextBold )   iStyle |= static_cast<int>( dbp::text::TextStyleFlag::Bold );
	return iStyle;
}

DARKSDK int TextBackgroundType ( void )
{
	if(m_bTextOpaque)
		return 1;
	else
		return 0;
}

DARKSDK int TextWidth ( DWORD_PTR szString )
{
	int iWidth=0;
	if(szString)
	{
		const uint8_t* pStr = (const uint8_t*)szString;
		while ( *pStr )
		{
			uint8_t c = *pStr++;
			if ( c>=32 ) iWidth+=m_szTexWidth [ c - 32 ];
		}
	}
	return iWidth;
}

DARKSDK int TextHeight ( DWORD_PTR szString )
{
	int iHeight=0;
	if(szString)
	{
		const uint8_t* pStr = (const uint8_t*)szString;
		while ( *pStr )
		{
			uint8_t c = *pStr++;
			if ( c>=32 )
			{
				int iThisH = m_szTexHeight [ c - 32 ];
				if(iThisH>iHeight) iHeight=iThisH;
			}
		}
	}
	return iHeight;
}

//
// New Command Functions
//

DARKSDK void Text3D ( char* szText )
{
	if(szText==nullptr)
		return;

	float			x = 0.0f;
    float			y = 0.0f;
	FONT3DVERTEX*	pVertices;
    DWORD			dwVertex       = 0L;
    DWORD			dwNumTriangles = 0L;
	FLOAT			fStartX = x;
	unsigned char	c;

	D3DXMATRIX	matTrans;

	m_pD3D->GetTransform ( D3DTS_WORLD, &matTrans );
	m_pD3D->SetTransform ( D3DTS_WORLD, &m_pPosText->matObject );

	// setup renderstates
    m_pSavedStateBlock->Capture();
    m_pDrawTextStateBlock->Apply();

    m_pD3D->SetVertexShader   ( nullptr );
    m_pD3D->SetFVF   ( D3DFVF_FONT3DVERTEX );

    m_pD3D->SetStreamSource   ( 0, m_pVB, 0, sizeof ( FONT3DVERTEX) );

    // set filter states
	m_pD3D->SetSamplerState ( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	m_pD3D->SetSamplerState ( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	m_pD3D->SetRenderState ( D3DRS_CULLMODE, D3DCULL_NONE );

	// fill vertex buffer
    m_pVB->Lock ( 0, 0, (void**)&pVertices, D3DLOCK_DISCARD );

	while ( c = *szText++ )
    {
        if ( c == '\n' )
        {
            x  = fStartX;
            y -= ( m_fTexCoords [ 0 ] [ 3 ] - m_fTexCoords [ 0 ] [ 1 ] ) * m_dwTexHeight / 10.0f;
        }

        if ( c < 32 )
            continue;

        FLOAT tx1 = m_fTexCoords [ c - 32 ] [ 0 ];
        FLOAT ty1 = m_fTexCoords [ c - 32 ] [ 1 ];
        FLOAT tx2 = m_fTexCoords [ c - 32 ] [ 2 ];
        FLOAT ty2 = m_fTexCoords [ c - 32 ] [ 3 ];

        FLOAT w = ( tx2 - tx1 ) * m_dwTexWidth  / ( 10.0f * m_fTextScale );
        FLOAT h = ( ty2 - ty1 ) * m_dwTexHeight / ( 10.0f * m_fTextScale );

        *pVertices++ = InitFont3DVertex ( D3DXVECTOR3 ( x + 0, y + 0, 0 ), D3DXVECTOR3 ( 0, 0, -1 ), tx1, ty2 );
        *pVertices++ = InitFont3DVertex ( D3DXVECTOR3 ( x + 0, y + h, 0 ), D3DXVECTOR3 ( 0, 0, -1 ), tx1, ty1 );
        *pVertices++ = InitFont3DVertex ( D3DXVECTOR3 ( x + w, y + 0, 0 ), D3DXVECTOR3 ( 0, 0, -1 ), tx2, ty2 );
        *pVertices++ = InitFont3DVertex ( D3DXVECTOR3 ( x + w, y + h, 0 ), D3DXVECTOR3 ( 0, 0, -1 ), tx2, ty1 );
        *pVertices++ = InitFont3DVertex ( D3DXVECTOR3 ( x + w, y + 0, 0 ), D3DXVECTOR3 ( 0, 0, -1 ), tx2, ty2 );
        *pVertices++ = InitFont3DVertex ( D3DXVECTOR3 ( x + 0, y + h, 0 ), D3DXVECTOR3 ( 0, 0, -1 ), tx1, ty1 );
        dwNumTriangles += 2;

        if ( dwNumTriangles * 3 > ( MAX_NUM_VERTICES - 6 ) )
        {
            m_pVB->Unlock ( );
            m_pD3D->DrawPrimitive ( D3DPT_TRIANGLELIST, 0, dwNumTriangles );
            m_pVB->Lock ( 0, 0, (void**)&pVertices, D3DLOCK_DISCARD );
            dwNumTriangles = 0L;
        }

        x += w;
    }

	// Unlock and render the vertex buffer
    m_pVB->Unlock ( );

    if ( dwNumTriangles > 0 )
		m_pD3D->DrawPrimitive ( D3DPT_TRIANGLELIST, 0, dwNumTriangles );

    // restore the modified renderstates
//    m_pD3D->ApplyStateBlock ( m_dwSavedStateBlock );
    m_pSavedStateBlock->Apply();

	//m_pD3D->GetTransform ( D3DTS_WORLD, &matTrans );
	m_pD3D->SetTransform ( D3DTS_WORLD, &matTrans );
}

//
// Extra String Expressions
//

DARKSDK DWORD_PTR Spaces( DWORD_PTR pDestStr, int iSpaces )
{
	// mike - 250604 - addition for negative input
	if ( iSpaces < 0 )
	{
		if(pDestStr) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
		LPSTR pReturnString=nullptr;
		g_pCreateDeleteStringFunction((DWORD_PTR*)&pReturnString, 2 );

		memset(pReturnString, 32, 2);

		pReturnString [ 0 ] = 0;
		pReturnString [ 1 ] = 0;

		return (DWORD_PTR)pReturnString;	
	}

	// Create and return string
	if(pDestStr) g_pCreateDeleteStringFunction((DWORD_PTR*)&pDestStr, 0);
	LPSTR pReturnString=nullptr;
	g_pCreateDeleteStringFunction((DWORD_PTR*)&pReturnString, iSpaces+1 );
	memset(pReturnString, 32, iSpaces);
	pReturnString[iSpaces]=0;
	return (DWORD_PTR)pReturnString;
}

// MIKE - 100204 - new text based commands

/*
	APPEND$%SS%?Append@@YAXKK@Z%string A, string B
	REVERSE$%S%?Reverse@@YAXK@Z%string
	FIND FIRST CHAR$[%LSS%?FindFirstChar@@YAHKK@Z%source, char
	FIND LAST CHAR$[%LSS%?FindLastChar@@YAHKK@Z%source, char
	FIND SUB STRING$[%LSS%?FindSubString@@YAHKK@Z%source, string
	COMPARE CASE$[%LSS%?CompareCase@@YAHKK@Z%string A, string B
	FIRST TOKEN$[%SSS%?FirstToken@@YAKKKK@Z%source, delim
	NEXT TOKEN$[%SS%?NextToken@@YAKKK@Z%delim
*/

DARKSDK char* SetupString ( const char* szInput )
{
	char* pReturn = nullptr;
	DWORD dwSize  = static_cast<DWORD>(strlen ( szInput ));

	g_pGlob->CreateDeleteString((DWORD_PTR*)&pReturn, dwSize + 1 );

	// error
	if ( !pReturn )
		RunTimeError ( RUNTIMEERROR_NOTENOUGHMEMORY );
		
	memcpy ( pReturn, szInput, dwSize );

	pReturn [ dwSize ] = 0;

	return pReturn;
}

// u74b7 - removed append statement as not fixable
// For information: The string resource was ... APPEND$%SS%?Append@@YAXKK@Z%string A, string B
//DARKSDK void Append ( DWORD dwA, DWORD dwB )
//{
//	strcat ( ( char* ) dwA, ( char* ) dwB );
//}

DARKSDK DWORD_PTR Reverse ( DWORD_PTR pDestStr, DWORD_PTR szText )
{
	ValidateWorkString( (const char*)szText );
	std::string text = szText ? (const char*)szText : "";
	std::reverse( text.begin(), text.end() );

	if ( pDestStr ) g_pCreateDeleteStringFunction( (DWORD_PTR*)&pDestStr, 0 );
	dbp::text::set_work_string( text );
	return (DWORD_PTR)GetReturnStringFromWorkString();
}

DARKSDK int FindFirstChar ( DWORD_PTR dwSource, DWORD_PTR dwChar )
{
	const char* pSrc = ( const char* ) dwSource;
	const char* pCh  = ( const char* ) dwChar;
	if ( !pSrc || !pCh || pCh[0] == '\0' )
		return 0;
	const char* pFound = std::strchr( pSrc, pCh[0] );
	if ( !pFound )
		return 0;
	return static_cast<int>( pFound - pSrc + 1 );
}

DARKSDK int FindLastChar ( DWORD_PTR dwSource, DWORD_PTR dwChar )
{
	const char* pSrc = ( const char* ) dwSource;
	const char* pCh  = ( const char* ) dwChar;
	if ( !pSrc || !pCh || pCh[0] == '\0' )
		return 0;
	const char* pFound = std::strrchr( pSrc, pCh[0] );
	if ( !pFound )
		return 0;
	return static_cast<int>( pFound - pSrc + 1 );
}

DARKSDK int FindSubString ( DWORD_PTR dwSource, DWORD_PTR dwString )
{
	const char* szSource = ( const char* ) dwSource;
	const char* szString = ( const char* ) dwString;

	if ( !szSource || !szString || szString[0] == '\0' )
		return 0;

	const char* szResult = std::strstr( szSource, szString );
	if ( szResult )
		return static_cast<int>( szResult - szSource + 1 );

	return 0;
}

DARKSDK int CompareCase ( DWORD_PTR dwA, DWORD_PTR dwB )
{
	const char* szA = ( const char* ) dwA;
	const char* szB = ( const char* ) dwB;

	if ( !szA || !szB )
		return 0;

	return std::strcmp( szA, szB ) == 0 ? 1 : 0;
}

// u74b7 - rewrite so that tokens are non-destructive to the source string.
//         Work is carried out on a copy instead of the original.
DARKSDK DWORD_PTR FirstToken ( DWORD_PTR dwReturn, DWORD_PTR dwSource, DWORD_PTR dwDelim )
{
    LPCSTR szSource = (LPCSTR) dwSource;
    LPCSTR szDelim  = (LPCSTR) dwDelim;

    // If the delimiter an empty string, use a space
    if (szDelim == nullptr || szDelim[0] == 0)
    {
        szDelim = " ";
    }

    // If there's a string, copy it to the temp area
    if ( szSource && szSource[0] )
    {
        // Get length, including null terminator
        DWORD dwLength = static_cast<DWORD>(strlen( szSource ) + 1);

        // If the temp area isn't large enough, make it so
        if ( dwLength > m_dwTokenStringSize )
        {
            // U74 - 060709 - double the allocation is not enough, must use dwLength
            m_dwTokenStringSize = dwLength + 1;

			// free old string and create larger one
            delete[] m_szTokenString;
            m_szTokenString = new char[ m_dwTokenStringSize ];
			memset ( m_szTokenString, 0, m_dwTokenStringSize );
        }

        // Copy the source string into the temp area
        memcpy( m_szTokenString, szSource, dwLength );
    }
    else
    {
        m_szTokenString[ 0 ] = 0;
    }

	// U74 BETA9 - 060709 - free old string and create new one
	g_pGlob->CreateDeleteString((DWORD_PTR*)&dwReturn, 0 );
	char* szToken = dbp::text::next_token ( m_szTokenString, szDelim );
    if ( szToken )
    {
	    return (DWORD_PTR)SetupString ( szToken );
    }
    return (DWORD_PTR)SetupString ( "" );
}

DARKSDK DWORD_PTR NextToken ( DWORD_PTR dwReturn, DWORD_PTR dwDelim )
{
    LPCSTR szDelim  = (LPCSTR) dwDelim;

    // If the delimiter an empty string, use a space
    if (szDelim == nullptr || szDelim[0] == 0)
    {
        szDelim = " ";
    }

	// U74 BETA9 - 060709 - free old string and create new one
	g_pGlob->CreateDeleteString((DWORD_PTR*)&dwReturn, 0 );
	char* szToken = dbp::text::next_token ( nullptr, szDelim );
    if ( szToken )
    {
	    return (DWORD_PTR)SetupString ( szToken );
    }
    return (DWORD_PTR)SetupString ( "" );
}

//////////////////////////////////////////////////////////////////////////////////
// DARK SDK SECTION //////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#ifdef DARKSDK_COMPILE

void ConstructorText ( HINSTANCE hSetup  )
{
	TextConstructor ( hSetup );
}

void DestructorText ( void )
{
	TextDestructor ( );
}

void SetErrorHandlerText ( LPVOID pErrorHandlerPtr )
{
	SetErrorHandler ( pErrorHandlerPtr );
}

void PassCoreDataText ( LPVOID pGlobPtr )
{
	PassCoreData ( pGlobPtr );
}

void RefreshD3DText ( int iMode )
{
	RefreshD3D ( iMode );
}

int dbAsc ( char* dwSrcStr)
{
	return Asc ( ( DWORD_PTR ) dwSrcStr);
}

char* dbBin	( int iValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Bin ( nullptr, iValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbChr	(  int iValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Chr ( nullptr, iValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbHex	(  int iValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Hex ( nullptr, iValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbLeft ( char* szText, int iValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Left ( nullptr, ( DWORD_PTR ) szText, iValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

int dbLen ( char* dwSrcStr )
{
	return Len	(  ( DWORD_PTR ) dwSrcStr );
}

char* dbLower ( char* szText )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Lower ( nullptr, ( DWORD_PTR ) szText );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbMid	(  char* szText, int iValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Mid ( nullptr, ( DWORD_PTR ) szText,  iValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbRight (  char* szText, int iValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Right ( nullptr, ( DWORD_PTR ) szText,  iValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbStr	(  float fValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Str ( nullptr,  fValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbStr	(  int iValue )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Str ( nullptr, iValue );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

// leefix - 2103060 u6b4 - changed from DWORD to char* - GDK could not resolve external linkage
char* dbUpper (  char* szText )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Upper ( nullptr, szText );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

float dbValF ( char* dwSrcStr )
{
	DWORD dwReturn = ValF ( ( DWORD_PTR ) dwSrcStr );
	
	return *( float* ) &dwReturn;
}

double dbStrDouble (  double dValue )
{
	return StrDouble (  nullptr,  dValue );
}

LONGLONG dbValR ( char* dwSrcStr )
{
	// mike - 220107 - modified this function for correct param and casting
	return ValR ( ( DWORD_PTR ) dwSrcStr );
}

void dbPerformChecklistForFonts ( void )
{
	PerformChecklistForFonts ( );
}

void dbText ( int iX, int iY, char* szText )
{
	BasicText ( iX, iY, (DWORD_PTR)szText );
}

void dbCenterText ( int iX, int iY, char* szText )
{
	CenterText ( iX, iY, (DWORD_PTR)szText );
}

void dbSetTextFont ( char* szTypeface )
{
	SetBasicTextFont ( (DWORD_PTR)szTypeface );
}

void dbSetTextFont ( char* szTypeface, int iCharacterSet )
{
	SetBasicTextFont ( (DWORD_PTR)szTypeface, iCharacterSet );
}
	
void dbSetTextSize ( int iSize )
{
	SetTextSize ( iSize );
}
 
void dbSetTextToNormal ( void )
{
	SetTextToNormal ( );
}
 
void dbSetTextToItalic ( void )
{
	SetTextToItalic ( );
}
 
void dbSetTextToBold ( void )
{
	SetTextToBold ( );
}
 
void dbSetTextToBoldItalic ( void )
{
	SetTextToBoldItalic ( );
}
 
void dbSetTextToOpaque ( void )
{
	SetTextToOpaque ( );
}
 
void dbSetTextToTransparent ( void )
{
	SetTextToTransparent ( );
}

int dbTextBackgroundType ( void )
{
	return TextBackgroundType ( );
}

char* dbTextFont ( void )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = TextFont ( nullptr );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

int dbTextSize ( void )
{
	return TextSize ( );
}

int dbTextStyle ( void )
{
	return TextStyle ( );
}

int dbTextWidth ( char* szString )
{
	return TextWidth ( (DWORD_PTR)szString );
}

int dbTextHeight ( char* szString )
{
	return TextHeight ( (DWORD_PTR)szString );
}

void dbText3D ( char* szText )
{
	Text3D (  szText );
}

char* dbSpaces	( int iSpaces )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = Spaces ( nullptr, iSpaces );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

void dbAppend ( char* dwA, char* dwB )
{
//  U74 discontinued command
//	Append ( ( DWORD ) dwA, ( DWORD ) dwB );
}

char* dbReverse ( char* szText )
{
	static char* szReturn = nullptr;
	DWORD_PTR dwReturn = Reverse ( nullptr, (DWORD_PTR)szText );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

int dbFindFirstChar ( char* dwSource, char* dwChar )
{
	return FindFirstChar ( dwSource, ( DWORD ) dwChar );
}

int dbFindLastChar ( char* dwSource, char* dwChar )
{
	return FindLastChar ( dwSource, ( DWORD ) dwChar );
}

int dbFindSubString ( char* dwSource, char* dwString )
{
	return FindSubString ( dwSource, ( DWORD ) dwString );
}

int dbCompareCase ( char* dwA, char* dwB )
{
	return CompareCase ( ( DWORD ) dwA, ( DWORD ) dwB );
}

char* dbFirstToken ( char* dwSource, char* dwDelim )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = FirstToken (  nullptr, dwSource,  dwDelim );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

char* dbNextToken ( char* dwDelim )
{
	static char* szReturn = nullptr;
	DWORD		 dwReturn = NextToken (  nullptr, dwDelim );

	szReturn = ( char* ) dwReturn;

	return szReturn;
}

void dbSetTextColor	( int iAlpha, int iRed, int iGreen, int iBlue )
{
	SetTextColor ( iAlpha, iRed, iGreen, iBlue );
}

int	dbGetTextWidth ( char* szString )
{
	return GetTextWidth ( szString );
}

int	dbGetTextHeight	( char* szString )
{
	return GetTextHeight ( szString );
}

// lee - 300706 - GDK fixes
void dbSetTextOpaque ( void ) { dbSetTextToOpaque (); }							
void dbSetTextTransparent ( void ) { dbSetTextToTransparent (); }					
float dbVal	( char* dwSrcStr ) { return dbValF ( dwSrcStr ); }

#endif

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////