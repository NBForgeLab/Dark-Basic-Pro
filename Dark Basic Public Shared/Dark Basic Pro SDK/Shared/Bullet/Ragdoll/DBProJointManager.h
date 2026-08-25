
// BulletPhysicsWrapper for DarkBasic Professional
//Stab In The Dark Software 
//Copyright (c) 2002-2014
//http://stabinthedarksoftware.com


#pragma once

#include <windows.h>
#include <cstdint>
#include "DBProJoint.h"
#include "BaseItemManager.h"
#include "btBulletDynamicsCommon.h"

class DBProJointManager : public BaseItemManager
{
public:
	DBProJointManager(void);
	virtual ~DBProJointManager(void);

	int AddJoint(btTypedConstraint* constraint);
	void DeleteJoint(int jointID);
	int GetNumberOfJoints();
	DBProJoint* GetJoint(int jointID);
	static void AssertValidJointID(int jointID, const char* message);
};

extern DBProJointManager* jointManager;


