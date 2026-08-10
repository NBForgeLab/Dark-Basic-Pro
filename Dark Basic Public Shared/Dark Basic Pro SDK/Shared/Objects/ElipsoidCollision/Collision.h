//
// Collision Functions Header
//

// Define
#ifndef _ELIPSOID_COLLISION_H_
#define _ELIPSOID_COLLISION_H_

// Includes
#include <d3d9.h>
#include <d3dx9.h>
#include <D3dx9tex.h>
#include <D3dx9core.h>
#include <basetsd.h>
#include <stdio.h>
#include <math.h>
#include <D3DX9.h>
#include <d3d9types.h>
#include <vector>
using namespace std;
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include "col_global.h"
#include "..\\Universe.h"

#ifndef _COLLISION_CLASS_DEFINED_
#define _COLLISION_CLASS_DEFINED_
class Collision 
{
	public:
		void	Init	( void );
		BOOL	World	( D3DXVECTOR3 o_pos, D3DXVECTOR3 n_pos, D3DXVECTOR3 eRadius, float fScale );
		BOOL	World  ( D3DXVECTOR3 o_pos, D3DXVECTOR3 n_pos, D3DXVECTOR3 eRadius, D3DXVECTOR3* out_pos, int cut, float fScale );
};
#endif

#endif
