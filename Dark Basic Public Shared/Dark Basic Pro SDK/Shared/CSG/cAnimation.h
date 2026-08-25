//////////////////////////////////////////////////////////////////////////////////
// information ///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
/*
	
*/
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// ANIMATION STRUCTURE ///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#ifndef _CANIMATION_H_
#define _CANIMATION_H_

//////////////////////////////////////////////////////////////////////////////////
// INCLUDES / LIBS ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//#pragma comment ( lib, "d3dx8.lib"  )
#pragma comment ( lib, "d3dxof.lib" )
#pragma comment ( lib, "winmm.lib"  )

#pragma comment ( lib, "dxguid.lib" )
//#pragma comment ( lib, "d3d8.lib"   )
//#pragma comment ( lib, "dxerr8.lib" )

#include <d3d8.h>
#include <cmath>
#include <cstdint>
#include <d3dx8.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "sFrame.h"
#include "sAnimationSet.h"

extern LPDIRECT3DDEVICE8	m_pD3D;
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// DEFINES ///////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#define DARKSDK __declspec ( dllexport )
#include "..\Core\macros.h"
#define GXRELEASE(_p) do       { if ((_p) != nullptr) {(_p)->Release(); (_p) = nullptr;} } while (0)
//////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////
// CLASS ////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
class cAnimation 
{
	public:
		long			m_NumAnimations = 0;
		sAnimationSet*	m_AnimationSet  = nullptr;
		sFrame*			m_pFrame        = nullptr;
		
		cAnimation  ( );
		~cAnimation ( );

		BOOL           IsLoaded ( void );

		void           ParseXFileData ( IDirectXFileData* DataObj, sAnimationSet* ParentAnim, sAnimation* CurrentAnim );

		long           GetNumAnimations ( void );
		sAnimationSet* GetAnimationSet  ( char* Name = nullptr );
		unsigned long  GetLength        ( char* Name = nullptr );

		BOOL           Load ( char* Filename, sFrame* pFrame );
		BOOL           Free ( void );

		BOOL           MapToMesh ( void );
		BOOL           SetLoop   ( BOOL ToLoop, char *Name = nullptr );
};

class cSpecialEffect;

struct sObjectData
{
	D3DXMATRIX*			m_matObject;			// positional data
	cAnimation			m_Animations;
	long				m_NumMeshes;
	sMesh*				m_Meshes;
	long				m_NumFrames;
	sFrame*				m_Frames;
	D3DXVECTOR3			m_Min,
						m_Max;
	float				m_Radius;
	unsigned long		m_StartTime;

	bool				m_bOverrideVertexShader;
	DWORD				m_dwVertexShader;
	
	DWORD				m_dwFVF;
	DWORD				m_dwFVFSize;

//	int					m_iPrimType;
	int					m_iTriangleCount;

//	ID3DXMesh*			m_MainMesh;-no need
};

void ParseXFileData   ( sObjectData* pData, IDirectXFileData* pDataObj, sFrame* ParentFrame, char* TexturePath );
bool LoadModelData    ( sObjectData* pData, char* szFilename, char* TexturePath, bool bAnim );
bool AppendAnimationData ( sObjectData* pData, char* szFilename, int iFrameStart );
float* CreatePureTriangleMeshData( float* pMeshDataRaw, DWORD* pdwVertCount, DWORD dwNumVert, DWORD dwFVFSize, WORD* pIndiceData, DWORD dwNumPoly );
sMesh* MakeMeshFromData (	DWORD dwInFVF, DWORD dwInFVFSize, float* pInMesh, DWORD dwInNumVert, DWORD dwPrimType );
bool MakeModelData	  (	sObjectData* pData, DWORD dwInFVF, DWORD dwInFVFSize,
						float* pInMesh, DWORD dwInNumPoly, DWORD dwInNumVert, DWORD dwInPrimType );
void MapFramesToBones ( sObjectData* pData, sFrame *Frame);
void DrawFrame        ( sObjectData* pData, sFrame* Frame);
void BuildMesh		  (	sObjectData* pData, sFrame* Frame, DWORD dwFVF, DWORD dwFVFSize,
						DWORD* pdwVertMax, float** ppTempMeshData, float** ppMeshDataPtr, DWORD* pdwVertsSoFar,
						DWORD* pdwPolyMax, WORD** ppTempIndexData, WORD** ppIndexDataPtr, DWORD* pdwPolysSoFar, int iLimbPart );
ID3DXMesh* ConvertMeshStripToList ( sMesh* pMesh, DWORD dwInFVF, DWORD dwFVFSize );

void UpdateFrame     ( sObjectData* pData, sFrame* Frame, D3DXMATRIX* Matrix);
void DrawFrame       ( sObjectData* pData, sFrame* Frame, int iTIndex, D3DXMATRIX* pHierarchy);
void DrawMesh        ( sObjectData* pData, sMesh* Mesh );
BOOL SetAnimation    ( sObjectData* pData, char* Name, unsigned long StartTime);
BOOL UpdateAnimation ( sObjectData* pData, float Time, BOOL Smooth );

BOOL GetBounds ( sObjectData* pData, float* MinX, float* MinY, float* MinZ, float* MaxX, float* MaxY, float* MaxZ, float* Radius );
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


#endif _CANIMATION_H_