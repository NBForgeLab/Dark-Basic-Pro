#pragma once

#include <windows.h>
#include <cstdint>
#include "btBulletDynamicsCommon.h"
#include "D3dx9math.h"

class BT2DX
{
public:
	BT2DX(void);
	~BT2DX(void);
	static D3DXVECTOR3 BT2DX_VECTOR3(const btVector3 &v);
	static D3DXQUATERNION BT2DX_QUATERNION(const btQuaternion &q);
	static D3DXMATRIX BT2DX_MATRIX(const btTransform &ms);
	static D3DXMATRIX ConvertBulletMotionState(const btMotionState &ms);
	static btVector3 DX_VECTOR3_2BT(const D3DXVECTOR3 &v);
	static D3DXMATRIX ConvertBulletTransform(const btTransform *bulletTransformMatrix);
	static btTransform ConvertD3DXMatrix(const D3DXMATRIX *d3dMatrix);
	static void XPrepareMatrixFromRULP(D3DXMATRIX &matOutput, const D3DXVECTOR3 *R, const D3DXVECTOR3 *U, const D3DXVECTOR3 *L, const D3DXVECTOR3 *P);
};

