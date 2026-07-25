// ParserResultData.cpp: CResultData deep-copy semantics
//

#include "ParserHeader.h"
#include "Str.h"
#include "ParserResultData.h"

CResultData::CResultData(const CResultData& other)
	: m_dwType(other.m_dwType)
	, m_dwDataOffset(other.m_dwDataOffset)
	, m_pStruct(other.m_pStruct)
{
	if (other.m_pStringToken)
		m_pStringToken = std::make_unique<CStr>(other.m_pStringToken->GetStr());
	if (other.m_pAdditionalOffset)
		m_pAdditionalOffset = std::make_unique<CStr>(other.m_pAdditionalOffset->GetStr());
}

CResultData& CResultData::operator=(const CResultData& other)
{
	if (this != &other)
	{
		m_dwType = other.m_dwType;
		m_dwDataOffset = other.m_dwDataOffset;
		m_pStruct = other.m_pStruct;

		if (other.m_pStringToken)
			m_pStringToken = std::make_unique<CStr>(other.m_pStringToken->GetStr());
		else
			m_pStringToken.reset();

		if (other.m_pAdditionalOffset)
			m_pAdditionalOffset = std::make_unique<CStr>(other.m_pAdditionalOffset->GetStr());
		else
			m_pAdditionalOffset.reset();
	}
	return *this;
}
