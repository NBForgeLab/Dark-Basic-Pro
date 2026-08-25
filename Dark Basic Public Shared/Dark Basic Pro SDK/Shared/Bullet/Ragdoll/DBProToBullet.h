

#pragma once

#include <windows.h>
#include <cstdint>
#include "btBulletDynamicsCommon.h"
#include "LinearMath\btVector3.h"
#include "LinearMath\btTransform.h"

namespace DBProToBullet
{
	class DBProVertexData
	{
	public:
		DBProVertexData() = default;
		~DBProVertexData()
		{
			vertexBuffer.clear();
			indexBuffer.clear();
			normals.clear();
			uvData.clear();
		}
		btAlignedObjectArray<btVector3> vertexBuffer;
		btAlignedObjectArray<int> indexBuffer;
		btAlignedObjectArray<btVector3> normals;
		btAlignedObjectArray<btVector3> uvData;
	};
	btScalar GetObjectDiameter(int objectID);
	btVector3 GetObjectSize(int objectID);
	DBProToBullet::DBProVertexData* GetVertexData(int objectID, btScalar scaleFactor, bool bTransform, bool bReverseVertexOrder, bool bMirrorOnXAxis); 
	btVector3 GetScale(int objectID); 
	btTransform GetTransform(int objectID, btScalar scaleFactor = 1.0);
	btVector3 GetVector3(int vectorID);
	void AssertValidVector(int vectorID, const char* message);
	void AssertValidObject(int objectID, const char* message);
}

