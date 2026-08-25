
// BulletPhysicsWrapper for DarkBasic Proffessional
//Stab In The Dark Software 
//Copyright (c) 2002-2014
//http://stabinthedarksoftware.com

#pragma once

#include "btBulletDynamicsCommon.h"

class DBProRagDollBone
{
public:
	DBProRagDollBone(int dbproObjectID, int dbproStartLimbID, int dbproEndLimbID, btScalar diameter, float lengthmod, int collisionGroup, int collisionMask);
	~DBProRagDollBone(void);
	btRigidBody* GetRigidBody();
	int GetEndLimbID();
	int GetStartLimbID();
	int GetObjectID();
	int GetRagDollBoneID();
	btVector3 GetNormilizedVector();
	void AddJointConstraint(int jointID, int jointType);
	void AddDBProLimbID(int dbproLimbID);
	btMatrix3x3 initialRotation;
	btAlignedObjectArray <int> dbproLimbIDs;
	btAlignedObjectArray <btVector3> limbOffsets;
	btAlignedObjectArray <btMatrix3x3> limbInitalRotation;
	btAlignedObjectArray <btVector3> ceterOfObjectOffsets;
	btScalar boneVolume;

private:
	int dbproObjectID = 0;
	int dbproStartLimbID = 0;
	int dbproEndLimbID = 0;
	int dbproRagDollBoneID = 0;
	btScalar diameter = 0;
	btScalar lengthmod = 0;
	int collisionGroup = 0;
	int collisionMask = 0;
	btVector3 boneNormVec = {};  
	btRigidBody* rigidBody = nullptr;
	btTypedConstraint* jointConstraint = nullptr;
	btCollisionShape* m_collisionShape = nullptr;

private:
	void CreateBone();
	btRigidBody* localCreateRigidBody (btScalar mass, const btTransform& startTransform, btCollisionShape* shape, int objID, int groupFilter, int maskFilter);
};

