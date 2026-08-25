#include "BaseItem.h"

BaseItem::BaseItem(int id) : m_id(id)
{
}

int BaseItem::GetID() const noexcept
{
	return m_id;
}