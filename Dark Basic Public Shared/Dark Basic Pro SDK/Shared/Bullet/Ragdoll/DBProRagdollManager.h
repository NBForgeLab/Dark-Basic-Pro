#pragma once

#include <windows.h>
#include <cstdint>
#include "DBProRagDoll.h"
#include "BaseItemManager.h"

class DBProRagdollManager : public BaseItemManager
{
public:
	DBProRagdollManager();
	~DBProRagdollManager();

	void AddRagdoll(DBProRagDoll* ragdoll);
	void DeleteRagdoll(int ragdollID);
	DBProRagDoll* GetRagdoll(int ragdollID);
	void Update(); 
	int GetIDFromBoneObject(int objectID);
	static void AssertRagdollExist(int ragdollID, const char* message, bool bExist = true); 
};

extern DBProRagdollManager* ragdollManager;
