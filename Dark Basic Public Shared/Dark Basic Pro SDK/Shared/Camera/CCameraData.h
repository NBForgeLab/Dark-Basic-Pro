#ifndef _CCAMERADATA_H_
#define _CCAMERADATA_H_

#pragma comment ( lib, "dxguid.lib" )
#pragma comment ( lib, "d3d8.lib"   )
#pragma comment ( lib, "d3dx8.lib"  )
#pragma comment ( lib, "dxerr8.lib" )

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9tex.h>
#include <basetsd.h>
#include <stdio.h>
#include <math.h>

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>   
#include <windowsx.h>

struct tagCameraData
{
	D3DXMATRIX	matProj;
	D3DXMATRIX	matView;

	D3DXMATRIX	matRotateX;
	D3DXMATRIX	matRotateY;
	D3DXMATRIX	matRotateZ;

	float		fAspect;
	float		fFOV;
	float		fZNear;
	float		fZFar;

	float		fX;
	float		fY;
	float		fZ;

	float		fXRot;
	float		fYRot;
	float		fZRot;
};

#endif _CCAMERADATA_H_