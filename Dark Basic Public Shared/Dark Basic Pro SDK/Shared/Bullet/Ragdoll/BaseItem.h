#pragma once
#include <compare>

class BaseItem
{
public:
	explicit BaseItem(int id = 0);
	virtual ~BaseItem() = default;

	int GetID() const noexcept;
	
	auto operator<=>(const BaseItem&) const = default;
	bool operator==(const BaseItem&) const = default;

	static bool sortfnc(const BaseItem* a, const BaseItem* b) noexcept
	{
		if (a && b) return a->GetID() < b->GetID();
		return a != nullptr;
	}

protected: 
	int m_id = 0;
};

