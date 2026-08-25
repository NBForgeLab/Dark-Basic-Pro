///#include "StdAfx.h"
#include "BaseItemManager.h"

BaseItemManager::BaseItemManager()
{
}

BaseItemManager::~BaseItemManager()
{
	Dispose();
}

void BaseItemManager::Dispose()
{
	for(int i = 0; i < m_data.size(); i++)
	{
		delete m_data[i];
		m_data[i] = nullptr;
	}
	m_data.clear();
}

void BaseItemManager::AddItem(BaseItem* item)
{
	m_data.push_back(item);
	Sort();
}

bool BaseItemManager::RemoveItem(int id)
{
	Sort();
	int index = Find(id);
	if(index > -1 && index < m_data.size())
	{
		BaseItem* item = m_data[index];
		m_data.remove2(item);
		delete item;
		return true;
	}
	return false;
}

void BaseItemManager::Sort()
{
	m_data.quickSort(BaseItemSortPredicate());
}

int BaseItemManager::Find(int id)
{
	BaseItem temp(id);
	int index = m_data.findLinearSearch2(&temp); // lee - 300614 - cannot assume sorted array (ragdolls can be created in any order during combat)
	return index;
}

BaseItem* BaseItemManager::GetItem(int id)
{
	int index = Find(id);
	return (index > -1 && index < m_data.size()) ? m_data[index] : nullptr;
}

int BaseItemManager::GetFreeID()
{
	Sort();
	int index = 0;
	int id = -1;
	do
	{
		id++;
		index = Find(id);
	}while(index < m_data.size());
	return id;
}