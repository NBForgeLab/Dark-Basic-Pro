#ifndef _CCUSTOMBSPDATA_H_
#define _CCUSTOMBSPDATA_H_

#include <cstdint>

const int			CP_FRONT	= 1001;			// in front of plane
const int			CP_BACK		= 1002;			// behind plane
const int			CP_ONPLANE	= 1003;			// on plane
const int			CP_SPANNING	= 1004;			// spanning plane
const FLOAT			g_EPSILON	= 0.001f;		// tolerance for floats

struct D3DLVERTEX
{
    float		x; 
    float		y;    
    float		z; 
	//float nx, ny, nz;
	D3DCOLOR	color;
	float		tu;
	float		tv;
};

struct D3DVERTEX
{
    float		x; 
    float		y;    
    float		z; 
	float		nx, ny, nz;
	D3DCOLOR	color;
	float		tu;
	float		tv;
};

struct PLANE
{
	D3DXVECTOR3 PointOnPlane;
	D3DXVECTOR3 Normal;
};

struct PLANE2
{
 	float		Distance;
	D3DXVECTOR3	Normal;
};

struct POLYGON
{
	D3DVERTEX*	VertexList;
	D3DXVECTOR3 Normal;
	WORD		NumberOfVertices;
	WORD		NumberOfIndices;
	WORD*		Indices;
	POLYGON*	Next;
	WORD		TextureIndex;
};

struct BOUNDINGBOX
{
	D3DXVECTOR3 BoxMin;
	D3DXVECTOR3 BoxMax;
};

struct LEAF
{							
	long		StartPolygon;			
	long		EndPolygon;				
	long		PVSIndex;
	BOUNDINGBOX BoundingBox;
};

struct NODE 
{
	BYTE		IsLeaf;
	ULONG		Plane;
	ULONG		Front;
	LONG		Back;
};

extern PLANE2				FrustumPlanes [ 6 ];
extern BOOL				DontFrustumReject;
extern POLYGON*			PolygonArray;
extern NODE*				NodeArray;
extern LEAF*				LeafArray;
extern PLANE*				PlaneArray;
extern BYTE*				PVSData;
extern LPDIRECT3DTEXTURE9*	lpTextureSurface;
extern uint16_t			NumberOfTextures;
extern POLYGON**			pTexturePolygonList;

extern int32_t				BytesPerSet;
extern int32_t				NumberOfPolygons;
extern int32_t				NumberOfNodes;
extern int32_t				NumberOfLeafs;
extern int32_t				NumberOfPlanes;
extern int32_t				MAXNUMBEROFNODES;
extern int32_t				MAXNUMBEROFPLANES;
extern int32_t				MAXNUMBEROFPOLYGONS;
extern int32_t				MAXNUMBEROFLEAFS;

extern int32_t				PVSCompressedSize;
extern char				( *TextureLUT ) [ 21 ];

#endif _CCUSTOMBSPDATA_H_