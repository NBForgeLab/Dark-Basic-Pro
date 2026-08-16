#ifndef _CANIMATION_H_
#define _CANIMATION_H_

//////////////////////////////////////////////////////////////////////////////////
// INCLUDES / LIBS ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include <mmsystem.h>
#include <atlbase.h>
#include <stdio.h>
#include <d3d9types.h>

#include <D3D9.h>
#include <D3DX9.h>
#include <dshow.h>

#include "CTextureRenderer.h"

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////
// DEFINES ///////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#ifndef DARKSDK_COMPILE
	#define DARKSDK __declspec ( dllexport )
	#define DBPRO_GLOBAL 
#else
	#define DARKSDK static
	#define DBPRO_GLOBAL static
#endif

#define SAFE_DELETE( p )       { if ( p ) { delete ( p );       ( p ) = NULL; } }
#define SAFE_RELEASE( p )      { if ( p ) { ( p )->Release ( ); ( p ) = NULL; } }
#define SAFE_DELETE_ARRAY( p ) { if ( p ) { delete [ ] ( p );   ( p ) = NULL; } }

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////
// INTERNAL FUNCTIONS ////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#ifdef DARKSDK_COMPILE
		void ConstructorAnimation			( void );
		void DestructorAnimation			( void );
		void SetErrorHandlerAnimation		( LPVOID pErrorHandlerPtr );
		void PassCoreDataAnimation			( LPVOID pGlobPtr );
		void RefreshD3DAnimation			( int iMode );
#endif

DARKSDK void Constructor					( void );
DARKSDK void Destructor						( void );
DARKSDK void SetErrorHandler				( LPVOID pErrorHandlerPtr );
DARKSDK void PassCoreData					( LPVOID pGlobPtr );
DARKSDK void RefreshD3D						( int iMode );
DARKSDK void PreventTextureLock				( bool bDoNotLock );
DARKSDK void UpdateAllAnimation				( void );
DARKSDK BOOL DB_FreeAnimation				( int AnimIndex );

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////
// COMMANDS FUNCTIONS ////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

DARKSDK void LoadAnimation					( DWORD pFilename, int iIndex );
DARKSDK void PlayAnimation					( int iIndex );
DARKSDK void DeleteAnimation				( int animindex );
DARKSDK void PlayAnimation					( int animindex );
DARKSDK void StopAnimation					( int animindex );
DARKSDK void PauseAnimation					( int animindex );
DARKSDK void ResumeAnimation				( int animindex );
DARKSDK void LoopAnimation					( int animindex );
DARKSDK void PlayAnimation					( int animindex, int x1, int y1 );
DARKSDK void PlayAnimation					( int animindex, int x1, int y1, int x2, int y2 );
DARKSDK void PlayAnimationToImage			( int animindex, int imageindex, int x1, int y1, int x2, int y2 );
DARKSDK void PlayAnimationToBitmap			( int animindex, int bitmapindex, int x1, int y1, int x2, int y2 );
DARKSDK void LoopAnimationToBitmap			( int animindex, int bitmapindex, int x1, int y1, int x2, int y2 );
DARKSDK void PlaceAnimation					( int animindex, int x1, int y1, int x2, int y2);

DARKSDK int  AnimationExist					( int animindex );
DARKSDK int  AnimationPlaying				( int animindex );
DARKSDK int  AnimationPaused				( int animindex );
DARKSDK int  AnimationLooping				( int animindex );
DARKSDK int  AnimationPositionX				( int animindex );
DARKSDK int  AnimationPositionY				( int animindex );
DARKSDK int  AnimationWidth					( int animindex );
DARKSDK int  AnimationHeight				( int animindex );

DARKSDK void LoadDVDAnimation				( int animindex );
DARKSDK void SetAnimationVolume				( int animindex, int ivolume );
DARKSDK void SetAnimationSpeed				( int animindex, int ispeed );
DARKSDK void SetDVDChapter					( int animindex, int iTitle, int iChapterNumber );

DARKSDK int  AnimationVolume				( int animindex );
DARKSDK int  AnimationSpeed					( int animindex );
DARKSDK int  TotalDVDChapters				( int animindex, int iTitle );

#ifdef DARKSDK_COMPILE
		void dbLoadAnimation				( char* szFilename, int iIndex );
		void dbDeleteAnimation				( int animindex );
		void dbPlayAnimation				( int animindex );
		void dbStopAnimation				( int animindex );
		void dbPauseAnimation				( int animindex );
		void dbResumeAnimation				( int animindex );
		void dbLoopAnimation				( int animindex );
		void dbPlayAnimation				( int animindex, int x1, int y1 );
		void dbPlayAnimation				( int animindex, int x1, int y1, int x2, int y2 );
		void dbPlayAnimationToImage			( int animindex, int imageindex, int x1, int y1, int x2, int y2 );
		void dbPlayAnimationToBitmap		( int animindex, int bitmapindex, int x1, int y1, int x2, int y2 );
		void dbLoopAnimationToBitmap		( int animindex, int bitmapindex, int x1, int y1, int x2, int y2 );
		void dbPlaceAnimation				( int animindex, int x1, int y1, int x2, int y2);

		int  dbAnimationExist				( int animindex );
		int  dbAnimationPlaying				( int animindex );
		int  dbAnimationPaused				( int animindex );
		int  dbAnimationLooping				( int animindex );
		int  dbAnimationPositionX			( int animindex );
		int  dbAnimationPositionY			( int animindex );
		int  dbAnimationWidth				( int animindex );
		int  dbAnimationHeight				( int animindex );

		void dbLoadDVDAnimation				( int animindex );
		void dbSetAnimationVolume			( int animindex, int ivolume );
		void dbSetAnimationSpeed			( int animindex, int ispeed );
		void dbSetDVDChapter				( int animindex, int iTitle, int iChapterNumber );

		int  dbAnimationVolume				( int animindex );
		int  dbAnimationSpeed				( int animindex );
		int  dbTotalDVDChapters				( int animindex, int iTitle );

		void dbPreventTextureLock			( bool bDoNotLock );
		void dbUpdateAllAnimation			( void );
#endif

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#endif _CANIMATION_H_