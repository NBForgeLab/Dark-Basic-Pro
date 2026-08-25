
// BulletPhysicsWrapper for DarkBasic Proffessional
//Stab In The Dark Software 
//Copyright (c) 2002-2014
//http://stabinthedarksoftware.com

///#include "StdAfx.h"
#include "DBProRagdollManager.h"
#include "DBProRagDoll.h"
#include "DBPro.hpp"

DBProRagdollManager* ragdollManager = nullptr;// now in constructor new DBProRagdollManager();

//-------------------------------------

DBProRagdollManager::DBProRagdollManager()
{
}

DBProRagdollManager::~DBProRagdollManager()
{
}

void DBProRagdollManager::AddRagdoll(DBProRagDoll* ragdoll)
{
	AddItem(ragdoll);
}

void DBProRagdollManager::DeleteRagdoll(int ragdollID)
{
	DBProRagDoll* ragdoll = GetRagdoll(ragdollID);
	if(ragdoll)
	{
		ragdoll->ResetObjectParametersForAnimation();
		ragdoll->ResetObjectParametersForCulling();
		RemoveItem(ragdollID);
	}
}

DBProRagDoll* DBProRagdollManager::GetRagdoll(int ragdollID)
{
	return static_cast<DBProRagDoll*>(GetItem(ragdollID));
}

int DBProRagdollManager::GetIDFromBoneObject(int objectID)
{
	for(int i = 0; i < m_data.size(); i++)
	{
		DBProRagDoll* ragdoll = static_cast<DBProRagDoll*>(m_data[i]);
		if(ragdoll && ragdoll->IsBoneObject(objectID))
		{
			return ragdoll->GetID();
		}
	}
	return -1;
}

void DBProRagdollManager::Update()
{
	for(int i = 0; i < m_data.size(); i++)
	{	
		DBProRagDoll* ragdoll = static_cast<DBProRagDoll*>(m_data[i]);
		if(ragdoll)
		{
			if(!ragdoll->IsSleeping())
			{
				ragdoll->Update();
			}
			else
			{
				ragdoll->ResetObjectParametersForCulling();
			}
		}
	}
}

void DBProRagdollManager::AssertRagdollExist(int ragdollID, const char* message, bool bExist /*= true*/) 
{
	if(bExist && ragdollManager->GetRagdoll(ragdollID) == nullptr)
	{
		DBPro::ReportError(message, "Bullet Physics Wrapper");
	}
	else if(!bExist && ragdollManager->GetRagdoll(ragdollID) != nullptr)
	{
		DBPro::ReportError(message, "Bullet Physics Wrapper");
	}
}



