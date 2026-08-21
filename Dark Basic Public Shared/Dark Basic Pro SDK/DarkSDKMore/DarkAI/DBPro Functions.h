#ifndef H_DBPRO_FUNC
#define H_DBPRO_FUNC

#include "directx-macros.h"
#include ".\..\..\Shared\DBOFormat\DBOData.h"
#include "CommonC.h"
#include "CObjectsNewC.h"
#include "CPositionC.h"
#include "CMemblocks.h"
#include "CFileC.h"

// Forwarding inline helpers to map legacy DarkSDK call names to modern DBP SDK exports
inline void SetObjectCollisionOff(int iID) { SetCollisionOff(iID); }
inline void PositionObject(int iID, float x, float y, float z) { Position(iID, x, y, z); }
inline bool CheckObjectExist(int iID) { return ObjectExist(iID) != 0; }
inline void MakeObjectTriangle(int iID, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3) { MakeTriangle(iID, x1, y1, z1, x2, y2, z2, x3, y3, z3); }
inline void DisableObjectZWrite(int iID) { DisableZWrite(iID); }
inline void SetAlphaMappingOn(int iID, float alpha) { SetAlphaFactor(iID, alpha); }
inline void YRotateObject(int iID, float y) { YRotate(iID, y); }
inline void MakeObjectCube(int iID, float size) { MakeCube(iID, size); }
inline float ObjectAngleX(int iID) { return GetXRotation(iID); }
inline float ObjectAngleY(int iID) { return GetYRotation(iID); }
inline float ObjectAngleZ(int iID) { return GetZRotation(iID); }
inline void PointObject(int iID, float x, float y, float z) { Point(iID, x, y, z); }
inline void SetObjectEmissive(int iID, DWORD color) { SetEmissiveMaterial(iID, color); }
inline void MakeObjectSphere(int iID, float fRadius, int iRings = 12, int iSegments = 12) { MakeSphere(iID, fRadius, iRings, iSegments); }
inline void SetObjectMask(int iID, int mask) { SetMask(iID, mask); }
inline void ColorObject(int iID, DWORD color) { Color(iID, color); }
inline void FixObjectPivot(int iID) { FixPivot(iID); }
inline void MakeObjectPlane(int iID, float w, float h, int flag = 1) { (void)flag; MakePlane(iID, w, h); }
inline void XRotateObject(int iID, float x) { XRotate(iID, x); }
inline char* GetMemblockPtr(int mbi) { return (char*)ExtGetMemblockPtr(mbi); }

int dbFreeObject ( );
int dbFreeMesh ( );
int dbMakeEdgeMesh ( );
int dbMakePointMesh ( );
void dbCombineLimbs ( int iObjID );

#endif